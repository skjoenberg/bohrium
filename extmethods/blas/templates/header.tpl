/**********************************************************
 * Do not edit this file. It has been auto generated by   *
 * ../core/codegen/gen_extmethod.py at @!timestamp!@.  *
 **********************************************************/
#include <bh_extmethod.hpp>

#if defined(__APPLE__) || defined(__MACOSX)
    #include <Accelerate/Accelerate.h>
#else
    #include <cblas.h>
#endif

#include <stdexcept>

using namespace bohrium;
using namespace extmethod;
using namespace std;

namespace {

    void cblas_sgemmt(CBLAS_ORDER layout, CBLAS_TRANSPOSE TransA, CBLAS_TRANSPOSE TransB, const int M, const int N, const int K, const bh_float32 alpha, const bh_float32 *A, const int lda, const bh_float32 *B, const int ldb, const bh_float32 beta, bh_float32 *C, const int ldc) {
        cblas_sgemm(layout, TransA, TransB, K, N, M, alpha, A, K, B, K, beta, C, ldc);
    }

    void cblas_dgemmt(CBLAS_ORDER layout, CBLAS_TRANSPOSE TransA, CBLAS_TRANSPOSE TransB, const int M, const int N, const int K, const bh_float64 alpha, const bh_float64 *A, const int lda, const bh_float64 *B, const int ldb, const bh_float64 beta, bh_float64 *C, const int ldc) {
        cblas_dgemm(layout, TransA, TransB, K, N, M, alpha, A, K, B, K, beta, C, ldc);
    }

    void cblas_cgemmt(CBLAS_ORDER layout, CBLAS_TRANSPOSE TransA, CBLAS_TRANSPOSE TransB, const int M, const int N, const int K, void* alpha, const bh_complex64 *A, const int lda, const bh_complex64 *B, const int ldb, void* beta, bh_complex64 *C, const int ldc) {
        cblas_cgemm(layout, TransA, TransB, K, N, M, alpha, A, K, B, K, beta, C, ldc);
    }

    void cblas_zgemmt(CBLAS_ORDER layout, CBLAS_TRANSPOSE TransA, CBLAS_TRANSPOSE TransB, const int M, const int N, const int K, void* alpha, const bh_complex128 *A, const int lda, const bh_complex128 *B, const int ldb, void* beta, bh_complex128 *C, const int ldc) {
        cblas_zgemm(layout, TransA, TransB, K, N, M, alpha, A, K, B, K, beta, C, ldc);
    }

    @!body!@
} /* end of namespace */

@!footer!@
