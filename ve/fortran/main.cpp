/*
This file is part of Bohrium and copyright (c) 2012 the Bohrium
team <http://www.bh107.org>.

Bohrium is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version.

Bohrium is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the
GNU Lesser General Public License along with Bohrium.

If not, see <http://www.gnu.org/licenses/>.
*/

#include <cassert>
#include <numeric>
#include <chrono>

#include <bh_component.hpp>
#include <bh_extmethod.hpp>
#include <bh_util.hpp>
#include <bh_opcode.h>
#include <jitk/fuser.hpp>
#include <jitk/block.hpp>
//#include <jitk/instruction.hpp>
#include <jitk/graph.hpp>
#include <jitk/transformer.hpp>
#include <jitk/fuser_cache.hpp>
#include <jitk/codegen_util.hpp>
#include <jitk/statistics.hpp>
#include <jitk/dtype.hpp>
#include <jitk/apply_fusion.hpp>

#include "engine_fortran.hpp"
#include "fortran_writer.hpp"

using namespace bohrium;
using namespace jitk;
using namespace component;
using namespace std;

namespace {
class Impl : public ComponentImpl {
  private:
    // Some statistics
    Statistics stat;
    // Fuse cache
    FuseCache fcache;
    // Teh OpenMP engine
    EngineFortran engine;
    // Known extension methods
    map<bh_opcode, extmethod::ExtmethodFace> extmethods;
    //Allocated base arrays
    set<bh_base*> _allocated_bases;

  public:
    Impl(int stack_level) : ComponentImpl(stack_level),
                            stat(config.defaultGet("prof", false)),
                            fcache(stat), engine(config, stat) {}
    ~Impl();
    void execute(bh_ir *bhir);
    void extmethod(const string &name, bh_opcode opcode) {
        // ExtmethodFace does not have a default or copy constructor thus
        // we have to use its move constructor.
        extmethods.insert(make_pair(opcode, extmethod::ExtmethodFace(config, name)));
    }

    // Implement the handle of extension methods
    void handle_extmethod(bh_ir *bhir) {
        util_handle_extmethod(this, bhir, extmethods);
    }

    // The following methods implements the methods required by jitk::handle_gpu_execution()

    // Write the OpenMP kernel
    void write_kernel(const vector<Block> &block_list, const SymbolTable &symbols, const ConfigParser &config,
                      const vector<bh_base*> &kernel_temps, stringstream &ss);

    // Handle messages from parent
    string message(const string &msg) {
        stringstream ss;
        if (msg == "statistic_enable_and_reset") {
            stat = Statistics(true, config.defaultGet("prof", false));
        } else if (msg == "statistic") {
            stat.write("OpenMP", "", ss);
            return ss.str();
        } else if (msg == "info") {
            ss << engine.info();
        }
        return ss.str();
    }

    // Handle memory pointer retrieval
    void* get_mem_ptr(bh_base &base, bool copy2host, bool force_alloc, bool nullify) {
        if (not copy2host) {
            throw runtime_error("OpenMP - get_mem_ptr(): `copy2host` is not True");
        }
        if (force_alloc) {
            bh_data_malloc(&base);
        }
        void *ret = base.data;
        if (nullify) {
            base.data = NULL;
        }
        return ret;
    }

    // Handle memory pointer obtainment
    void set_mem_ptr(bh_base *base, bool host_ptr, void *mem) {
        if (not host_ptr) {
            throw runtime_error("OpenMP - set_mem_ptr(): `host_ptr` is not True");
        }
        if (base->data != nullptr) {
            throw runtime_error("OpenMP - set_mem_ptr(): `base->data` is not NULL");
        }
        base->data = mem;
    }

    // We have no context so returning NULL
    void* get_device_context() {
        return nullptr;
    };

    // We have no context so doing nothing
    void set_device_context(void* device_context) {};
};
}

extern "C" ComponentImpl* create(int stack_level) {
    return new Impl(stack_level);
}
extern "C" void destroy(ComponentImpl* self) {
    delete self;
}

Impl::~Impl() {
    if (stat.print_on_exit) {
        stat.write("OpenMP", config.defaultGet<std::string>("prof_filename", ""), cout);
    }
}

void Impl::write_kernel(const vector<Block> &block_list, const SymbolTable &symbols, const ConfigParser &config,
                        const vector<bh_base*> &kernel_temps, stringstream &ss) {

    // Write the header of the launcher subroutine
    ss << "SUBROUTINE launcher(data_list, offset_strides, constants) BIND(C)" << endl;

    // Include convert_pointer, which unpacks the c pointers for fortran use
    spaces(ss, 4);
    ss << "USE iso_c_binding" << endl;
    spaces(ss, 4);
    ss << "INTERFACE" << endl;
    spaces(ss, 8);
    ss << "FUNCTION convert_pointer(a,b) RESULT(res) BIND(C, name=\"convert_pointer\")" << endl;
    spaces(ss, 12);
    ss << "USE iso_c_binding" << endl;
    spaces(ss, 12);
    ss << "TYPE(c_ptr) :: a" << endl;
    spaces(ss, 12);
    ss << "integer :: b" << endl;
    spaces(ss, 12);
    ss << "TYPE(c_ptr) :: res" << endl;
    spaces(ss, 8);
    ss << "END FUNCTION" << endl;
    spaces(ss, 4);
    ss << "END INTERFACE" << endl;
    spaces(ss, 4);
    ss << "TYPE(c_ptr) :: data_list" << endl << endl;
    if (symbols.useRandom()) { // Write the random function
        spaces(ss, 4);
        ss << "INTERFACE" << endl;
        spaces(ss, 8);
        ss << "FUNCTION random123(start, key, index) RESULT(res) BIND(C, name=\"random123\")" << endl;
        spaces(ss, 12);
        ss << "USE iso_c_binding" << endl;
        spaces(ss, 12);
        ss << "INTEGER :: start" << endl;
        spaces(ss, 12);
        ss << "INTEGER :: key" << endl;
        spaces(ss, 12);
        ss << "INTEGER :: index" << endl;
        spaces(ss, 12);
        ss << "INTEGER*16 :: res" << endl;
        spaces(ss, 8);
        ss << "END FUNCTION" << endl;
        spaces(ss, 4);
        ss << "END INTERFACE" << endl;
    }

    FortranWriter* writer = new FortranWriter();
    stringstream body;
    {
        // For each declares input, declare a fortran pointer and a c pointer
        for(size_t i=0; i < symbols.getParams().size(); ++i) {
            bh_base *b = symbols.getParams()[i];
            spaces(ss, 4);
            ss << write_fortran_type(b->type) << ", POINTER, DIMENSION (:) :: a" << symbols.baseID(b) << "\n";
            spaces(ss, 4);
            ss << "TYPE(c_ptr) :: c" << symbols.baseID(b) << "\n";
        }

        for(const Block &block: block_list) {
            write_loop_block(symbols, nullptr, block.getLoop(), config, {}, false, true, write_fortran_type,
                             writer);

            const LoopB &loopBlock = block.getLoop();
            vector<const bh_view*> scalar_replaced_input_only;
            vector<const bh_view*> scalar_replaced_reduction_outputs;
            Scope scope(symbols, nullptr, loopBlock.getLocalTemps(), scalar_replaced_reduction_outputs,
                        scalar_replaced_input_only, config);
        }

    }
    ss << writer->declarations.str();

    ss << "\n";
    // Write convert statements for the c pointers
    for(size_t i=0; i < symbols.getParams().size(); ++i) {
        bh_base *b = symbols.getParams()[i];
        spaces(ss, 4);
        ss << "c" << symbols.baseID(b) << "= CONVERT_POINTER(" << "data_list, " << i << ")" << "\n";
        spaces(ss, 4);
        ss << "CALL c_f_pointer(c" << symbols.baseID(b) << ", a" << symbols.baseID(b) << ", shape=[" << b->nelem << "])\n";
    }
    ss << "\n";

    ss << writer->ss.str();

    // End the subroutine
    ss << "END SUBROUTINE LAUNCHER\n\n";
}

void Impl::execute(bh_ir *bhir) {
    // Let's handle extension methods
    util_handle_extmethod(this, bhir, extmethods);

    // And then the regular instructions
    handle_cpu_execution(*this, bhir, engine, config, stat, fcache);
}
