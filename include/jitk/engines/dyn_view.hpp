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

void slide_views(BhIR *bhir) {
    // Iterate through all instructions and slide the relevant views
    for (bh_instruction &instr : bhir->instr_list) {
        for (bh_view &view : instr.operand) {
            if (not view.slide.empty()) {
                // The relevant dimension in the view is updated by the given stride
                for (size_t i = 0; i < view.slide.size(); i++) {
                    int dim = view.slide_dim.at(i);
                    int dim_stride = view.slide_dim_stride.at(i);

                    int change = view.slide.at(i)*dim_stride;
                    int max_rel_idx = dim_stride*view.slide_dim_shape.at(i);
                    int rel_idx = view.start % (dim_stride*view.slide_dim_shape.at(i));

                    rel_idx += change;

                    if (rel_idx < 0) {
                        change += max_rel_idx;
                    } else if (rel_idx >= max_rel_idx) {
                        change -= max_rel_idx;
                    }

                    view.start += (int64_t) change;

                    view.shape[dim] += (int64_t) view.slide_dim_shape_change.at(i);
                }
            }
        }
    }
}

BhIR delay_iterator_array_frees(BhIR *bhir) {
    std::unordered_set<bh_base*> index_bases;
    for (bh_instruction &instr : bhir->instr_list) {
        for (bh_view &view : instr.operand) {
            if (view.start_pointer != nullptr) {
                index_bases.insert(view.start_pointer);
            }
            if (view.shape_pointer != nullptr) {
                index_bases.insert(view.shape_pointer);
            }
        }
    }

    std::unordered_map<bh_base*, bh_instruction> frees;
    BhIR new_bhir(std::vector<bh_instruction>(), bhir->_syncs, bhir->getNRepeats(), bhir->getRepeatCondition());
    for (bh_instruction &instr : bhir->instr_list) {
        if (instr.opcode == 55) {
            bh_base *free_base = instr.operand[0].base;
            if (index_bases.count(free_base) > 0) {
                frees.insert({free_base, instr});
                continue;
            }
        }
        new_bhir.instr_list.push_back(instr);
    }

    for (int i = new_bhir.instr_list.size()-1; i >= 0; i--) {
        bh_instruction instr = new_bhir.instr_list[i];
        for (bh_view &view : instr.operand) {
            if (view.start_pointer != nullptr) {
                bh_base* view_base = view.start_pointer;
                auto search = frees.find(view_base);
                if (search != frees.end()) {
                    bh_instruction free = search->second;
                    new_bhir.instr_list.insert(new_bhir.instr_list.begin()+i+1, free);
                    frees.erase(view_base);
                }
            }

            if (view.shape_pointer != nullptr) {
                bh_base* view_base = view.shape_pointer;
                auto search = frees.find(view_base);
                if (search != frees.end()) {
                    bh_instruction free = search->second;
                    new_bhir.instr_list.insert(new_bhir.instr_list.begin()+i+1, free);
                    frees.erase(view_base);
                }
            }
        }
    }

    return new_bhir;
}

bool uses_array_iterators(BhIR *bhir) {
    bool has_pointer = false;
    for (bh_instruction &instr : bhir->instr_list) {
        for (bh_view &view : instr.operand) {
            if (view.uses_pointer()) {
                has_pointer = true;
                break;
            }
        }
    }
    return has_pointer;
}


BhIR* two_phase_bhirs(BhIR *bhir) {
    BhIR* resulting_bhirs;

    BhIR setup_bhir(std::vector<bh_instruction>(), bhir->_syncs, 0, nullptr);
    BhIR execution_bhir(std::vector<bh_instruction>(), bhir->_syncs, 0, bhir->getRepeatCondition());

    bool has_pointer = false;
    for (bh_instruction &instr : bhir->instr_list) {
        if (has_pointer) {
            execution_bhir.instr_list.push_back(instr);
        } else {
            for (bh_view &view : instr.operand) {
                if (view.uses_pointer()) {
                    has_pointer = true;
                    break;
                }
            }
            if (not has_pointer) {
                setup_bhir.instr_list.push_back(instr);
            } else {
                execution_bhir.instr_list.push_back(instr);
            }
        }
    }

    resulting_bhirs[0] = setup_bhir;
    resulting_bhirs[1] = execution_bhir;
    return resulting_bhirs;
}

void update_array_iterators(BhIR *execution_bhir) {
    for (bh_instruction &instr : execution_bhir->instr_list) {
        for (bh_view &view : instr.operand) {
            if (not (view.start_pointer == nullptr)) {
                view.start = *((int64_t*) view.start_pointer->data);
            }
            if (not (view.shape_pointer == nullptr)) {
                int64_t* shapes = (int64_t*) view.shape_pointer->data;
                for (int i=0; i < view.ndim; i++) {
                    view.shape[i] = shapes[i];
                }
            }
            if (not (view.stride_pointer == nullptr)) {
                int64_t* strides = (int64_t*) view.stride_pointer->data;
                for (int i=0; i < view.ndim; i++) {
                    view.stride[i] = strides[i];
                }
            }
        }
    }
}
