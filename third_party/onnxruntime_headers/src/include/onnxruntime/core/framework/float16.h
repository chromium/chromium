// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <math.h>

#include "endian.h"
#if defined(CUDA_VERSION) && CUDA_VERSION >= 11000
#include "cuda_bf16.h"
#endif

#if !defined(__CUDACC__) && !defined(__HIPCC__)
#include "core/common/narrow.h"
#endif

#include "core/common/common.h"

#include "core/session/onnxruntime_float16.h"

namespace onnxruntime {

#if defined(__CUDACC__) || defined(__HIPCC__)
#define ORT_HOST_DEVICE __host__ __device__
#else
#define ORT_HOST_DEVICE
#endif

// MLFloat16
struct MLFloat16 : onnxruntime_float16::Float16Impl<MLFloat16> {
 private:
  explicit constexpr MLFloat16(uint16_t x) noexcept { val = x; }

 public:
  using Base = onnxruntime_float16::Float16Impl<MLFloat16>;

  MLFloat16() = default;

  constexpr static MLFloat16 FromBits(uint16_t x) noexcept { return MLFloat16(x); }

  // Using inherited implementation instead of math floatToHalf allows us to use this
  // in other shared providers without having to implement the bridge
  explicit MLFloat16(float v) noexcept { val = Base::ToUint16Impl(v); }

  static const MLFloat16 NaN;
  static const MLFloat16 NegativeNaN;
  static const MLFloat16 Infinity;
  static const MLFloat16 NegativeInfinity;
  static const MLFloat16 MaxValue;
  static const MLFloat16 Zero;
  static const MLFloat16 One;
  static const MLFloat16 MinusOne;

  // Using inherited implementation instead of math halfToFloat allows us to use this
  // in other shared providers without having to implement the bridge
  float ToFloat() const noexcept { return Base::ToFloatImpl(); }

  using Base::IsNegative;

  using Base::IsNaN;

  using Base::IsFinite;

  using Base::IsPositiveInfinity;

  using Base::IsNegativeInfinity;

  using Base::IsInfinity;

  using Base::IsNaNOrZero;

  using Base::IsNormal;

  using Base::IsSubnormal;

  using Base::Abs;

  using Base::Negate;

  operator float() const noexcept { return ToFloat(); }

  using Base::operator==;
  using Base::operator!=;
  using Base::operator<;
};

// BFloat16
struct BFloat16 : onnxruntime_float16::BFloat16Impl<BFloat16> {
  using Base = onnxruntime_float16::BFloat16Impl<BFloat16>;

#if defined(__HIP__)
  ORT_HOST_DEVICE BFloat16() = default;
#else
  BFloat16() = default;
#endif

  struct FromBitsT {};
  static constexpr ORT_HOST_DEVICE FromBitsT FromBits() noexcept { return FromBitsT(); }
  constexpr ORT_HOST_DEVICE BFloat16(unsigned short bits, FromBitsT) noexcept { val = bits; }

  static constexpr ORT_HOST_DEVICE BFloat16 FromBits(uint16_t bits) noexcept {
    return BFloat16(bits, FromBits());
  }

  inline ORT_HOST_DEVICE BFloat16(float v) noexcept {
#if defined(CUDA_VERSION) && CUDA_VERSION >= 11000 && defined(__CUDA_ARCH__) && __CUDA_ARCH__ >= 800
    val = __bfloat16_as_ushort(__float2bfloat16(v));
#elif defined(__HIP__)
    // We should be using memcpy in order to respect the strict aliasing rule but it fails in the HIP environment.
    if (v != v) {  // isnan
      val = UINT16_C(0x7FC0);
    } else {
      union {
        uint32_t U32;
        float F32;
      };

      F32 = v;
      uint32_t rounding_bias = ((U32 >> 16) & 1) + UINT32_C(0x7FFF);
      val = static_cast<uint16_t>((U32 + rounding_bias) >> 16);
    }
#else

    // Use C isnan to work both in host and device
    if (::isnan(v)) {
      val = kPositiveQNaNBits;
    } else {
      auto get_msb_half = [](float fl) {
        uint16_t result;
        if constexpr (onnxruntime_float16::detail::endian::native == onnxruntime_float16::detail::endian::little) {
          std::memcpy(&result, reinterpret_cast<char*>(&fl) + sizeof(uint16_t), sizeof(uint16_t));
        } else {
          std::memcpy(&result, &fl, sizeof(uint16_t));
        }
        return result;
      };

      uint16_t upper_bits = get_msb_half(v);
      union {
        uint32_t U32;
        float F32;
      };
      F32 = v;
      U32 += (upper_bits & 1) + kRoundToNearest;
      val = get_msb_half(F32);
    }
#endif
  }

  inline ORT_HOST_DEVICE float ToFloat() const noexcept {
#if defined(CUDA_VERSION) && CUDA_VERSION >= 11000
    return __bfloat162float(*reinterpret_cast<const __nv_bfloat16*>(&val));
#elif defined(__HIP__)
    // We should be using memcpy in order to respect the strict aliasing rule but it fails in the HIP environment.
    float result = 0;
    uint32_t tmp = val;
    tmp <<= 16;
    float* tempRes = reinterpret_cast<float*>(&tmp);
    result = *tempRes;
    return result;
#else

    if (IsNaNHostDevice()) {
      return std::numeric_limits<float>::quiet_NaN();
    }

    float result = 0;
    char* const first = reinterpret_cast<char*>(&result);
    if constexpr (endian::native == endian::little) {
      char* const second = first + sizeof(uint16_t);
      std::memcpy(second, &val, sizeof(uint16_t));
    } else {
      std::memcpy(first, &val, sizeof(uint16_t));
    }
    return result;
#endif
  }

  static const BFloat16 NaN;
  static const BFloat16 NegativeNaN;
  static const BFloat16 Infinity;
  static const BFloat16 NegativeInfinity;
  static const BFloat16 MaxValue;
  static const BFloat16 Zero;
  static const BFloat16 One;
  static const BFloat16 MinusOne;

  using Base::IsNegative;

  using Base::IsNaN;

  using Base::IsFinite;

  using Base::IsPositiveInfinity;

  using Base::IsNegativeInfinity;

  using Base::IsInfinity;

  using Base::IsNaNOrZero;

  using Base::IsNormal;

  using Base::IsSubnormal;

  using Base::Abs;

  using Base::Negate;

  ORT_HOST_DEVICE operator float() const noexcept { return ToFloat(); }

#if defined(CUDA_VERSION) && CUDA_VERSION >= 11000
  ORT_HOST_DEVICE BFloat16(const __nv_bfloat16& value) { val = *reinterpret_cast<const unsigned short*>(&value); }
  explicit ORT_HOST_DEVICE operator __nv_bfloat16() const { return *reinterpret_cast<const __nv_bfloat16*>(&val); }
#endif

  ORT_HOST_DEVICE bool operator==(const BFloat16& rhs) const noexcept {
    if (IsNaNHostDevice() || rhs.IsNaNHostDevice()) {
      // IEEE defines that NaN is not equal to anything, including itself.
      return false;
    }
    return val == rhs.val;
  }

  ORT_HOST_DEVICE bool operator!=(const BFloat16& rhs) const noexcept {
    return !(*this == rhs);
  }

  ORT_HOST_DEVICE bool operator<(const BFloat16& rhs) const noexcept {
    if (IsNaNHostDevice() || rhs.IsNaNHostDevice()) {
      // IEEE defines that NaN is unordered with respect to everything, including itself.
      return false;
    }

    const bool left_is_negative = IsNegativeHostDevice();
    if (left_is_negative != rhs.IsNegativeHostDevice()) {
      // When the signs of left and right differ, we know that left is less than right if it is
      // the negative value. The exception to this is if both values are zero, in which case IEEE
      // says they should be equal, even if the signs differ.
      return left_is_negative && !AreZeroHostDevice(*this, rhs);
    }
    return (val != rhs.val) && ((val < rhs.val) ^ left_is_negative);
  }

  ORT_HOST_DEVICE bool IsNegativeHostDevice() const noexcept {
    return (val & kSignMask) != 0;
  }

  ORT_HOST_DEVICE bool IsNaNHostDevice() const noexcept {
    return static_cast<uint16_t>(val & ~kSignMask) > kPositiveInfinityBits;
  }

  ORT_HOST_DEVICE static bool AreZeroHostDevice(const BFloat16Impl& lhs, const BFloat16Impl& rhs) noexcept {
    // IEEE defines that positive and negative zero are equal, this gives us a quick equality check
    // for two values by or'ing the private bits together and stripping the sign. They are both zero,
    // and therefore equivalent, if the resulting value is still zero.
    return static_cast<uint16_t>((lhs.val | rhs.val) & ~kSignMask) == 0;
  }
};

// User defined suffixes to make it easier to declare
// initializers with MLFloat16 and BFloat16 from unsigned short
// E.g 10_f16 or 10_b16
#if !defined(__CUDACC__) && !defined(__HIPCC__)
inline MLFloat16 operator"" _f16(unsigned long long int v) noexcept {
  return MLFloat16::FromBits(narrow<uint16_t>(v));
}

inline MLFloat16 operator"" _fp16(long double v) noexcept {
  return MLFloat16(static_cast<float>(v));
}

inline BFloat16 operator"" _b16(unsigned long long int v) noexcept {
  return BFloat16::FromBits((narrow<uint16_t>(v)));
}

inline BFloat16 operator"" _bfp16(long double v) noexcept {
  return BFloat16(static_cast<float>(v));
}
#endif

inline void BFloat16ToFloat(const BFloat16* blf, float* flt, size_t size) noexcept {
  auto src = blf;
  auto d = flt;
  for (; size != 0; ++src, ++d, --size) {
    *d = src->ToFloat();
  }
}

inline void FloatToBFloat16(const float* flt, BFloat16* blf, size_t size) {
  auto src = flt;
  auto d = blf;
  for (; size != 0; ++src, ++d, --size) {
    *d = BFloat16(*src);
  }
}

}  // namespace onnxruntime

namespace std {

template <>
class numeric_limits<onnxruntime::MLFloat16> {
 public:
  static constexpr onnxruntime::MLFloat16 min() noexcept {
    return onnxruntime::MLFloat16::FromBits(0x0400U);  // Minimum positive normalized value: 0.00006103515625
  }

  static constexpr onnxruntime::MLFloat16 max() noexcept {
    return onnxruntime::MLFloat16::FromBits(0x7BFFU);  // Largest representable value: 65504
  }

  static constexpr onnxruntime::MLFloat16 lowest() noexcept {
    return onnxruntime::MLFloat16::FromBits(0xFBFFU);  // Smallest representable value: -65504
  }

  static constexpr onnxruntime::MLFloat16 infinity() noexcept {
    return onnxruntime::MLFloat16::FromBits(0x7C00U);  // Bits: sign(0), exponent(111,11), fraction(00,0000,0000)
  }

  static constexpr onnxruntime::MLFloat16 quiet_NaN() noexcept {
    // The most significant fraction bit shall be 1, and no limitation on other fraction bits.
    // Note that most frameworks use 0x7E00; while CUDA uses 0x7FFF; .Net System.Half.NaN uses 0xFE00;
    return onnxruntime::MLFloat16::FromBits(0x7E00U);  // Bits: sign(0), exponent(111,11), fraction(10,0000,0000)
  }

  static constexpr onnxruntime::MLFloat16 signaling_NaN() noexcept {
    return onnxruntime::MLFloat16::FromBits(0x7D00U);  // Bits: sign(0), exponent(111,11), fraction(01,0000,0000)
  }

  static constexpr onnxruntime::MLFloat16 denorm_min() noexcept {
    return onnxruntime::MLFloat16::FromBits(0x0001U);  // Minimum subnormal value: 0.000000059604645
  }

  static constexpr onnxruntime::MLFloat16 epsilon() noexcept {
    return onnxruntime::MLFloat16::FromBits(0x1400U);  // Difference between 1.0 and the next value: 2^-10 = 0.0009765625
  }

  static constexpr onnxruntime::MLFloat16 round_error() noexcept {
    return onnxruntime::MLFloat16::FromBits(0x3800U);  // 0.5
  }

  static constexpr bool is_specialized = true;

  static constexpr bool is_signed = true;
  static constexpr bool is_integer = false;
  static constexpr bool is_exact = false;
  static constexpr bool has_infinity = true;
  static constexpr bool has_quiet_NaN = true;
  static constexpr bool has_signaling_NaN = true;
  static constexpr float_denorm_style has_denorm = denorm_present;
  static constexpr bool has_denorm_loss = false;

  static constexpr bool is_bounded = true;
  static constexpr bool is_iec559 = true;
  static constexpr bool is_modulo = false;

  static constexpr int digits = 11;       // Number of significant digits (mantissa)
  static constexpr int digits10 = 3;      // Decimal digits of precision
  static constexpr int max_digits10 = 5;  // Max decimal digits required for precision
  static constexpr int radix = 2;
  static constexpr int min_exponent = -13;
  static constexpr int min_exponent10 = -4;
  static constexpr int max_exponent = 16;
  static constexpr int max_exponent10 = 4;

  static constexpr bool traps = false;
  static constexpr bool tinyness_before = false;
  static constexpr std::float_round_style round_style = std::round_to_nearest;
};

template <>
class numeric_limits<onnxruntime::BFloat16> {
 public:
  static constexpr onnxruntime::BFloat16 min() noexcept {
    return onnxruntime::BFloat16::FromBits(0x0080U);  // Minimum positive normalized value: 1.175494e-38
  }

  static constexpr onnxruntime::BFloat16 max() noexcept {
    return onnxruntime::BFloat16::FromBits(0x7F7FU);  // Largest representable value: 3.38953139e38
  }

  static constexpr onnxruntime::BFloat16 lowest() noexcept {
    return onnxruntime::BFloat16::FromBits(0xFF7FU);  // Smallest representable value: -3.38953139e38
  }

  static constexpr onnxruntime::BFloat16 infinity() noexcept {
    return onnxruntime::BFloat16::FromBits(0x7F80U);  // Bits: sign(0), exponent(111,1111,1), fraction(000,0000)
  }

  static constexpr onnxruntime::BFloat16 quiet_NaN() noexcept {
    // The most significant fraction bit shall be 1, and no limitation on other fraction bits.
    // Note that Torch, Tensorflow, OpenVino, nGraph uses 0x7FC0; Paddle uses 0x7FC1; CUDA uses 0x7FFF.
    return onnxruntime::BFloat16::FromBits(0x7FC1U);  // Bits: sign(0), exponent(111,1111,1), fraction(100,0001)
  }

  static constexpr onnxruntime::BFloat16 signaling_NaN() noexcept {
    // The most significant fraction bit shall be 0, and there is at least one 1 in other fraction bits.
    return onnxruntime::BFloat16::FromBits(0x7F81U);  // Bits: sign(0), exponent(111,1111,1), fraction(000,0001)
  }

  static constexpr onnxruntime::BFloat16 denorm_min() noexcept {
    return onnxruntime::BFloat16::FromBits(0x0001U);  // Minimum subnormal value: 9.1835e-41
  }

  static constexpr onnxruntime::BFloat16 epsilon() noexcept {
    return onnxruntime::BFloat16::FromBits(0x3C00U);  // Difference between 1.0 and the next value: 2^-7 = 0.0078125
  }

  static constexpr onnxruntime::BFloat16 round_error() noexcept {
    return onnxruntime::BFloat16::FromBits(0x3F00U);  // 0.5
  }

  static constexpr bool is_specialized = true;
  static constexpr bool is_signed = true;
  static constexpr bool is_integer = false;
  static constexpr bool is_exact = false;
  static constexpr bool has_infinity = true;
  static constexpr bool has_quiet_NaN = true;
  static constexpr bool has_signaling_NaN = true;
  static constexpr float_denorm_style has_denorm = denorm_present;
  static constexpr bool has_denorm_loss = false;

  static constexpr bool is_bounded = true;
  static constexpr bool is_iec559 = false;
  static constexpr bool is_modulo = false;

  static constexpr int digits = 8;
  static constexpr int digits10 = 2;
  static constexpr int max_digits10 = 4;
  static constexpr int radix = 2;
  static constexpr int min_exponent = -125;
  static constexpr int min_exponent10 = -37;
  static constexpr int max_exponent = 128;
  static constexpr int max_exponent10 = 38;

  static constexpr bool traps = false;
  static constexpr bool tinyness_before = false;
  static constexpr float_round_style round_style = round_to_nearest;
};

}  // namespace std
