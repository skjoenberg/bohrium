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
#include <jitk/instruction.hpp>
#include <jitk/graph.hpp>
#include <jitk/transformer.hpp>
#include <jitk/fuser_cache.hpp>
#include <jitk/codegen_util.hpp>
#include <jitk/statistics.hpp>
#include <jitk/dtype.hpp>
#include <jitk/apply_fusion.hpp>

#include "engine_openmp.hpp"

#include "openmp_writer.hpp"

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
    EngineOpenMP engine;
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

    // Write the need includes
    ss << "#include <stdint.h>\n";
    ss << "#include <stdlib.h>\n";
    ss << "#include <stdbool.h>\n";
    ss << "#include <complex.h>\n";
    ss << "#include <tgmath.h>\n";
    ss << "#include <math.h>\n";
    if (symbols.useRandom()) { // Write the random function
        ss << "#include <kernel_dependencies/random123_openmp.h>\n";
    }
    write_c99_dtype_union(ss); // We always need to declare the union of all constant data types
    ss << "\n";

    // Write the header of the execute function
    ss << "void execute";
    write_kernel_function_arguments(symbols, write_c99_type, ss, nullptr, false);

    // Write the block that makes up the body of 'execute()'
    ss << "{\n";
    // Write allocations of the kernel temporaries
    for(const bh_base* b: kernel_temps) {
        spaces(ss, 4);
        ss << write_c99_type(b->type) << " * __restrict__ a" << symbols.baseID(b) << " = malloc(" << bh_base_size(b)
           << ");\n";
    }
    ss << "\n";

    OpenMPWriter* writer = new OpenMPWriter();
    for(const Block &block: block_list) {
        write_loop_block(symbols, nullptr, block.getLoop(), config, {}, false, false, write_c99_type,
                         writer);
    }
    ss << writer->ss.str();

    // Write frees of the kernel temporaries
    ss << "\n";
    for(const bh_base* b: kernel_temps) {
        spaces(ss, 4);
        ss << "free(" << "a" << symbols.baseID(b) << ");\n";
    }
    ss << "}\n\n";

    // Write the launcher function, which will convert the data_list of void pointers
    // to typed arrays and call the execute function
    {
        ss << "void launcher(void* data_list[], uint64_t offset_strides[], union dtype constants[]) {\n";
        for(size_t i=0; i < symbols.getParams().size(); ++i) {
            spaces(ss, 4);
            bh_base *b = symbols.getParams()[i];
            ss << write_c99_type(b->type) << " *a" << symbols.baseID(b);
            ss << " = data_list[" << i << "];\n";
        }
        spaces(ss, 4);
        ss << "execute(";
        // We create the comma separated list of args and saves it in `stmp`
        stringstream stmp;
        for(size_t i=0; i < symbols.getParams().size(); ++i) {
            bh_base *b = symbols.getParams()[i];
            stmp << "a" << symbols.baseID(b) << ", ";
        }
        uint64_t count=0;
        for (const bh_view *view: symbols.offsetStrideViews()) {
            stmp << "offset_strides[" << count++ << "], ";
            for (int i=0; i<view->ndim; ++i) {
                stmp << "offset_strides[" << count++ << "], ";
            }
        }
        if (symbols.constIDs().size() > 0) {
            uint64_t i=0;
            for (auto it = symbols.constIDs().begin(); it != symbols.constIDs().end(); ++it) {
                const InstrPtr &instr = *it;
                stmp << "constants[" << i++ << "]." << bh_type_text(instr->constant.type) << ", ";
            }
        }
        // And then we write `stmp` into `ss` excluding the last comma
        const string strtmp = stmp.str();
        if (not strtmp.empty()) {
            ss << strtmp.substr(0, strtmp.size()-2);
        }
        ss << ");\n";
        ss << "}\n";
    }
}

void Impl::execute(bh_ir *bhir) {
    // Let's handle extension methods
    util_handle_extmethod(this, bhir, extmethods);

    // And then the regular instructions
    handle_cpu_execution(*this, bhir, engine, config, stat, fcache);
}
