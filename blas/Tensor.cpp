//
// Created by LevZ on 7/8/2020.
//

#include <utility>
#include <numeric>
#include <algorithm>
#include "Tensor.h"
#include <iomanip>

#define MAX_ROW_STRING_SIZE 50
#define MAX_EXPANSION_STRING_SIZE 5

using std::cout;
using std::endl;


inline size_t shape2size(const std::vector<size_t>& shape) {
    return std::accumulate(shape.begin(), shape.end(), 1, std::multiplies<size_t>{});
}
std::string shape2str(const shape_t &shape) {
    std::string ret = "(";
    for (int i = 0; i < shape.size(); ++i) {
        ret += std::to_string(shape[i]);
        if (i < shape.size() - 1)
            ret += ", ";
    }
    ret += ")";
    return ret;
}
std::string shape2str(const std::vector<long> &shape) {
    std::string ret = "(";
    for (int i = 0; i < shape.size(); ++i) {
        ret += std::to_string(shape[i]);
        if (i < shape.size() - 1)
            ret += ", ";
    }
    ret += ")";
    return ret;
}

namespace blas {

    template<typename T>
    Tensor<T>::Tensor() : data(nullptr), size(0) {

    }

    template<typename T>
    Tensor<T>::Tensor(T scalar) : data(new T(scalar)), size(1) {
    }

    template<typename T>
    Tensor<T>::Tensor(std::vector<T> data, const std::vector<size_t> &shape) : Tensor(data.data(), shape) {

    }

    template<typename T>
    Tensor<T>::Tensor(const Tensor &other) :
            data(new T[other.size]), shape(other.shape), size(other.size), strides(other.strides) {
        for (int i = 0; i < size; ++i)
            this->data[i] = other.data[i];
    }

    template<typename T>
    Tensor<T>::Tensor(Tensor &&other) noexcept : Tensor() {
        using std::swap;
        swap(*this, other);
    }

    template<typename T>
    Tensor<T>::~Tensor() {
        if (requires_deletion)
            delete[] data;
    }

    template<typename T>
    Tensor<T> &Tensor<T>::operator=(Tensor &&other) noexcept {
        swap(*this, other);
        return *this;
    }

    index_t check_index(const index_t& index, const shape_t& shape) {
        using std::to_string;
        if (index.size() > shape.size())
            throw std::out_of_range(
                    "Index has " + to_string(index.size()) + " > " + to_string(shape.size()) + " dimensions."
            );
        index_t ret(index);
        for (int i=0; i < index.size(); ++i)
            ret[i] = normalize_index(index[i], shape[i]);
        return ret;
    }

    shape_t shape2strides(const shape_t &shape) {
        std::vector<size_t> res;
        res.reserve(shape.size());
        size_t stride = std::accumulate(shape.begin(), shape.end(), 1, std::multiplies<size_t>{});
        for (auto s: shape) {
            stride /= s;
            res.push_back(stride);
        }
        return res;
    }

    template<typename T>
    Tensor<T>::Tensor(T *data, const std::vector<size_t> &shape) :
            size(shape2size(shape)), data(new T[shape2size(shape)]), shape(shape), strides(shape2strides(shape)) {
        for (int i = 0; i < size; ++i)
            this->data[i] = data[i];
    }

    template<class Tensor1, class Tensor2>
    Tensor1 &copy_(Tensor1 &dst, const Tensor2 &src) {
        using sceiterator = typename Tensor2::ceiterator;
        using deiterator = typename Tensor1::eiterator;
        if (dst.shape != src.shape)
            throw shape_mismatch(dst.shape, src.shape, "copy_");
        sceiterator src_it = Tensor2::const_elem_begin(src);
        deiterator dst_it = Tensor1::elem_begin(dst), dst_it_end = Tensor1::elem_end(dst);
        while (dst_it != dst_it_end) {
            *dst_it = *src_it;
            ++dst_it; ++src_it;
        }
        return dst;
    }


    template<typename T>
    typename Tensor<T>::iterator Tensor<T>::begin() {
        if (shape == shape_t{1})
            throw std::out_of_range("Cannot iterate over a scalar tensor.");
        size_t stride = strides[0];
        shape_t remaining_shape{shape.begin() + 1, shape.end()};
        return Tensor::iterator(data, stride, 0, remaining_shape);
    }

    template<typename T>
    typename Tensor<T>::iterator Tensor<T>::end() {
        if (shape == shape_t{1})
            throw std::out_of_range("Cannot iterate over a scalar tensor.");
        size_t stride = strides[0];
        size_t pos = shape[0];
        shape_t remaining_shape{shape.begin() + 1, shape.end()};
        return Tensor::iterator(data, stride, pos, remaining_shape);
    }

    template<typename T>
    typename Tensor<T>::const_iterator Tensor<T>::begin() const {
        if (shape == shape_t{1})
            throw std::out_of_range("Cannot iterate over a scalar tensor.");
        size_t stride = strides[0];
        shape_t remaining_shape{shape.begin() + 1, shape.end()};
        return Tensor::const_iterator(data, stride, 0, remaining_shape);
    }

    template<typename T>
    typename Tensor<T>::const_iterator Tensor<T>::end() const {
        if (shape == shape_t{1})
            throw std::out_of_range("Cannot iterate over a scalar tensor.");
        size_t stride = strides[0];
        size_t pos = shape[0];
        shape_t remaining_shape{shape.begin() + 1, shape.end()};
        return Tensor::const_iterator(data, stride, pos, remaining_shape);
    }

    template<typename T>
    Tensor<T> Tensor<T>::reshape(const vector<long> &new_shape) const {
        return ((Tensor<T> &) *this).view(new_shape).contiguous();
    }

    shape_t normalize_shape(const vector<long>& s, const shape_t& old_shape) {
        bool found_neg_one = false;
        int neg_one_idx = -1;
        size_t accumulated_size = 1, total_size = shape2size(old_shape);
        shape_t ret(s.begin(), s.end());
        for (int i=0; i < s.size(); ++i) {
            long s_i = s[i];
            if (s_i > 0)
                accumulated_size *= s_i;
            else if (s_i == -1) {
                if (found_neg_one)
                    throw std::out_of_range("Shape can only have a single (-1) element.");
                found_neg_one = true;
                neg_one_idx = i;
            }
            else
                throw std::out_of_range("Shape must have positive elements except a single (-1).");
        }
        if (found_neg_one)
            ret[neg_one_idx] = total_size / accumulated_size;
        else if (accumulated_size != total_size)
            throw shape_mismatch(s, old_shape, "normalize_shape");
        return ret;
    }

    template<typename T>
    TensorView<T> Tensor<T>::view(const vector<long> &new_shape) {
        shape_t normalized = normalize_shape(new_shape, shape);
        return TensorView<T>(data, normalized);
    }

    std::pair<size_t, size_t>
    ravel_index_checked(const std::vector<int> &idx, const std::vector<size_t> &shape, int size) {
        using std::to_string;
        if (idx.size() > shape.size())
            throw std::out_of_range("Index cannot be of larger dimension of the shape.");
        size_t index = 0;
        size_t stride = size > 0 ? size :
                        std::accumulate(shape.begin(), shape.end(), 1, std::multiplies<size_t>{});
        int curr_index;
        for (int i = 0; i < idx.size(); ++i) {
            stride /= shape[i];
            curr_index = idx[i];
            if (curr_index < -shape[i] || curr_index >= shape[i])
                throw std::out_of_range("Index out of range in dimension "
                                        + to_string(i) + ": idx=" + to_string(curr_index) + ", shape=" +
                                        to_string(shape[i]) + ".");
            curr_index = (curr_index + shape[i]) % shape[i];
            index += stride * curr_index;
        }
        return {index, stride};
    }

    template<typename T>
    TensorView<T>::TensorView(T *data, const std::vector<size_t> &shape) : Tensor<T>() {
        Tensor<T>::data = data;
        Tensor<T>::shape = shape;
        Tensor<T>::strides = shape2strides(shape);
        Tensor<T>::size = shape2size(shape);
        Tensor<T>::requires_deletion = false;
    }

    template<typename T>
    TensorView<T>::TensorView(Tensor<T> t) : TensorView<T>(t.get_data_ptr(), t.shape) {

    }

    template<typename T>
    template<typename Tnsr>
    Tensor<T>::subtensor_iterator<Tnsr>::subtensor_iterator(T *data_ptr, size_t stride, size_t pos, shape_t shape) :
            baseline_data_ptr(data_ptr), stride(stride), pos(pos), shape(std::move(shape)) {

    }


    size_t ravel_index(const index_t &idx, const shape_t &shape, int size) {
        size_t index = 0;
        size_t stride = size > 0 ? size :
                        std::accumulate(shape.begin(), shape.end(), 1, std::multiplies<size_t>{});
        int curr_index;
        size_t size_idx = idx.size();
        for (int i = 0; i < size_idx; ++i) {
            stride /= shape[i];
            curr_index = idx[i];
            index += stride * curr_index;
        }
        return index;
    }

    index_t unravel_index(size_t true_idx, const shape_t &shape, int size) {
        int stride = size > 0 ?
                     size :
                     std::accumulate(shape.begin(), shape.end(), 1, std::multiplies<size_t>{});
        index_t res(shape.size());
        for (int i = 0; i < res.size(); ++i) {
            stride /= shape[i];
            res[i] = true_idx / stride;
            true_idx %= stride;
        }
        return res;
    }

    template<template<typename>class Tens, typename T>
    Tens<T> &fill_(Tens<T> &dst, T scalar) {
        auto it = Tens<T>::elem_begin(dst), it_end = Tens<T>::elem_end(dst);
        for (; it != it_end; ++it)
            *it = scalar;
        return dst;
    }

    template<typename T>
    ostream &Tensor<T>::print_to_os(ostream &os, bool rec_start) const {
        using std::to_string;
        using std::endl;
        if (rec_start && print_precision != 0) {
            os << std::scientific;
            os << std::setprecision(print_precision);
        }
        os << (rec_start ? "Tensor(" : "");
        os << (dim() > 0 ? "[" : "") ;
        if (dim() <= 1) {
            size_t expected_string_size = sizeof(", ") * (size - 1) + to_string(data[0]).size() * size;
            if (expected_string_size < MAX_ROW_STRING_SIZE)
                for (int i = 0; i < size; ++i) {
                    os << data[i];
                    if (i < size - 1)
                        os << ", ";
                }
            else
                os << data[0] << ",..., " << data[size - 1];

        } else {
            if (shape[0] <= MAX_EXPANSION_STRING_SIZE) {
                int i = 0;
                for (const auto &sub: *this) {
                    sub.print_to_os(os, false);
                    if (++i < shape[0]) os << "," << endl << "       ";
                }
            } else {
                this->operator[](0).print_to_os(os, false);
                os << "," << endl << "...," << endl;
                this->operator[](-1).print_to_os(os, false);
            }
        }
        os << (dim() > 0 ? "]" : "") ;
        if (rec_start) os << ")";
        return os;
    }

    template<typename T>
    Tensor<T> Tensor<T>::operator[](long idx) const {
        idx = normalize_index(idx, shape[0]);
        return unchecked_subscript(idx);
    }

    template<typename T>
    TensorView<T> Tensor<T>::operator[](long idx) {
        idx = normalize_index(idx, shape[0]);
        return unchecked_subscript(idx);
    }

    template<typename T>
    Tensor<T> Tensor<T>::operator[](const index_t &index) const {
        index_t idx = normalize_index(index, shape);
        return unchecked_subscript(idx);
    }

    template<typename T>
    TensorView<T> Tensor<T>::operator[](const index_t &index) {
        index_t idx = normalize_index(index, shape);
        return unchecked_subscript(idx);
    }

    template<typename T>
    Tensor<T> Tensor<T>::operator()(const Slice &slice) const {
        auto s = normalize_slice(slice, -1);
        return unchecked_slice(s);
    }

    template<typename T>
    TensorSliced<T> Tensor<T>::operator()(const Slice &slice) {
        auto s = normalize_slice(slice, -1);
        return unchecked_slice(s);
    }

    template<typename T>
    Tensor<T> Tensor<T>::operator()(const SliceGroup &slice_group) const {
        auto sg = normalize_slice_group(slice_group);
        return unchecked_slice_group(sg);
    }

    template<typename T>
    TensorSliced<T> Tensor<T>::operator()(const SliceGroup &slice_group) {
        auto sg = normalize_slice_group(slice_group);
        return unchecked_slice_group(sg);
    }

    template<typename T>
    Tensor<T> Tensor<T>::operator-() const {
        return apply([](T x) -> T { return -x; });
    }

    template<typename T>
    Slice Tensor<T>::normalize_slice(const Slice &slice, long max_size) const {
        max_size = max_size <= 0 ? shape[0] : max_size;
        long s_b = normalize_index(slice.b, max_size);
        long s_e = (slice.e == slice.b) ?
                    max_size :
                    normalize_index(slice.e, max_size, true);
        return Slice {s_b, s_e, slice.stride};
    }

    template<typename T>
    shape_t Tensor<T>::slice2shape(const Slice &slice) const {
        shape_t result(shape);
        result[0] = slice.size();
        return result;
    }

    template<typename T>
    Tensor<T>::Tensor(const std::vector<size_t> &shape) :
            size(shape2size(shape)), data(new T[shape2size(shape)]), shape(shape), strides(shape2strides(shape)) {
    }

    template<typename T>
    SliceGroup Tensor<T>::normalize_slice_group(const SliceGroup &group) const {
        auto g = group;
        for (int i = 0; i < group.slices.size(); ++i)
            g.slices[i] = normalize_slice(group.slices[i], shape[i]);
        return g;
    }

    template<typename T>
    Tensor<T> Tensor<T>::unchecked_subscript(long idx) const {
        Tensor::const_iterator it = begin();
        return *(it + idx);
    }

    template<typename T>
    TensorView<T> Tensor<T>::unchecked_subscript(long idx) {
        Tensor::iterator it = begin();
        return *(it + idx);
    }

    template<typename T>
    Tensor<T> Tensor<T>::unchecked_subscript(const index_t &index) const {
        long idx = ravel_index(index, shape, size);
        std::vector<size_t> remaining_shape = std::vector<size_t>{shape.begin() + index.size(), shape.end()};
        return Tensor(&data[idx], remaining_shape);
    }

    template<typename T>
    TensorView<T> Tensor<T>::unchecked_subscript(const index_t &index) {
        long idx = ravel_index(index, shape, size);
        std::vector<size_t> remaining_shape = std::vector<size_t>{shape.begin() + index.size(), shape.end()};
        return TensorView(&data[idx], remaining_shape);
    }

    template<typename T>
    TensorView<T> Tensor<T>::optimized_unchecked_subscript(int idx) const {
        TensorView<T> x = const_cast<Tensor *>(this)->unchecked_subscript(idx);
        return static_cast<TensorView<T>>(x);
    }

    template<typename T>
    TensorView<T> Tensor<T>::optimized_unchecked_subscript(const index_t &index) const {
        TensorView<T> x = const_cast<Tensor *>(this)->unchecked_subscript(index);
        return static_cast<TensorView<T>>(x);
    }

    template<typename T>
    Tensor<T> Tensor<T>::unchecked_slice(const Slice &slice) const {
        SliceGroup sg;
        sg.slices.reserve(shape.size());
        sg.slices.emplace_back(slice);
        for (int i = 1; i < shape.size(); ++i)
            sg.slices.emplace_back(0, shape[i], 1);
        return TensorSliced<T>::_from_const(*this, sg).contiguous();
    }

    template<typename T>
    TensorSliced<T> Tensor<T>::unchecked_slice(const Slice &slice) {
        SliceGroup sg;
        sg.slices.reserve(shape.size());
        sg.slices.emplace_back(slice);
        for (int i = 1; i < shape.size(); ++i)
            sg.slices.emplace_back(0, shape[i], 1);
        return TensorSliced(*this, sg);
    }

    template<typename T>
    Tensor<T> Tensor<T>::unchecked_slice_group(const SliceGroup &slice_group) const {
        return TensorSliced<T>::_from_const(*this, slice_group).contiguous();
    }

    template<typename T>
    TensorSliced<T> Tensor<T>::unchecked_slice_group(const SliceGroup &slice_group) {
        return TensorSliced<T>(*this, slice_group);
    }


    template<typename T>
    ostream &TensorSliced<T>::print_to_os(ostream &os, bool rec_start) const {
        return this->contiguous().print_to_os(os, rec_start);
    }

    template<typename T>
    T *Tensor<T>::get_data_ptr() {
        return data;
    }

    template<typename T>
    T *Tensor<T>::get_data_ptr() const {
        return data;
    }

    Slice::Slice(long b, long e, long stride) : b(b), e(e), stride(stride), size_(get_size(b, e, stride)) {
        check_stride();
    }

    Slice::Slice(initializer_list<long> lst) : b(0), e(0), stride(1) {
        using std::tuple;
        switch (lst.size()) {
            case 0:
                break;
            case 1: {
                long b_ = *lst.begin();
                std::tie(b, e) = tuple{b_, b_ + 1};
                break;
            }
            case 2: {
                b = *lst.begin();
                e = *(lst.begin() + 1);
                stride = 1;
                break;
            }
            case 3: {
                b = *lst.begin();
                e = *(lst.begin() + 1);
                stride = *(lst.begin() + 2);
                break;
            }
            default:
                throw std::invalid_argument("Slice takes 0 to 3 arguments.");
        }
        size_ = get_size(b, e, stride);
        check_stride();
    }

    constexpr void Slice::check_stride() const {
        if (e == b)
            return;
        if ((e - b) * stride <= 0)// stride cannot reach end from beginning.
            throw std::out_of_range("Iteration cannot reach end with current stride.");
    }

    Slice Slice::subslice(const Slice &rs) const {
        long b_abs = rs.b >= 0 ?
                     this->b + rs.b * this->stride:
                     this->e - rs.b * this->stride;
        long e_abs = rs.e >= 0 ?
                     this->b + rs.e * this->stride:
                     this->e - rs.e * this->stride;
        long stride_abs = this->stride * rs.stride;
        return {b_abs, e_abs, stride_abs};
    }

    SliceGroup::SliceGroup(const vector<tuple<int, int, int>> &slices) {
        for (auto tup: slices)
            this->slices.emplace_back(tup);
    }

    SliceGroup::SliceGroup(initializer_list<initializer_list<long>> lst) {
        for (auto l: lst)
            this->slices.emplace_back(l);
    }

    size_t SliceGroup::size() const {
        return std::accumulate(slices.begin(), slices.end(), 1,
                               [](size_t x, Slice s) { return x * s.size(); });
    }

    string SliceGroup::to_str() const {
        string res = "[";
        for (int i=0; i < slices.size(); ++i) {
            auto s = slices[i];
            res += s.to_str();
            if (i < slices.size()-1) res+= ", ";
        }
        res += "]";
        return res;
    }

    SliceGroup SliceGroup::subslice(const SliceGroup &relative_slice) const {
        SliceGroup ret;
        size_t ret_size = std::max(this->slices.size(), relative_slice.slices.size());
        ret.slices.reserve(ret_size);
        for (int i=0; i < ret_size; ++i) {
            if (i < relative_slice.slices.size() && i < this->slices.size())
                ret.slices.emplace_back(this->slices[i].subslice(relative_slice.slices[i]));
            else
                ret.slices.emplace_back(i < this->slices.size() ?
                                        this->slices[i] :
                                        relative_slice.slices[i]);
        }
        return ret;
    }


    SliceGroup::const_iterator::const_iterator(index_t pos, vector<Slice> slices,
                                               size_t elems_passed) :
            pos(std::move(pos)), slices(std::move(slices)), elems_passed(elems_passed) {

    }

    inline bool past_end(int pos, int stride, int end) {
        return stride > 0 ? pos >= end : pos <= end;
    }

    SliceGroup::const_iterator &SliceGroup::const_iterator::operator++() {
        ++elems_passed;
        for (int i = pos.size() - 1; i >= 0; --i) {
            auto s = slices[i];
            if (s.size() > 1) {
                pos[i] += s.stride;
                if (past_end(pos[i], s.stride, s.e))
                    pos[i] = s.b;
                else
                    break;
            }
        }
        return *this;
    }

    template<typename T>
    typename TensorSliced<T>::eiterator TensorSliced<T>::elem_begin(TensorSliced<T> &ts) {
        return typename TensorSliced<T>::eiterator{
            ts.data, ts.slice_group.begin(), ts.underlying_tensor_shape, ts.underlying_tensor_size
        };
    }

    template<typename T>
    typename TensorSliced<T>::eiterator TensorSliced<T>::elem_end(TensorSliced<T> &ts) {
        return typename TensorSliced<T>::eiterator{
                ts.data, ts.slice_group.end(), ts.underlying_tensor_shape, ts.underlying_tensor_size
        };
    }

    template<typename T>
    typename TensorSliced<T>::ceiterator TensorSliced<T>::const_elem_begin(const TensorSliced<T> &ts) {
        return typename TensorSliced<T>::ceiterator{
                ts.data, ts.slice_group.begin(), ts.underlying_tensor_shape, ts.underlying_tensor_size
        };
    }

    template<typename T>
    typename TensorSliced<T>::ceiterator TensorSliced<T>::const_elem_end(const TensorSliced<T> &ts) {
        return typename TensorSliced<T>::ceiterator{
                ts.data, ts.slice_group.end(), ts.underlying_tensor_shape, ts.underlying_tensor_size
        };
    }

    template<typename T>
    TensorSliced<T>::TensorSliced(T *data, const shape_t &shape, const SliceGroup &slice_group) :
            Tensor<T>(),
            underlying_tensor_shape(shape), underlying_tensor_size(shape2size(shape)),
            slice_group(slice_group.fill_to_shape(shape)) {
        Tensor<T>::data = data;
        shape_t slice_shape = this->slice_group.shape();
        Tensor<T>::shape = slice_shape;
        Tensor<T>::strides = shape2strides(slice_shape);
        Tensor<T>::size = shape2size(slice_shape);
        Tensor<T>::requires_deletion = false;
        Tensor<T>::is_sliced = true;
    }

    template<typename T>
    TensorSliced<T>::TensorSliced(Tensor<T>& t, const SliceGroup &slice_group) :
            TensorSliced<T>(t.get_data_ptr(), t.shape, slice_group) {

    }
    template<typename T>
    TensorSliced<T> TensorSliced<T>::_from_const(const Tensor<T> &t, const SliceGroup &slice_group) {
        return TensorSliced(t.get_data_ptr(), t.shape, slice_group);
    }

    template<typename T>
    TensorSliced<T>::TensorSliced(const TensorSliced& other) :
        TensorSliced<T>(other.data, other.underlying_tensor_shape, other.slice_group) {

    }

    template<typename T>
    T &TensorSliced<T>::get(TensorSliced<T> &ts, size_t true_idx) {
        index_t vec_idx = ts.slice_group.translate_true_idx(true_idx);
        size_t data_relative_index = ravel_index(vec_idx, ts.shape, ts.size);
        return ts.data[data_relative_index];
    }

    template<typename T>
    T TensorSliced<T>::get(const TensorSliced<T> &ts, size_t true_idx) {
        index_t vec_idx = ts.slice_group.translate_true_idx(true_idx);
        size_t data_relative_index = ravel_index(vec_idx, ts.underlying_tensor_shape);
        return ts.data[data_relative_index];
    }

    template<typename T>
    Tensor<T> TensorSliced<T>::contiguous() const {
        Tensor<T> t(this->shape);
        t.copy_(*this);
        return t;
    }

    Slice& select_elem(Slice& s, long idx) {
        long new_b = s.b + idx * s.stride;
        long new_e = s.b + (idx + 1) * s.stride;
        s.b = new_b;
        s.e = new_e;
        return s;
    }

    template<typename T>
    Tensor<T> TensorSliced<T>::unchecked_subscript(long idx) const {
        SliceGroup sg{this->slice_group};
        select_elem(sg.slices[0], idx);
        TensorSliced ret(this->data, this->underlying_tensor_shape, std::move(sg));
        return ret.contiguous();
    }

    template<typename T>
    Tensor<T> TensorSliced<T>::unchecked_subscript(const index_t &index) const {
        SliceGroup sg {this->slice_group};
        for (int i=0; i < index.size(); ++i)
            select_elem(sg.slices[i], index[i]);
        TensorSliced ret(this->data, this->underlying_tensor_shape, std::move(sg));
        return ret.contiguous();
    }

    template<typename T>
    Tensor<T> TensorSliced<T>::unchecked_slice(const Slice &slice) const {
        SliceGroup sg {this->slice_group};
        sg.slices[0] = sg.slices[0].subslice(slice);
        TensorSliced ret(this->data, this->underlying_tensor_shape, sg);
        return ret.contiguous();
    }

    template<typename T>
    TensorSliced<T> TensorSliced<T>::unchecked_slice(const Slice &slice) {
        SliceGroup sg {this->slice_group};
        sg.slices[0] = sg.slices[0].subslice(slice);
        TensorSliced ret(this->data, this->underlying_tensor_shape, sg);
        return ret;
    }

    template<typename T>
    Tensor<T> TensorSliced<T>::unchecked_slice_group(const SliceGroup &rel_sg) const {
        TensorSliced ret(this->data, this->underlying_tensor_shape, this->slice_group.subslice(rel_sg));
        return ret.contiguous();
    }

    template<typename T>
    TensorSliced<T> TensorSliced<T>::unchecked_slice_group(const SliceGroup &rel_sg) {
        TensorSliced ret(this->data, this->underlying_tensor_shape, this->slice_group.subslice(rel_sg));
        return ret;
    }

    template<typename T>
    Tensor<T> TensorSliced<T>::contiguous() {
        Tensor<T> ret(this->shape);
        auto ret_it = Tensor<T>::elem_begin(ret), ret_end = Tensor<T>::elem_end(ret);
        auto it = TensorSliced::const_elem_begin(*this);
        for (; ret_it != ret_end ; ++ret_it, ++it)
            *ret_it = *it;
        return ret;
    }

    template<typename T>
    Tensor<T> TensorSliced<T>::reshape(const vector<long> &new_shape) const {
        shape_t normalized = normalize_shape(new_shape, this->shape);
        Tensor<T> ret(normalized);
        TensorView<T> buff = ret.view(this->shape);
        buff.copy_(*this);
        return ret;
    }

#define OUT

    /**
     * Pulls an applicable index for unbroadcasted tensor using broadcasted destination
     * @param brdcst_dst_idx : the broadcasted destination index
     * @param brdcst_dst_shp : the broadcasted destination shape
     * @param unbrdcst_src : the unbroadcasted source shape
     * @return
    */
    shape_t pull_unbroadcasted_index(const shape_t& brdcst_dst_idx ,const shape_t& brdcst_dst_shp,
                                     const shape_t& unbrdcst_src) {
        shape_t res(unbrdcst_src.size());
        for (int i=0; i < res.size(); ++i){
            res[res.size() - 1 - i] = unbrdcst_src[res.size() - 1 - i] == 1 ?
                                      0 : brdcst_dst_idx[brdcst_dst_idx.size() - 1- i];
        }
        return res;
    }

    /**
     * Applies an element-wise function on all elements and stores into destination.
     * @tparam Tensor1
     * @tparam Tensor2
     * @tparam T
     * @param dst
     * @param src
     * @param op
     * @return
     */
    template<template<typename> class  Tensor1, template<typename> class  Tensor2, typename T>
    Tensor1<T> &_apply_tensors_(Tensor1<T> &dst, const Tensor2<T> &src, const binary_op<T>& op) {
        if (dst.shape != src.shape)
            // Maybe broadcasting will work.
            return _apply_broadcast_(dst, src, op);
        auto it_dst = Tensor1<T>::elem_begin(dst), it_dst_end=Tensor1<T>::elem_end(dst);
        auto it_src = Tensor2<T>::const_elem_begin(src);
        while (it_dst != it_dst_end)
            *it_dst++ = *it_src++;
        return dst;
    }

    /**
     * Applies an element-wise function on all elements and stores into destination, using broadcast mechanics.
     * @tparam Tensor1
     * @tparam Tensor2
     * @tparam T
     * @param dst
     * @param src
     * @param op
     * @return
     */
    template<template<typename> class  Tensor1, template<typename> class  Tensor2, typename T>
    Tensor1<T> &_apply_broadcast_(Tensor1<T> &dst, const Tensor2<T> &src, const binary_op<T>& op) {
        shape_t broadcasted_shape = broadcast_shapes(dst.shape, src.shape);
        if (broadcasted_shape != dst.shape)
            throw broadcast_failure(dst.shape, broadcasted_shape);
        // If broadcast has succeeded - we know that src.size() < dst.size(). (Morever - src.size() | dst.size())
        // Hence we'll iterate over src to make as little calls to src memory (as we cannot make less calls on dst).
        for (size_t src_idx_true = 0; src_idx_true < src.size; ++src_idx_true){
            T src_x = Tensor2<T>::get(src, src_idx_true);
            index_t src_idx = unravel_index(src_idx_true, src.shape, src.size);
            SliceGroup sg = broadcast_index(src_idx, src.shape, dst.shape);
            dst.unchecked_slice_group(sg).apply_(src_x, op);
        }
        return dst;
    }

    /**
     * Applies an element-wise function on all elements and stores into a new tensor..
     * @tparam Tensor1
     * @tparam Tensor2
     * @tparam T
     * @param dst
     * @param src
     * @param op
     * @return
     */
    template<template<typename> class TnsrSrc1,template<typename> class TnsrSrc2,template<typename> class TnsrDst, typename T>
    void _apply_tensors(const TnsrSrc1<T> &src1, const TnsrSrc2<T> &src2, const binary_op<T>& op, TnsrDst<T> &dst) {
        if (src1.shape != src2.shape) {
            // Maybe broadcast will help.
            _apply_broadcast(src1, src2, op, dst);
            return;
        }
        auto it_dst = TnsrDst<T>::elem_begin(dst), it_dst_end = TnsrDst<T>::elem_end(dst);
        auto [it_src1, it_src2] = tuple { TnsrSrc1<T>::const_elem_begin(src1), TnsrSrc2<T>::const_elem_begin(src2) };
        while (it_dst != it_dst_end)
            *it_dst++ = op(*it_src1++, *it_src2++);
    }

    template<template<typename> class  Tensor1, template<typename> class Tensor2, typename T>
    Tensor<T> _apply_tensors(const Tensor1<T> &src1, const Tensor2<T> &src2, const binary_op<T>& op) {
        Tensor<T> dst(broadcast_shapes(src1.shape, src2.shape));
        _apply_tensors(src1, src2, op, dst);
        return dst;
    }

    tuple<SliceGroup /*src2*/, SliceGroup /*dst*/>
    broadcast_index(index_t src1_idx, const shape_t& src1_shape, const shape_t& src2_shape, const shape_t& dst_shape){
        // TODO - optimize this mechanic.
        SliceGroup sg_src2(src2_shape.size()), sg_dst(dst_shape.size());
        for (int i = 0; i < dst_shape.size() ; ++i){
            size_t dsts_idx = dst_shape.size() - 1 - i;
            size_t src1s_idx = src1_shape.size() - 1 - i;
            size_t src2s_idx = src2_shape.size() - 1 - i;
            Slice &src2_s = sg_src2.slices[src2s_idx], &dst_s = sg_dst.slices[dsts_idx];
            if (src1s_idx < 0){ // this shape has finished, which means that other two shapes are definitely not
                size_t s2 = src2_shape[src2s_idx], d = dst_shape[dsts_idx];
                src2_s.e = s2;
                dst_s.e = d;
            }
            else if (src1_shape[src1s_idx] == 1){
                size_t d = dst_shape[dsts_idx];
                dst_s.e = d; dst_s.update();
                if (src2s_idx >= 0){
                    size_t s2 = src2_shape[src2s_idx];
                    src2_s.e = s2;
                }
            }
            else {
                long s1 = src1_idx[src1s_idx];
                dst_s.b =s1; dst_s.e = s1 + 1;
                if (src2s_idx >= 0){
                    size_t s2 = src2_shape[src2s_idx];
                    if (s2 != 1) {
                        src2_s.b = s1;
                        src2_s.e = s1+1;
                    } else {
                        src2_s.e = 1;
                    }
                }
            }
            src2_s.update();
            dst_s.update();
        }
        return {sg_src2, sg_dst};
    }


    /**
     * Applies an element-wise function on all elements and stores into a new tensor, using broadcast mechanics.
     * @tparam Tensor1
     * @tparam Tensor2
     * @tparam T
     * @param dst
     * @param src
     * @param op
     * @return
     */
    template<template<typename> class TnsrSrc1,template<typename> class TnsrSrc2,template<typename> class TnsrDst, typename T>
    void _apply_broadcast(const TnsrSrc1<T> &src1, const TnsrSrc2<T> &src2, const binary_op<T>& op, TnsrDst<T> &dst) {
        shape_t broadcast_shape = broadcast_shapes(src1.shape, src2.shape);
        if (dst.shape != broadcast_shape)
            throw broadcast_failure(dst.shape, broadcast_shape);
        // We would like the smaller size tensor to be iterated on.
        if (src1.size < src2.size){
            for (size_t src1_true_idx = 0; src1_true_idx < src1.size; ++src1_true_idx) {
                T src1_x = TnsrSrc1<T>::get(src1, src1_true_idx);
                index_t src1_idx = unravel_index(src1_true_idx, src1.shape, src1.size);
                auto [sg_src2, sg_dst] = broadcast_index(src1_idx, src1.shape, src2.shape, dst.shape);
                const binary_op<T> rev_op = [op](T x, T y) -> T { return op(y, x); };
                const Tensor<T>& src2_slice = src2.unchecked_slice_group(sg_src2);
                TensorSliced<T> dst_slice = dst.unchecked_slice_group(sg_dst);
                _apply_scalar(src2_slice, src1_x, rev_op, OUT dst_slice);
            }
        }
        // Unavoidable code duplication since this is a template.
        else {
            for (size_t src2_idx_true = 0; src2_idx_true < src2.size; ++src2_idx_true) {
                T src2_x = TnsrSrc2<T>::get(src2, src2_idx_true);
                index_t src2_idx = unravel_index(src2_idx_true, src2.shape, src2.size);
                auto [sg_src1, sg_dst] = broadcast_index(src2_idx, src2.shape, src1.shape, dst.shape);
                auto dst_slice = dst.unchecked_slice_group(sg_dst);
                _apply_scalar(src1.unchecked_slice_group(sg_src1), src2_x, op,OUT dst_slice);
            }
        }
    }

    template<template<typename>class Tnsr, typename T>
    Tnsr<T> &_apply_unary_(Tnsr<T> &dst, const unary_op<T>& op) {
        auto it = Tnsr<T>::elem_begin(dst), it_end = Tnsr<T>::elem_end(dst);
        for (; it != it_end; ++it) {
            auto &x = *it;
            x = op(x);
        }
        return dst;
    }

    template<template<typename>class Tnsr, typename T>
    Tnsr<T> &_apply_scalar_(Tnsr<T> &dst, T scalar, const binary_op<T>& op) {
        auto it = Tnsr<T>::elem_begin(dst), it_end = Tnsr<T>::elem_end(dst);
        for (; it != it_end; ++it) {
            auto& x = *it;
            x = op(x, scalar);
        }
        return dst;
    }

    template<template<typename> class TnsrSrc,template<typename> class TnsrDst, typename T>
    void _apply_unary(const TnsrSrc<T> &src, const unary_op<T>& op, TnsrDst<T> &dst) {
        using src_it_t = typename TnsrSrc<T>::ceiterator;
        using dst_it_t = typename TnsrDst<T>::eiterator;
        src_it_t src_it = TnsrSrc<T>::const_elem_begin(src), src_it_end = TnsrSrc<T>::const_elem_end(src);
        dst_it_t dst_it = TnsrDst<T>::elem_begin(dst);
        while(src_it != src_it_end) {
            *dst_it = op(*src_it);
            ++dst_it; ++src_it;
        }
    }

    template<template<typename> class TnsrSrc,template<typename> class TnsrDst, typename T>
    void _apply_scalar(const TnsrSrc<T> &src, T scalar, const binary_op<T>& op, TnsrDst<T> &dst) {
        using src_it_t = typename TnsrSrc<T>::ceiterator;
        using dst_it_t = typename TnsrDst<T>::eiterator;
        src_it_t src_it = TnsrSrc<T>::const_elem_begin(src), src_it_end = TnsrSrc<T>::const_elem_end(src);
        dst_it_t dst_it = TnsrDst<T>::elem_begin(dst);
        while(src_it != src_it_end){
            *dst_it = op(*src_it, scalar);
            ++dst_it; ++src_it;
        }
    }

    /**
     * Applies a unary operation.
     * @tparam Tnsr
     * @tparam T
     * @param dst
     * @param op
     * @return
     */
    template<template<typename> class Tnsr, typename T>
    inline Tensor<T> _apply_unary(const Tnsr<T> &src, const unary_op<T>& op) {
        Tensor<T> dst(src.shape);
        _apply_unary(src, op, dst);
        return dst;
    }

    /**
     * Applies a binary operation with another constant operand.
     * @tparam Tnsr
     * @tparam T
     * @param src
     * @param scalar
     * @param op
     * @return
     */
    template<template<typename> class Tnsr, typename T>
    inline Tensor<T> _apply_scalar(const Tnsr<T> &src, T scalar, const binary_op<T>& op) {
        Tensor<T> dst(src.shape);
        _apply_scalar(src, scalar, op, dst);
        return dst;
    }

    std::pair<size_t, size_t> ravel_index_checked(const index_t &idx, const shape_t &shape, int size) {
        if (idx.size() > shape.size())
            throw std::out_of_range("vector index bigger than shape.");
        index_t idx_(shape.size());
        size_t elems = 1;
        for (int i=0; i < idx.size(); ++i)
            idx_[i] = normalize_index(idx[i], shape[i]);
        for (int i = idx.size(); i < shape.size(); ++i) {
            idx_[i] = 0;
            elems *= shape[i];
        }
        size_t true_idx = ravel_index(idx_, shape, size);
        return {true_idx, elems};
    }

    SliceGroup broadcast_index(const index_t &src_idx, const shape_t &src_shape, const shape_t &dst_shape) {
        size_t dst_size = dst_shape.size(), src_size = src_shape.size();
        SliceGroup res = SliceGroup::cover_shape(dst_shape);
        for (int i = 0; i < dst_size ; ++i){
            size_t dsts_idx = dst_size - 1 - i;
            size_t srcs_idx = src_size - 1 - i;
            Slice& slice = res.slices[dsts_idx];
            if (i >= src_size || src_shape[srcs_idx] == 1)
                slice.e = dst_shape[dsts_idx];
            else if (dst_shape[dsts_idx] > 1){
                auto s1 = src_idx[srcs_idx];
                slice.b = s1;
                slice.e = s1 + 1;
            }
            slice.update();
        }
        return res;
    }


    template<typename T>
    TensorView<T> Tensor<T>::const_view(const vector<long> &new_shape) const {
        shape_t normalized = normalize_shape(new_shape, shape);
        return TensorView<T>(data, normalized);
    }


    template<typename T>
    TensorSliced<T> TensorSliced<T>::slice_unsqueeze(int i) {
        long norm_i = normalize_index(i, this->shape.size(), true);
        TensorSliced ret(*this);
        auto& ret_slices = ret.slice_group.slices;
        ret_slices.emplace(ret_slices.begin() + norm_i, 0, 1);
        ret.slice_group.update();
        auto& ret_shape = ret.shape;
        ret_shape.emplace(ret_shape.begin() + norm_i, 1);
        return ret;
    }

    template<typename T>
    TensorSliced<T> TensorSliced<T>::const_slice_unsqueeze(int i) const {
        long norm_i = normalize_index(i, this->shape.size(), true);
        TensorSliced ret(*this);
        auto& ret_slices = ret.slice_group.slices;
        ret_slices.emplace(ret_slices.begin() + norm_i, 0, 1);
        ret.slice_group.update();
        auto& ret_shape = ret.shape;
        ret_shape.emplace(ret_shape.begin() + norm_i, 1);
        return ret;
    }


#define TENSOR_UNARY_APPLY_(Tnsr) \
    template<typename T>\
    Tnsr<T>& Tnsr<T>::apply_(const unary_op<T>& op) {\
        return _apply_unary_(*this, op); \
    }

#define TENSOR_UNARY_APPLY(Tnsr) \
    template<typename T>\
    void Tnsr<T>::apply(const unary_op<T>& op, Tensor<T>&dst) const {\
        _apply_unary(*this, op, dst); \
    } \
    template<typename T>\
    void Tnsr<T>::apply(const unary_op<T>& op, TensorView<T>&dst) const {\
        _apply_unary(*this, op, dst); \
    } \
    template<typename T>\
    void Tnsr<T>::apply(const unary_op<T>& op, TensorSliced<T>&dst) const {\
        _apply_unary(*this, op, dst); \
    } \
    template<typename T>\
    Tensor<T> Tnsr<T>::apply(const unary_op<T>& op) const {\
        return _apply_unary(*this, op); \
    }

#define TENSOR_SCALAR_BINARY_APPLY_(Tnsr) \
    template<typename T>\
    Tnsr<T>& Tnsr<T>::apply_(T scalar, const binary_op<T>& op) {\
        return _apply_scalar_(*this, scalar, op); \
    }

#define TENSOR_SCALAR_BINARY_APPLY(Tnsr) \
    template<typename T>\
    void Tnsr<T>::apply(T scalar, const binary_op<T>& op, Tensor<T>&dst) const {\
        _apply_scalar(*this, scalar, op, dst); \
    } \
    template<typename T>\
    void Tnsr<T>::apply(T scalar, const binary_op<T>& op, TensorView<T>&dst) const {\
        _apply_scalar(*this, scalar, op, dst); \
    } \
    template<typename T>\
    void Tnsr<T>::apply(T scalar, const binary_op<T>& op, TensorSliced<T>&dst) const {\
        _apply_scalar(*this, scalar, op, dst); \
    } \
    template<typename T>\
    Tensor<T> Tnsr<T>::apply(T scalar, const binary_op<T>& op) const {\
        return _apply_scalar(*this, scalar, op); \
    }

#define TENSOR_TENSOR_BINARY_APPLY_(TensorT1, TensorT2, T) \
    template<typename T>\
    TensorT1<T>& TensorT1<T>::apply_tensors_(TensorT2<T> t, const binary_op<T>& op) {\
        return _apply_tensors_(*this, t, op); \
    }

#define TENSOR_TENSOR_BINARY_APPLY(TensorT1, TensorT2, T) \
    template<typename T>\
    void TensorT1<T>::apply_tensors(TensorT2<T> t, const binary_op<T>& op, Tensor<T>&dst) const {\
        _apply_tensors(*this, t, op, dst); \
    } \
    template<typename T>\
    void TensorT1<T>::apply_tensors(TensorT2<T> t, const binary_op<T>& op, TensorView<T>&dst) const {\
        _apply_tensors(*this, t, op, dst); \
    } \
    template<typename T>\
    void TensorT1<T>::apply_tensors(TensorT2<T> t, const binary_op<T>& op, TensorSliced<T>&dst) const {\
        _apply_tensors(*this, t, op, dst); \
    } \
    template<typename T>\
    Tensor<T> TensorT1<T>::apply_tensors(TensorT2<T> t, const binary_op<T>& op) const {\
        return _apply_tensors(*this, t, op); \
    }

#define APPLY_UNIQUE_INTERACTABLE(Tnsr) \
    TENSOR_UNARY_APPLY_(Tnsr) \
    TENSOR_UNARY_APPLY(Tnsr) \
    TENSOR_SCALAR_BINARY_APPLY_(Tnsr) \
    TENSOR_SCALAR_BINARY_APPLY(Tnsr)

#define APPLY_TENSOR_INTERACTABLE(TensorT1, TensorT2, T) \
    TENSOR_TENSOR_BINARY_APPLY_(TensorT1, TensorT2, T) \
    TENSOR_TENSOR_BINARY_APPLY(TensorT1, TensorT2, T)

#define DEF_APPLY_TENSOR(Tnsr) \
    MACRO_INTERACTABLE_TENSORTYPES(APPLY_TENSOR_INTERACTABLE, Tnsr, T) \
    APPLY_UNIQUE_INTERACTABLE(Tnsr)

    DEF_APPLY_TENSOR(Tensor)
    APPLY_UNIQUE_INTERACTABLE(TensorView) // We only need this because the others are inherited no problem.
    DEF_APPLY_TENSOR(TensorSliced)


#define MACRO_COPY_(TensorDst, TensorSrc, T) \
    template TensorDst<T>& copy_(TensorDst<T>&, const TensorSrc<T>&);

#define INSTANTIATE_COPY_(T) \
    MACRO_INTERACTABLE_TENSORTYPES(MACRO_COPY_, Tensor, T) \
    MACRO_INTERACTABLE_TENSORTYPES(MACRO_COPY_, TensorView, T) \
    MACRO_INTERACTABLE_TENSORTYPES(MACRO_COPY_, TensorSliced, T)

#define INSTANTIATE_TEMPLATE_TENSOR(T) \
    template class Tensor<T>; \
    template class TensorView<T>; \
    template class TensorSliced<T>; \
    template Tensor<T>& fill_(Tensor<T>&, T); \
    template TensorView<T>& fill_(TensorView<T>&, T); \
    template TensorSliced<T>& fill_(TensorSliced<T>&, T); \
    INSTANTIATE_COPY_(T)

    INSTANTIATE_TEMPLATE_TENSOR(double)
    INSTANTIATE_TEMPLATE_TENSOR(float)
    INSTANTIATE_TEMPLATE_TENSOR(long)
}