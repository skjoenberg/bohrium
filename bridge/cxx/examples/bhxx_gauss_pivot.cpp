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

// void pivot_gauss()
// {
//     BhArray<float> a({2,3,4}, {12,4,1});
//     BhArray<float> b({2,3,4}, {12,4,1});
//     BhArray<float> c({2,3,4}, {12,4,1});
//     identity(b, 1);
//     identity(c, 2);

//     argmax(a);

//     add(a, b, c);
//     std::cout << a << std::endl;

//     add(b, a, -10.0);
//     std::cout << b << std::endl;

//     BhArray<uint64_t> r({10});
//     random(r, 42, 42);
//     std::cout << r << std::endl;

//     Runtime::instance().flush();
// }

// BhArray<float> gauss(BhArray<float> a) {
//     size_t size = a.shape[0];
//     for (size_t c = 1; c < size ; c++) {
//         Shape shape = {size-c, size-c+1};
//         BhArray<float> a1 = BhArray<float>(a.base,
//                                            shape,
//                                            a.stride,
//                                            (c * a.stride.at(0)) + (c-1) * a.stride.at(1));
//         BhArray<float> a2 = BhArray<float>(a.base,
//                                            shape,
//                                            {a.stride.at(0), 0},
//                                            (c * a.stride.at(0)) + (c-1) * a.stride.at(1));

//         BhArray<float> a3 = BhArray<float>(a.base,
//                                            shape,
//                                            {0, 0},
//                                            ((c-1) * a.stride.at(0)) + (c-1) * a.stride.at(1));
//         BhArray<float> a4(shape,  a.stride);
//         divide(a4,a2,a3);
//         BhArray<float> a5 = BhArray<float>(a.base,
//                                            shape,
//                                            {0, a.stride.at(1)},
//                                            ((c-1) * a.stride.at(0)) + (c-1) * a.stride.at(1));
//         BhArray<float> a6(shape,  a.stride);
//         multiply(a6,a4,a5);

// //        BhArray<float> a7(shape,  a.stride);
//         //        subtract(a7,a2,a2);
//         //        subtract(a7,a1,a6);
//         subtract(a1,a1,a6);

//         //        identity(a1,a7);
//         free(a1);
//         free(a2);
//         free(a3);
//         free(a4);
//         free(a5);
//         free(a6);
// //        free(a7);
//         Runtime::instance().flush();
//         //        printf("heeyyyy %d\n", c);
//         //        printf("off %d\n", (c * a.stride.at(0)) + (c-1) * a.stride.at(1));
//     }
//     printf("hehehehehhe\n");
//     BhArray<float> diag = BhArray<float>(a.base,
//         a.shape,
//         {a.stride.at(0) + a.stride.at(1), 0}, 0);

//     BhArray<float> a7(a.shape,  a.stride);
//     divide(a7, a, diag);

//     return a7;
// }


int main()
{
    //        BhArray<float> a({4,4}, {4,1});
    //    BhArray<float> a({2,3,4,5}, {12,4,1,6}, {34,3,5,9}, {34,3,5,9});
    //    BhArray<float> a1;

    //    identity(a, 1);
    //    random(a, 1);
    //    float* data = bhxx::Runtime::getMemoryPointer(a.base, true, true, false);
    //    Runtime runtime = bhxx::Runtime::instance();

    //    float data[16] = {1.0,2.0,3.0,4.0,5.0,6.0,7.0,8.0,9.0,10.0,11.0,12.0,13.0,14.0,15.0,16.0};
    //    float* array_data = (float*) Runtime::instance().getMemoryPointer(a.base, true, true, false);
    //    memcpy(array_data, &data, 16*sizeof(float));

    Runtime::instance().flush();
    std::cout << "what:\n" << std::endl;
    Shape shape = {200,200};
    //    BhArray<float> a(shape, contiguous_stride(shape));
    BhArray<float> a(shape, {200,1});
    //    BhArray<uint64_t> b(shape, contiguous_stride(shape));
    BhArray<uint64_t> b(shape, {200,1});
    random(b, 1, 1);
    //    identity(a,b);
    //    free(b);
    //    divide(a,a,std::numeric_limits<uint64_t>::max());
    //    multiply(a,a,100);
    size_t size = a.shape[0];
    Runtime::instance().flush();
    std::cout << "strides:\n" << a.stride << std::endl;
    for (size_t c = 1; c < size ; c++) {
        Shape shape = {size-c, size-c+1};
        std::cout << "shape:\n" << shape << std::endl;
        std::cout << "off:\n" << (c * a.stride.at(0)) + (c-1) * a.stride.at(1) << std::endl;

        BhArray<float> a1 = BhArray<float>(a.base,
                     shape,
                     a.stride,
                     (c * a.stride.at(0)) + (c-1) * a.stride.at(1));
        BhArray<float> a2 = BhArray<float>(a.base,
                     shape,
                     {a.stride.at(0), 0},
                     (c * a.stride.at(0)) + (c-1) * a.stride.at(1));
        BhArray<float> a3 = BhArray<float>(a.base,
                     shape,
                     {0, 0},
                     ((c-1) * a.stride.at(0)) + (c-1) * a.stride.at(1));
        BhArray<float> a4(shape,  a.stride);
        //        BhArray<float> a4(shape, contiguous_stride(shape));

        BhArray<float> a5 = BhArray<float>(a.base,
                     shape,
                     {0, a.stride.at(1)},
                     ((c-1) * a.stride.at(0)) + (c-1) * a.stride.at(1));
        BhArray<float> a6(shape,  a.stride);
        //        BhArray<float> a6(shape, contiguous_stride(shape));
        //        divide(a4,a2,a3);
        //        multiply(a6,a4,a5);
        //        subtract(a1,a1,a6);
        free(a1);
        free(a2);
        free(a3);
        free(a4);
        free(a5);
        free(a6);
                Runtime::instance().flush();
        printf("hej\n");
    }
    BhArray<float> diag = BhArray<float>(a.base,
        a.shape,
        {a.stride.at(0) + a.stride.at(1), 0}, 0);

    BhArray<float> a7(a.shape,  a.stride);
    divide(a7, a, diag);

    Runtime::instance().flush();
    a7;

    return 0;
}
