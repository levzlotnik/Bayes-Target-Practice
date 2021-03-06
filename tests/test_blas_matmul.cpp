//
// Created by LevZ on 9/11/2020.
//


#include "../blas/blas.h"
#include "common.h"
#include <iostream>
using namespace std;
using namespace blas;

int main(){

    auto t = Tensor<double> {
            {1, 2, 3, 4,
             5, 6, 7, 8},
            {2, 4}
    };
    auto t10 = t({{}, {2, 4}});
    auto t10_c = t10.contiguous();
    auto t10_t = t10_c.transpose();
    PRINT_EXPR(t10);
    PRINT_EXPR(t10_t);
    PRINT_EXPR(matmul(t10, t));
    PRINT_EXPR(matmul(t10_t, t));

    auto t11 = arange<double>(0, 1 * 2 * 1 * 2).reshape({1, 2, 1, 2});
    PRINT_EXPR(bmm(t11, t));
    auto t12 = arange<double>(0, 2 * 1 * 2 * 1).reshape({2, 1, 2, 1});
    PRINT_EXPR(bmm(t11, t12));
    // PRINT_EXPR(matmul())
    // Stress testing matmul for profiling:
    auto beeg = uniform<double>(-1, 1, {1000, 1000});
    cout << "Profiling blas::matmul..." << endl;
    auto beeg_matmul_result = matmul(beeg, beeg);
    TensorView<double> beeg_v1 = beeg.const_view({100, 1, 40, 250});
    TensorView<double> beeg_v2 = beeg.const_view({100, 10, 250, 4});
    cout << "Profiling blas::bmm..." << endl;
    auto beeg_bmm_result = bmm(beeg_v1, beeg_v2);
    return 0;
}