/**********************************************************
 * Do not edit this file. It has been auto generated by   *
 * ../core/codegen/gen_extmethod.py at @!timestamp!@.  *
 **********************************************************/
#include <bh_extmethod.hpp>
    #include <bh_main_memory.hpp>

#if defined(__APPLE__) || defined(__MACOSX)
    #include <Accelerate/Accelerate.h>
    #define LAPACK_FUN(op) op##_
    #define __LAPACKE_ROW_MAJOR_ARG
#else
    #include <lapacke.h>
    #define LAPACK_FUN(op) LAPACKE_##op
    #define __LAPACKE_ROW_MAJOR_ARG LAPACK_ROW_MAJAOR,
#endif

#include <stdexcept>
#include <algorithm>

using namespace bohrium;
using namespace extmethod;
using namespace std;

namespace {
    @!body!@
} /* end of namespace */

@!footer!@
