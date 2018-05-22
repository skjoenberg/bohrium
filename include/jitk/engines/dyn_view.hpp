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
        //        printf("op %d\n", instr.constructor);

        for (bh_view &view : instr.operand) {
            //            printf("start: %d\n", view.start);
            //            printf("ndim %d\n", view.ndim);
            //            printf("shape: ");
            //            for (int64_t i=0; i < view.ndim; ++i) {
            //                printf("%d: %d, ", i, view.shape[i]);
            //            }
            //            printf(".\n");

            if (not view.slide.empty()) {
                //                printf("....new view\n");
                //                printf("base: %d\n", view.base);
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

                    //printf("change: %d\n", change);
                    //                    printf("change: %d\n", change);
                    //                    printf("new start: %d\n", view.start);
                    //                    printf("shape %d\n", view.shape[dim]);
                }
            }
        }
    }
}
