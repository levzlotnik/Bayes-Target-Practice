//
// Created by LevZ on 6/14/2020.
//

#include "Matrix.h"
#include <algorithm>
#include <string>

Matrix::Matrix(int n, int m): n(n), m(m), sparse(false) {
    if (n <= 0 || m <= 0)
        throw runtime_error("Matrix sizes must be positive.");
    data = new float[n*m];
}

Matrix::Matrix(int n, int m, float init): Matrix(n, m) {
    std::fill_n(data, n*m, init);
}

Matrix::Matrix(const Matrix &other): Matrix(other.n, other.m) {
    if (other.data)
        std::copy_n(other.data, n*m, data);
    sparse = other.sparse;
}

Matrix::~Matrix() {
    delete [] data;
}

ostream &operator<<(ostream &os, const Matrix& matrix) {
    os << '[';
    for(int i=0; i<matrix.n; ++i){
        os << '[';
        for(int j=0; j<matrix.m; ++j){
            os << matrix.at(i, j);
            if(j < matrix.m - 1)
                os << ", ";
        }
        os << ']';
        if(i < matrix.n - 1)
            os << ", " << endl;
    }
    os << ']';
    return os;
}

Matrix &Matrix::apply_(UnaryOperation op) {
    for(int i=0; i < n*m; ++i){
        data[i] = op(data[i]);
    }
    return (*this);
}

Matrix &Matrix::apply_(const Matrix& other, BinaryOperation op) {
    check_shapes(other);
    for(int i=0; i < n*m; ++i){
        data[i] = op(data[i], other.data[i]);
    }
    return (*this);
}

Matrix &Matrix::apply_(float scalar, BinaryOperation op) {
    for(int i=0; i < n*m; ++i){
        data[i] = op(data[i], scalar);
    }
    return (*this);
}

Matrix Matrix::apply(UnaryOperation op) const {
    Matrix res(n, m);
    for(int i=0; i<n*m; ++i){
        res.data[i] = op(data[i]);
    }
    return res;
}

Matrix Matrix::apply(const Matrix &other, BinaryOperation op) const {
    check_shapes(other);
    Matrix res(n, m);
    for(int i=0; i<n*m; ++i){
        res.data[i] = op(data[i], other.data[i]);
    }
    return res;
}

Matrix Matrix::apply(float scalar, BinaryOperation op) const {
    Matrix res(n, m);
    for(int i=0; i<n*m; ++i){
        res.data[i] = op(data[i], scalar);
    }
    return res;
}

#define DEF_MATRIX_OPERATOR_MATRIX_INPLACE(op) \
    Matrix& Matrix::operator op(const Matrix& other) { \
        const BinaryOperation oper = [](float& x, float& y) {return x op y;}; \
        return apply_(other, oper); \
    }

#define DEF_MATRIX_OPERATOR_SCALAR_INPLACE(op) \
    Matrix& Matrix::operator op(float scalar) { \
        const BinaryOperation oper = [](float& x, float& y) {return x op y;}; \
        return apply_(scalar, oper); \
    }

#define DEF_MATRIX_OPERATOR_INPLACE(op) \
    DEF_MATRIX_OPERATOR_MATRIX_INPLACE(op) \
    DEF_MATRIX_OPERATOR_SCALAR_INPLACE(op)

#define DEF_MATRIX_OPERATOR_MATRIX(op) \
    Matrix Matrix::operator op(const Matrix& other) const { \
        const BinaryOperation oper = [](float& x, float& y) {return x op y;}; \
        return apply(other, oper); \
    }

#define DEF_MATRIX_OPERATOR_SCALAR(op) \
    Matrix Matrix::operator op(float scalar) const { \
        const BinaryOperation oper = [](float& x, float& y) {return x op y;}; \
        return apply(scalar, oper); \
    }

#define DEF_SCALAR_OPERATOR_MATRIX(op) \
    Matrix operator op(float scalar, const Matrix& matrix) { \
        const BinaryOperation oper = [](float& x, float& y) {return y op x;}; \
        return matrix.apply(scalar, oper); \
    }

#define DEF_MATRIX_OPERATOR(op) \
    DEF_MATRIX_OPERATOR_MATRIX(op) \
    DEF_MATRIX_OPERATOR_SCALAR(op) \
    DEF_SCALAR_OPERATOR_MATRIX(op)

// Define all basic element wise operations!
MACRO_BASIC_ARITHMETIC_INPLACE_OPERATORS(DEF_MATRIX_OPERATOR_INPLACE)
MACRO_BASIC_ARITHMETIC_OPERATORS(DEF_MATRIX_OPERATOR)

void Matrix::check_shapes(const Matrix &other) const {
    if(n != other.n || m != other.m)
        throw runtime_error("Shapes mismatch: " + str_shape() + ", " + other.str_shape());
}

string Matrix::str_shape() const{
    return "(" + to_string(n) + ", " + to_string(m) + ")";
}

float Matrix::reduce(float init_value, BinaryOperation op) {
    for (int i = 0; i < n * m ; ++i)
        init_value = op(init_value, data[i]);
    return init_value;
}

float Matrix::sum() {
    return reduce(0, [](float& x, float& y){return x+y;});
}

float Matrix::prod() {
    return reduce(1, [](float& x, float& y){return x*y;});
}

float Matrix::trace() {
    if(n!=m)
        throw runtime_error("Matrix isn't square (n,m should be equal), shape= " + str_shape());
    float t = 0;
    for(int i=0; i<n*n; i+=n)
        t += data[i];
    return t;
}

Matrix Matrix::transpose() {
    Matrix res(m, n);
    for (int i =0; i < n; ++i)
        for (int j=0; j < m; ++j)
            res.at(j, i) = this->at(i, j);

    return res;
}

Matrix Matrix::zeros(int n, int m) {
    return Matrix(n, m, 0);
}

Matrix Matrix::ones(int n, int m) {
    return Matrix(n, m, 1);
}

Matrix Matrix::zeros_like(const Matrix &matrix) {
    return zeros(matrix.n, matrix.n);
}

Matrix Matrix::ones_like(const Matrix &matrix) {
    return ones(matrix.n, matrix.n);
}

Matrix Matrix::eye(int n) {
    Matrix res(n, n, 0);
    for (int i = 0; i < n; ++i)
        res.at(i, i) = 1;
    return res;
}

void swap(Matrix &m1, Matrix &m2) noexcept {
    using std::swap;
    swap(m1.data, m2.data);
    swap(m1.n, m2.n);
    swap(m1.m, m2.m);
    swap(m1.sparse, m2.sparse);
}

Matrix &Matrix::operator=(Matrix other) {
    swap(*this, other);
    return (*this);
}

Matrix::Matrix(Matrix &&other) noexcept : Matrix() {
    swap(*this, other);
}

Matrix::Matrix() : n(0), m(0), data(nullptr), sparse(false){

}

float Matrix::mean() {
    return sum() / float(m*n);
}

Vector Matrix::flatten(bool copy) {
    return Vector(n*m, data, false, copy);
}

Matrix Matrix::reshape(int new_n, int new_m) {
    Matrix matrix(*this);
    matrix.n = new_n;
    matrix.m = new_m;
    return matrix;
}

Vector Matrix::get_row(int i) {
    return Vector(m, &this->at(i, 0), false, true);
}

void Matrix::set_row(int i, const Vector &row) {
    if (row.n != m)
        throw runtime_error("Shape mismatch: "+ to_string(m)+ ", " + to_string(row.n));
    for (int j = 0; j < m; ++j)
        this->at(i, j) = row.at(j);

}

Vector Matrix::reduce_axis(float init_value, int axis, BinaryOperation op) {
    if (axis != 0 && axis != 1)
        throw out_of_range("Axes are only 0, 1.");
    int k = axis == 0 ? n : m;
    int t = axis == 0 ? m : n;
    auto fn = axis == 0 ? &Matrix::get_row : &Matrix::get_col;
    Vector res(t, init_value);
    for (int i = 0; i < k; ++i)
        res.apply_((this->*fn)(i), op);
    return res;
}

Vector Matrix::get_col(int i) {
    Vector res(n);
    for(int j = 0; j < n; ++j)
        res[j] = this->at(j, i);

    return res;
}

Vector Matrix::sum(int axis) {
    return reduce_axis(0, axis, [](float& x, float& y) {return x+y;});
}

Vector Matrix::mean(int axis) {
    float k = (axis == 0 ? n : m);
    return sum(axis) / k;
}

float Matrix::std() {
    return flatten(false).std();
}

Vector Matrix::var(int axis) {
    if (axis != 0 && axis != 1)
        throw out_of_range("Axes are only 0, 1.");
    int k = axis == 0 ? n : m;
    int t = axis == 0 ? m : n;
    auto fn = axis == 0 ? &Matrix::get_row : &Matrix::get_col;
    Vector M2(t, 0.0), mu((this->*fn)(0)), delta(t);
    for (int i = 1; i < k; ++i) {
        auto v = (this->*fn)(i);
        delta = v - mu;
        mu += delta / float(i+1);
        M2 += (v - mu) * delta;
    }
    return M2 / float(k);
}

float Matrix::var() {
    return flatten(false).var();
}

Vector Matrix::std(int axis) {
    return this->var(axis).apply_([](float& x){return sqrt(x);});
}

Vector Matrix::get_diag() const {
    Vector res(min(n, m));
    for(int i=0; i < res.n; ++i)
        res[i] = this->at(i, i);
    return res;
}

Matrix &Matrix::set_diag(Vector diag) {
    if (diag.n != min(n, m))
        throw out_of_range("Shape mismatch: attempting to set matrix diagonal of shape (" +
            to_string(min(n, m)) + ") with vector of shape (" + to_string(diag.n) + ").");
    for (int i=0; i < diag.n; ++i)
        this->at(i, i) = diag[i];
    return (*this);
}

Matrix Matrix::operator-() const {
    return apply([](float& x){return -x;});
}

Matrix *Matrix::clone() const {
    return new Matrix(*this);
}

Matrix Matrix::diag(const Vector& v) {
    Matrix res(v.n, v.n, 0.0);
    res.set_diag(v);
    return res;
}

Matrix Matrix::T() {
    return transpose();
}

Matrix::Matrix(initializer_list<initializer_list<float>> list2d) :
    Matrix(list2d.size(), list2d.begin()->size())
{
    // Check sizes identical:
    for (auto list1d: list2d) {
        if (list1d.size() != m)
            throw invalid_argument("The initializer lists must have uniform shapes.");
    }
    // Fill in matrix:
    int i=0, j=0;
    for (auto l1d : list2d) {
        j = 0;
        for (auto x: l1d) {
            data[i*m + j] = x;
            ++j;
        }
        ++i;
    }
}

