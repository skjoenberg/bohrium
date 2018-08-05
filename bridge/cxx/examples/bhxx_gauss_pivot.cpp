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
#include <iostream>

#include <bhxx/bhxx.hpp>

using namespace bhxx;

int main()
{
    // Shape of input matrix
    long int N = 500;
    Shape shape = {N,N};
    BhArray<float> a(shape, contiguous_stride(shape));

    // Fill the input matrix with random numbers
    BhArray<uint64_t> b(shape, {(long int)shape.at(0), 1});
    random(b, 1, 1);
    identity(a,b);
    free(b);
    divide(a,a,std::numeric_limits<uint64_t>::max());
    multiply(a,a,100);

    // Define the size the dimension which is iterated over
    size_t size = a.shape[0];
    Runtime::instance().flush();


    // Loop and loop body
    for (size_t c = 1; c < size ; c++) {
        // Python syntax:
        // a[c:, c - 1:] -= (a[c:, c - 1] / a[c - 1, c - 1:c])[:, None] * a[c - 1, c - 1:]
        // This is explicitly turned into:
        // a1 -= (a2 / a3) * a4
        // a5 = (a2 / a3)
        // a6 = a5 * a4
        // a1 = a1 - a6

        // Get the shape of the view into array a (shrinks every iteration)
        Shape shape = {size-c, size-c+1};

        // a1: a[c:, c - 1:]
        BhArray<float> a1 = BhArray<float>(a.base,
                     shape,
                     a.stride,
                     (c * a.stride.at(0)) + (c-1) * a.stride.at(1));
        // a2: a[c:, c - 1]
        BhArray<float> a2 = BhArray<float>(a.base,
                     shape,
                     {a.stride.at(0), 0},
                     (c * a.stride.at(0)) + (c-1) * a.stride.at(1));
        // a3: a[c - 1, c - 1:c]
        BhArray<float> a3 = BhArray<float>(a.base,
                     shape,
                     {0, 0},
                     ((c-1) * a.stride.at(0)) + (c-1) * a.stride.at(1));
        // a4: a[c - 1, c - 1:]
        BhArray<float> a4 = BhArray<float>(a.base,
                     shape,
                     {0, a.stride.at(1)},
                     ((c-1) * a.stride.at(0)) + (c-1) * a.stride.at(1));

        // Temporary arrays
        BhArray<float> a5(shape,  a.stride);
        BhArray<float> a6(shape, contiguous_stride(shape));

        // Perform the array operations
        divide(a5,a2,a3);
        multiply(a6,a5,a4);
        subtract(a1,a1,a6);

        // Free the views
        free(a1);
        free(a2);
        free(a3);
        free(a4);
        free(a5);
        free(a6);

        // Flush
        Runtime::instance().flush();
    }

    // Devide by the diagonal
    BhArray<float> diag = BhArray<float>(a.base,
        a.shape,
        {a.stride.at(0) + a.stride.at(1), 0}, 0);
    BhArray<float> a7(a.shape,  a.stride);
    divide(a7, a, diag);

    Runtime::instance().flush();

    // Use the result to force the calculation
    (void)a7;
    return 0;
}
