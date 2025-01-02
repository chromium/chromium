// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <algorithm>
#include <cstddef>

#include <gsl/gsl>

namespace onnxruntime {

// AsSpan inspired by Fekir's Blog https://fekir.info/post/span-the-missing-constructor/
// Used under MIT license

// Use AsSpan for less typing on any container including initializer list to create a span
// (unnamed, untyped initializer list does not automatically convert to gsl::span).
// {1, 2, 3} as such does not have a type
// (see https://scottmeyers.blogspot.com/2014/03/if-braced-initializers-have-no-type-why.html)
//
//   Example: AsSpan({1, 2, 3}) results in gsl::span<const int>
//
// The above would deduce to std::initializer_list<int> and the result is gsl::span<const int>
//
// AsSpan<int64_t>({1, 2, 3}) produces gsl::span<const int64_t>
//
// We can also do std::array<int64_t, 3>{1, 2, 3} that can be automatically converted to span
// without memory allocation.
//
// If type conversion is not required, then for C++17 std::array template parameters are
// auto-deduced. Example: std::array{1, 2, 3}.
// We are aiming at not allocating memory dynamically.

namespace details {
template <class P>
constexpr auto AsSpanImpl(P* p, size_t s) {
  return gsl::span<P>(p, s);
}
}  // namespace details

template <class C>
constexpr auto AsSpan(C& c) {
  return details::AsSpanImpl(c.data(), c.size());
}

template <class C>
constexpr auto AsSpan(const C& c) {
  return details::AsSpanImpl(c.data(), c.size());
}

template <class C>
constexpr auto AsSpan(C&& c) {
  return details::AsSpanImpl(c.data(), c.size());
}

template <class T>
constexpr auto AsSpan(std::initializer_list<T> c) {
  return details::AsSpanImpl(c.begin(), c.size());
}

template <class T, size_t N>
constexpr auto AsSpan(T (&arr)[N]) {
  return details::AsSpanImpl(arr, N);
}

template <class T, size_t N>
constexpr auto AsSpan(const T (&arr)[N]) {
  return details::AsSpanImpl(arr, N);
}

template <class T>
inline gsl::span<const T> EmptySpan() { return gsl::span<const T>(); }

template <class U, class T>
[[nodiscard]] inline gsl::span<U> ReinterpretAsSpan(gsl::span<T> src) {
  // adapted from gsl-lite span::as_span():
  // https://github.com/gsl-lite/gsl-lite/blob/4720a2980a30da085b4ddb4a0ea2a71af7351a48/include/gsl/gsl-lite.hpp#L4102-L4108
  Expects(src.size_bytes() % sizeof(U) == 0);
  return gsl::span<U>(reinterpret_cast<U*>(src.data()), src.size_bytes() / sizeof(U));
}

[[nodiscard]] inline gsl::span<const std::byte> AsByteSpan(const void* data, size_t length) {
  return gsl::span<const std::byte>(reinterpret_cast<const std::byte*>(data), length);
}

template <class T1, size_t Extent1, class T2, size_t Extent2>
[[nodiscard]] inline bool SpanEq(gsl::span<T1, Extent1> a, gsl::span<T2, Extent2> b) {
  static_assert(std::is_same_v<std::remove_const_t<T1>, std::remove_const_t<T2>>,
                "T1 and T2 should be the same type except for const qualification");
  return std::equal(a.begin(), a.end(), b.begin(), b.end());
}

}  // namespace onnxruntime
