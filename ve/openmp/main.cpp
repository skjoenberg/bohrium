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
class Impl : public ComponentImpl {
  private:
    //Allocated base arrays
    set<bh_base*> _allocated_bases;

  public:
    // Some statistics
    Statistics stat;
    // The OpenMP engine
    EngineOpenMP engine;
    // Known extension methods
    map<bh_opcode, extmethod::ExtmethodFace> extmethods;

    Impl(int stack_level) : ComponentImpl(stack_level),
                            stat(config),
                            engine(config, stat) {}
    ~Impl();
    void execute(BhIR *bhir);
    void extmethod(const string &name, bh_opcode opcode) {
        // ExtmethodFace does not have a default or copy constructor thus
        // we have to use its move constructor.
        extmethods.insert(make_pair(opcode, extmethod::ExtmethodFace(config, name)));
    }

    // Handle messages from parent
    string message(const string &msg) {
        stringstream ss;
        if (msg == "statistic_enable_and_reset") {
            stat = Statistics(true, config);
        } else if (msg == "statistic") {
            stat.write("OpenMP", "", ss);
            return ss.str();
        } else if (msg == "info") {
            ss << engine.info();
        }
        return ss.str();
    }

    // Handle memory pointer retrieval
    void* getMemoryPointer(bh_base &base, bool copy2host, bool force_alloc, bool nullify) {
        if (not copy2host) {
            throw runtime_error("OpenMP - getMemoryPointer(): `copy2host` is not True");
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
    void setMemoryPointer(bh_base *base, bool host_ptr, void *mem) {
        if (not host_ptr) {
            throw runtime_error("OpenMP - setMemoryPointer(): `host_ptr` is not True");
        }
        if (base->data != nullptr) {
            throw runtime_error("OpenMP - setMemoryPointer(): `base->data` is not NULL");
        }
        base->data = mem;
    }

    // We have no context so returning NULL
    void* getDeviceContext() {
        return nullptr;
    };

    // We have no context so doing nothing
    void setDeviceContext(void* device_context) {};
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

void Impl::execute(BhIR *bhir) {
    bh_base *cond = bhir->getRepeatCondition();

    // //    BhIR setup_bhir(std::vector<bh_instruction>(), std::set<bh_base *>(), 0, nullptr);
    // BhIR setup_bhir(std::vector<bh_instruction>(), bhir->_syncs, 0, nullptr);
    // BhIR execution_bhir(std::vector<bh_instruction>(), bhir->_syncs, 0, cond);

    // std::unordered_set<bh_base*> index_bases; // = std::unorderepd_set<bh_base*>();

    // for (bh_instruction &instr : bhir->instr_list) {
    //     for (bh_view &view : instr.operand) {
    //         if (view.start_pointer != nullptr) {
    //             index_bases.insert(view.start_pointer);
    //         }
    //         if (view.shape_pointer != nullptr) {
    //             index_bases.insert(view.shape_pointer);
    //         }
    //     }
    // }

    // //    printf("index bases: %d\n", index_bases.size());

    // std::unordered_map<bh_base*, bh_instruction> frees;
    // BhIR new_bhir(std::vector<bh_instruction>(), bhir->_syncs, bhir->getNRepeats(), cond);
    // for (bh_instruction &instr : bhir->instr_list) {
    //     if (instr.opcode == 55) {
    //         bh_base *free_base = instr.operand[0].base;
    //         if (index_bases.count(free_base) > 0) {
    //             frees.insert({free_base, instr});
    //             continue;
    //         }
    //     }
    //     new_bhir.instr_list.push_back(instr);
    // }

    // for (int i = new_bhir.instr_list.size()-1; i >= 0; i--) {
    //     bh_instruction instr = new_bhir.instr_list[i];
    //     for (bh_view &view : instr.operand) {
    //         if (view.start_pointer != nullptr) {
    //             bh_base* view_base = view.start_pointer;
    //             auto search = frees.find(view_base);
    //             if (search != frees.end()) {
    //                 bh_instruction free = search->second;
    //                 new_bhir.instr_list.insert(new_bhir.instr_list.begin()+i+1, free);
    //                 printf("inserted at %d\n", i+1);
    //                 frees.erase(view_base);
    //             }
    //         }

    //         if (view.shape_pointer != nullptr) {
    //             bh_base* view_base = view.shape_pointer;
    //             auto search = frees.find(view_base);
    //             if (search != frees.end()) {
    //                 bh_instruction free = search->second;
    //                 new_bhir.instr_list.insert(new_bhir.instr_list.begin()+i+1, free);
    //                 printf("inserted at %d\n", i+1);
    //                 frees.erase(view_base);
    //             }
    //         }
    //     }
    // }

    // //    printf("Lengths: orig %d, new %d\n", bhir->instr_list.size(), new_bhir.instr_list.size());

    // for (int i = 0; i < bhir->instr_list.size(); i++) {
    //     int orig_op = bhir->instr_list[i].opcode;
    //     int new_op = new_bhir.instr_list[i].opcode;
    //     if (orig_op != new_op) {
    //         printf("%d. orig opcode: %d, new opcode: %d\n", i, orig_op, new_op);
    //     }
    // }

    // bhir = &new_bhir;

    // bool has_pointer = false;
    // for (bh_instruction &instr : bhir->instr_list) {
    //     //        printf("opcode: %d\n", instr.opcode);
    //     if (has_pointer) {
    //         execution_bhir.instr_list.push_back(instr);
    //     } else {
    //         for (bh_view &view : instr.operand) {
    //             if (view.uses_pointer()) {
    //                 printf("Pointer with adress: %d\n", view.start_pointer);
    //                 has_pointer = true;
    //                 break;
    //             }
    //         }
    //         if (not has_pointer) {
    //             setup_bhir.instr_list.push_back(instr);
    //         } else {
    //             execution_bhir.instr_list.push_back(instr);
    //         }
    //     }
    // }

    // if (!has_pointer) {
    //     execution_bhir = setup_bhir;
    // }

    for (uint64_t i = 0; i < bhir->getNRepeats(); ++i) {
        // if (has_pointer) {
        //     printf("!!!!!!!!!! Burde ikke ske\n");
        //     // Let's handle extension methods
        //     engine.handleExtmethod(*this, &setup_bhir);

        //     // And then the regular instructions
        //     engine.handleExecution(&setup_bhir);

        //     printf("length setup bhir %d\n", setup_bhir.instr_list.size());

        //     for (bh_instruction &instr : execution_bhir.instr_list) {
        //         for (bh_view &view : instr.operand) {
        //             if (not (view.start_pointer == nullptr)) {
        //                 printf("Updating view to value at adress: %d\n", view.start_pointer);
        //                 printf("Value is: %d\n", *((int64_t*) view.start_pointer->data));
        //                 view.start = *((int64_t*) view.start_pointer->data);
        //                 printf("New start is: %d\n", view.start);
        //             }
        //             if (not (view.shape_pointer == nullptr)) {
        //                 printf("HEEEEY\n");
        //                 int64_t* shapes = (int64_t*) view.shape_pointer->data;
        //                 for (int i=0; i < view.ndim; i++) {
        //                     view.shape[i] = shapes[i];
        //                     printf("hejsa\n");
        //                     printf("shape %d: %d\n", i, view.shape[i]);
        //                 }
        //             }
        //             if (not (view.stride_pointer == nullptr)) {
        //                 int64_t* strides = (int64_t*) view.stride_pointer->data;
        //                 for (int i=0; i < view.ndim; i++) {
        //                     view.stride[i] = strides[i];
        //                 }
        //             }
        //         }
        //     }
        // }

        // // Let's handle extension methods
        // engine.handleExtmethod(*this, &execution_bhir);

        // // And then the regular instructions
        // engine.handleExecution(&execution_bhir);

        // // Check condition
        // if (cond != nullptr and cond->data != nullptr and not ((bool*) cond->data)[0]) {
        //     break;
        // }

        // // Change views that slide between iterations
        // slide_views(&execution_bhir);


        //        printf("Executing ext\n");
        // Let's handle extension methods
        engine.handleExtmethod(*this, bhir);

        //        printf("Executing kernel\n");
        // And then the regular instructions
        engine.handleExecution(bhir);
        //        printf("Done executing kernel\n");

        // Check condition
        if (cond != nullptr and cond->data != nullptr and not ((bool*) cond->data)[0]) {
            break;
        }

        // Change views that slide between iterations
        slide_views(bhir);

    }
}
