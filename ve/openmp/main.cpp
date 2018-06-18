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
#include <jitk/engines/dyn_view.hpp>

#include "engine_openmp.hpp"

using namespace bohrium;
using namespace jitk;
using namespace component;
using namespace std;

namespace {
class Impl : public ComponentVE {
  private:
    //Allocated base arrays
    set<bh_base*> _allocated_bases;

  public:
    // Some statistics
    Statistics stat;
    // The OpenMP engine
    EngineOpenMP engine;

    Impl(int stack_level) : ComponentVE(stack_level),
                            stat(config),
                            engine(*this, stat) {}
    ~Impl() override;
    void execute(BhIR *bhir) override;
    void extmethod(const string &name, bh_opcode opcode) override {
        // ExtmethodFace does not have a default or copy constructor thus
        // we have to use its move constructor.
        extmethods.insert(make_pair(opcode, extmethod::ExtmethodFace(config, name)));
    }

    // Handle messages from parent
    string message(const string &msg) override {
        stringstream ss;
        if (msg == "statistic_enable_and_reset") {
            stat = Statistics(true, config);
        } else if (msg == "statistic") {
            engine.updateFinalStatistics();
            stat.write("OpenMP", "", ss);
            return ss.str();
        } else if (msg == "info") {
            ss << engine.info();
        }
        return ss.str();
    }

    // Handle memory pointer retrieval
    void* getMemoryPointer(bh_base &base, bool copy2host, bool force_alloc, bool nullify) override {
        if (not copy2host) {
            throw runtime_error("OpenMP - getMemoryPointer(): `copy2host` is not True");
        }
        if (force_alloc) {
            bh_data_malloc(&base);
        }
        void *ret = base.data;
        if (nullify) {
            base.data = nullptr;
        }
        return ret;
    }

    // Handle memory pointer obtainment
    void setMemoryPointer(bh_base *base, bool host_ptr, void *mem) override {
        if (not host_ptr) {
            throw runtime_error("OpenMP - setMemoryPointer(): `host_ptr` is not True");
        }
        if (base->data != nullptr) {
            throw runtime_error("OpenMP - setMemoryPointer(): `base->data` is not NULL");
        }
        base->data = mem;
    }

    // We have no context so returning NULL
    void* getDeviceContext() override {
        return nullptr;
    };

    // We have no context so doing nothing
    void setDeviceContext(void* device_context) override {};
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
        engine.updateFinalStatistics();
        stat.write("OpenMP", config.defaultGet<std::string>("prof_filename", ""), cout);
    }
}

void Impl::execute(BhIR *bhir) {
    //    printf("Goes on for %d iterations \n", bhir->getNRepeats());
    //    printf("EXECUTE\n");
    bh_base *cond = bhir->getRepeatCondition();
    bool has_pointer = uses_array_iterators(bhir);

    BhIR *setup_bhir = NULL;
    //    if (has_pointer) {
    //        std::vector<BhIR> bhirs = two_phase_bhirs(bhir);
    //        setup_bhir = &bhirs.at(0);
    //        bhir = &bhirs.at(1);
    //    }

    if (has_pointer) {
        //        printf("Starting new loop with pointer. Goes on for %d iterations \n", bhir->getNRepeats());
        BhIR new_bhir = delay_iterator_array_frees(bhir);
        std::vector<BhIR> bhirs = bhir_splitter(&new_bhir);
        uint64_t bhirs_size = bhirs.size();
        BhIR cur_bhir = bhirs.at(0);
        for (uint64_t i = 0; i < bhir->getNRepeats(); ++i) {
            //            printf("Starting new iteration\n");
            for (uint64_t j = 0; j < bhirs_size; j++) {
                update_array_iterators(&cur_bhir);
                // Let's handle extension methods
                engine.handleExtmethod(&cur_bhir);

                // And then the regular instructions
                engine.handleExecution(&cur_bhir);

                cur_bhir = bhirs.at((j+1)%bhirs_size);
            }
            // Check condition
            if (cond != nullptr and cond->data != nullptr and not ((bool*) cond->data)[0]) {
                break;
            }
        }
    } else {
        for (uint64_t i = 0; i < bhir->getNRepeats(); ++i) {
            // Let's handle extension methods
            engine.handleExtmethod(bhir);

            //            printf("lala\n");
            // And then the regular instructions
            engine.handleExecution(bhir);


            // Check condition
            if (cond != nullptr and cond->data != nullptr and not ((bool*) cond->data)[0]) {
                break;
            }

            //            auto tslide = chrono::steady_clock::now();
            // Change views that slide between iterations
            slide_views(bhir);
            //            stat.time_upd_iter += chrono::steady_clock::now() - tslide;
        }
    }
}
