/* Copyright 2024 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#ifndef ML_DTYPES_MXFLOAT_H_
#define ML_DTYPES_MXFLOAT_H_

// Microscaling (MX) floating point formats, as described in
//   https://www.opencompute.org/documents/ocp-microscaling-formats-mx-v1-0-spec-final-pdf
//
// Note: this implements the underlying raw data types (e.g. E2M1FN), not the
// composite types (e.g. MXFP4).

#include <cstdint>
#include <limits>

#include "ml_dtypes/include/float8.h"
#include "Eigen/Core"

namespace ml_dtypes {
namespace mxfloat_internal {

// Use 8-bit storage for 6-bit and 4-bit types.
template <typename Derived>
class mxfloat6_base : public float8_internal::float8_base<Derived> {
  using Base = float8_internal::float8_base<Derived>;
  friend class float8_internal::float8_base<Derived>;
  using Base::Base;

 public:
  static constexpr int kBits = 6;

  explicit EIGEN_DEVICE_FUNC operator bool() const {
    return (Base::rep() & 0x1F) != 0;
  }
  constexpr Derived operator-() const {
    return Derived::FromRep(Base::rep() ^ 0x20);
  }
  Derived operator-(const Derived& other) const {
    return Base::operator-(other);
  }
};

template <typename Derived>
class mxfloat4_base : public float8_internal::float8_base<Derived> {
  using Base = float8_internal::float8_base<Derived>;
  friend class float8_internal::float8_base<Derived>;
  using Base::Base;

 public:
  static constexpr int kBits = 4;

  explicit EIGEN_DEVICE_FUNC operator bool() const {
    return (Base::rep() & 0x07) != 0;
  }
  constexpr Derived operator-() const {
    return Derived::FromRep(Base::rep() ^ 0x08);
  }
  Derived operator-(const Derived& other) const {
    return Base::operator-(other);
  }
};

class float6_e2m3fn : public mxfloat6_base<float6_e2m3fn> {
  // Exponent: 2, Mantissa: 3, bias: 1.
  // Extended range: no inf, no NaN.
  using Base = mxfloat6_base<float6_e2m3fn>;
  friend class float8_internal::float8_base<float6_e2m3fn>;
  using Base::Base;

 public:
  template <typename T, float8_internal::RequiresIsDerivedFromFloat8Base<T> = 0>
  explicit EIGEN_DEVICE_FUNC float6_e2m3fn(T f8)
      : float6_e2m3fn(ConvertFrom(f8)) {}
};

class float6_e3m2fn : public mxfloat6_base<float6_e3m2fn> {
  // Exponent: 3, Mantissa: 2, bias: 3.
  // Extended range: no inf, no NaN.
  using Base = mxfloat6_base<float6_e3m2fn>;
  friend class float8_internal::float8_base<float6_e3m2fn>;
  using Base::Base;

 public:
  template <typename T, float8_internal::RequiresIsDerivedFromFloat8Base<T> = 0>
  explicit EIGEN_DEVICE_FUNC float6_e3m2fn(T f8)
      : float6_e3m2fn(ConvertFrom(f8)) {}
};

class float4_e2m1fn : public mxfloat4_base<float4_e2m1fn> {
  // Exponent: 2, Mantissa: 1, bias: 1.
  // Extended range: no inf, no NaN.
  using Base = mxfloat4_base<float4_e2m1fn>;
  friend class float8_internal::float8_base<float4_e2m1fn>;
  using Base::Base;

 public:
  template <typename T, float8_internal::RequiresIsDerivedFromFloat8Base<T> = 0>
  explicit EIGEN_DEVICE_FUNC float4_e2m1fn(T f8)
      : float4_e2m1fn(ConvertFrom(f8)) {}
};

// Common properties for specializing std::numeric_limits.
template <int E, int M>
struct numeric_limits_mxfloat_tpl {
 protected:
  static constexpr int kExponentBias = (1 << (E - 1)) - 1;
  static constexpr int kMantissaBits = M;

 public:
  // NOLINTBEGIN: these names must match std::numeric_limits.
  static constexpr bool is_specialized = true;
  static constexpr bool is_signed = true;
  static constexpr bool is_integer = false;
  static constexpr bool is_exact = false;
  static constexpr bool has_infinity = false;
  static constexpr bool has_quiet_NaN = false;
  static constexpr bool has_signaling_NaN = false;
  static constexpr std::float_denorm_style has_denorm = std::denorm_present;
  static constexpr bool has_denorm_loss = false;
  static constexpr std::float_round_style round_style = std::round_to_nearest;
  static constexpr bool is_iec559 = false;
  static constexpr bool is_bounded = true;
  static constexpr bool is_modulo = false;
  static constexpr int digits = kMantissaBits + 1;
  static constexpr int digits10 = float8_internal::Digits10FromDigits(digits);
  static constexpr int max_digits10 =
      float8_internal::MaxDigits10FromDigits(digits);
  static constexpr int radix = std::numeric_limits<float>::radix;
  static constexpr int min_exponent = (1 - kExponentBias) + 1;
  static constexpr int min_exponent10 =
      float8_internal::MinExponent10FromMinExponent(min_exponent);
  static constexpr int max_exponent = kExponentBias + 2;
  static constexpr int max_exponent10 =
      float8_internal::MaxExponent10FromMaxExponentAndDigits(max_exponent,
                                                             digits);
  static constexpr bool traps = std::numeric_limits<float>::traps;
  static constexpr bool tinyness_before =
      std::numeric_limits<float>::tinyness_before;
  // NOLINTEND
};

struct numeric_limits_float6_e2m3fn : public numeric_limits_mxfloat_tpl<2, 3> {
  // 1.0 * 2^(0) = 1
  static constexpr float6_e2m3fn min() {
    return float6_e2m3fn::FromRep(0b0'01'000);
  }
  // -1.875 * 2^(2) = -7.5
  static constexpr float6_e2m3fn lowest() {
    return float6_e2m3fn::FromRep(0b1'11'111);
  }
  // 1.875 * 2^(2) = 7.5
  static constexpr float6_e2m3fn max() {
    return float6_e2m3fn::FromRep(0b0'11'111);
  }
  // 0.125 * 2^(0) = 0.125
  static constexpr float6_e2m3fn epsilon() {
    return float6_e2m3fn::FromRep(0b0'00'001);
  }
  // 0.25 * 2^(0) = 0.25
  static constexpr float6_e2m3fn round_error() {
    return float6_e2m3fn::FromRep(0b0'00'010);
  }
  // 0.25 * 2^(0) = 0.125
  static constexpr float6_e2m3fn denorm_min() {
    return float6_e2m3fn::FromRep(0b0'00'001);
  }

  // Conversion from NaNs is implementation-defined (by MX specification).
  static constexpr float6_e2m3fn quiet_NaN() {
    return float6_e2m3fn::FromRep(0b1'00'000);
  }
  static constexpr float6_e2m3fn signaling_NaN() {
    return float6_e2m3fn::FromRep(0b1'00'000);
  }
  static constexpr float6_e2m3fn infinity() {
    return float6_e2m3fn::FromRep(0b0'11'111);
  }
};

struct numeric_limits_float6_e3m2fn : public numeric_limits_mxfloat_tpl<3, 2> {
  // 1.0 * 2^(-2) = 0.25
  static constexpr float6_e3m2fn min() {
    return float6_e3m2fn::FromRep(0b0'001'00);
  }
  // -1.75 * 2^(4) = -28
  static constexpr float6_e3m2fn lowest() {
    return float6_e3m2fn::FromRep(0b1'111'11);
  }
  // 1.75 * 2^(4) = 28
  static constexpr float6_e3m2fn max() {
    return float6_e3m2fn::FromRep(0b0'111'11);
  }
  // 1.0 * 2^(-2) = 0.25
  static constexpr float6_e3m2fn epsilon() {
    return float6_e3m2fn::FromRep(0b0'001'00);
  }
  // 1.0 * 2^(0) = 1
  static constexpr float6_e3m2fn round_error() {
    return float6_e3m2fn::FromRep(0b0'011'00);
  }
  // 0.25 * 2^(-2) = 0.0625
  static constexpr float6_e3m2fn denorm_min() {
    return float6_e3m2fn::FromRep(0b0'000'01);
  }

  // Conversion from NaNs is implementation-defined (by MX specification).
  static constexpr float6_e3m2fn quiet_NaN() {
    return float6_e3m2fn::FromRep(0b1'000'00);
  }
  static constexpr float6_e3m2fn signaling_NaN() {
    return float6_e3m2fn::FromRep(0b1'000'00);
  }
  static constexpr float6_e3m2fn infinity() {
    return float6_e3m2fn::FromRep(0b0'111'11);
  }
};

struct numeric_limits_float4_e2m1fn : public numeric_limits_mxfloat_tpl<2, 1> {
  // 1.0 * 2^(0) = 1
  static constexpr float4_e2m1fn min() {
    return float4_e2m1fn::FromRep(0b0'01'0);
  }
  // -1.5 * 2^(2) = -6
  static constexpr float4_e2m1fn lowest() {
    return float4_e2m1fn::FromRep(0b1'11'1);
  }
  // 1.5 * 2^(2) = 6
  static constexpr float4_e2m1fn max() {
    return float4_e2m1fn::FromRep(0b0'11'1);
  }
  // 0.5 * 2^(0) = 0.5
  static constexpr float4_e2m1fn epsilon() {
    return float4_e2m1fn::FromRep(0b0'00'1);
  }
  // 1.0 * 2^(0) = 1
  static constexpr float4_e2m1fn round_error() {
    return float4_e2m1fn::FromRep(0b0'01'0);
  }
  // 0.5 * 2^(0) = 0.5
  static constexpr float4_e2m1fn denorm_min() {
    return float4_e2m1fn::FromRep(0b0'00'1);
  }

  // Conversion from NaNs is implementation-defined (by MX specification).
  static constexpr float4_e2m1fn quiet_NaN() {
    return float4_e2m1fn::FromRep(0b1'00'0);
  }
  static constexpr float4_e2m1fn signaling_NaN() {
    return float4_e2m1fn::FromRep(0b1'00'0);
  }
  static constexpr float4_e2m1fn infinity() {
    return float4_e2m1fn::FromRep(0b0'11'1);
  }
};

// Free-functions for use with ADL and in Eigen.
constexpr inline float6_e2m3fn abs(const float6_e2m3fn& a) {
  return float6_e2m3fn::FromRep(a.rep() & 0b0'11'111);
}

constexpr inline bool(isnan)(const float6_e2m3fn& a) { return false; }

constexpr inline float6_e3m2fn abs(const float6_e3m2fn& a) {
  return float6_e3m2fn::FromRep(a.rep() & 0b0'111'11);
}

constexpr inline bool(isnan)(const float6_e3m2fn& a) { return false; }

constexpr inline float4_e2m1fn abs(const float4_e2m1fn& a) {
  return float4_e2m1fn::FromRep(a.rep() & 0b0'11'1);
}

constexpr inline bool(isnan)(const float4_e2m1fn& a) { return false; }

// Define traits required for floating point conversion.
template <typename T, int E, int M>
struct TraitsBase : public float8_internal::TraitsBase<T> {
  static constexpr int kBits = E + M + 1;
  static constexpr int kMantissaBits = M;
  static constexpr int kExponentBits = E;
  static constexpr int kExponentBias = (1 << (E - 1)) - 1;
  static constexpr uint8_t kExponentMask = ((1 << E) - 1) << M;
};

}  // namespace mxfloat_internal

// Exported types.
using float6_e2m3fn = mxfloat_internal::float6_e2m3fn;
using float6_e3m2fn = mxfloat_internal::float6_e3m2fn;
using float4_e2m1fn = mxfloat_internal::float4_e2m1fn;

}  // namespace ml_dtypes

// Standard library overrides.
namespace std {

template <>
struct numeric_limits<ml_dtypes::mxfloat_internal::float6_e2m3fn>
    : public ml_dtypes::mxfloat_internal::numeric_limits_float6_e2m3fn {};

template <>
struct numeric_limits<ml_dtypes::mxfloat_internal::float6_e3m2fn>
    : public ml_dtypes::mxfloat_internal::numeric_limits_float6_e3m2fn {};

template <>
struct numeric_limits<ml_dtypes::mxfloat_internal::float4_e2m1fn>
    : public ml_dtypes::mxfloat_internal::numeric_limits_float4_e2m1fn {};

}  // namespace std

// Conversion traits.
namespace ml_dtypes {
namespace float8_internal {

template <>
struct Traits<float6_e2m3fn>
    : public mxfloat_internal::TraitsBase<float6_e2m3fn, 2, 3> {};

template <>
struct Traits<float6_e3m2fn>
    : public mxfloat_internal::TraitsBase<float6_e3m2fn, 3, 2> {};

template <>
struct Traits<float4_e2m1fn>
    : public mxfloat_internal::TraitsBase<float4_e2m1fn, 2, 1> {};

}  // namespace float8_internal
}  // namespace ml_dtypes

// Eigen library overrides.
namespace Eigen {
namespace numext {

#define MXFLOAT_EIGEN_SIGNBIT_IMPL(Type)                              \
  EIGEN_DEVICE_FUNC EIGEN_ALWAYS_INLINE Type signbit(const Type& x) { \
    int8_t t = bit_cast<int8_t, Type>(x) << (8 - Type::kBits);        \
    return bit_cast<Type, int8_t>(t >> 7);                            \
  }

MXFLOAT_EIGEN_SIGNBIT_IMPL(ml_dtypes::float6_e2m3fn)
MXFLOAT_EIGEN_SIGNBIT_IMPL(ml_dtypes::float6_e3m2fn)
MXFLOAT_EIGEN_SIGNBIT_IMPL(ml_dtypes::float4_e2m1fn)

#undef MXFLOAT_EIGEN_SIGNBIT_IMPL

}  // namespace numext

// Work-around for isinf/isnan/isfinite issue on aarch64.
namespace internal {

#define MXFLOAT_EIGEN_ISFINITE_IMPL(Type)                          \
  template <>                                                      \
  EIGEN_DEVICE_FUNC inline bool isinf_impl<Type>(const Type&) {    \
    return false;                                                  \
  }                                                                \
  template <>                                                      \
  EIGEN_DEVICE_FUNC inline bool isnan_impl<Type>(const Type&) {    \
    return false;                                                  \
  }                                                                \
  template <>                                                      \
  EIGEN_DEVICE_FUNC inline bool isfinite_impl<Type>(const Type&) { \
    return true;                                                   \
  }

MXFLOAT_EIGEN_ISFINITE_IMPL(ml_dtypes::float6_e2m3fn)
MXFLOAT_EIGEN_ISFINITE_IMPL(ml_dtypes::float6_e3m2fn)
MXFLOAT_EIGEN_ISFINITE_IMPL(ml_dtypes::float4_e2m1fn)

#undef MXFLOAT_EIGEN_ISFINITE_IMPL

}  // namespace internal
}  // namespace Eigen

#endif  // ML_DTYPES_MXFLOAT_H_
