// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <cassert>
#include <type_traits>
#include "core/common/common.h"
#include <gsl/gsl>

namespace onnxruntime {

template <bool Signed>
struct Int4Traits;

template <>
struct Int4Traits<true> {
  using UnpackedType = int8_t;
  static constexpr int8_t min_val = -8;
  static constexpr int8_t max_val = 7;
};

template <>
struct Int4Traits<false> {
  using UnpackedType = uint8_t;
  static constexpr uint8_t min_val = 0;
  static constexpr uint8_t max_val = 15;
};

/// <summary>
/// Stores 2 packed 4-bit elements in 1 byte.
/// </summary>
/// <typeparam name="Signed">Set to true if signed int4, or false if unsigned uint4.</typeparam>
template <bool Signed>
struct Int4x2Base {
  using UnpackedType = typename Int4Traits<Signed>::UnpackedType;
  static constexpr UnpackedType min_val = Int4Traits<Signed>::min_val;
  static constexpr UnpackedType max_val = Int4Traits<Signed>::max_val;

  std::byte bits_{};

  Int4x2Base() = default;

  explicit Int4x2Base(std::byte bits) {
    bits_ = bits;
  }

  Int4x2Base(UnpackedType val0, UnpackedType val1) {
    bits_ = static_cast<std::byte>(((val1 & 0xF) << 4) | (val0 & 0xF));
  }

  static inline int8_t SignExtendLower4Bits(std::byte bits) {
    // Sign-extend lower 4-bits by left shifting and then doing an arithmetic right shift.
    constexpr uint8_t shift = (sizeof(int32_t) * 8) - 4;
    return static_cast<int8_t>((static_cast<int32_t>(bits) << shift) >> shift);
  }

  inline UnpackedType GetElem(size_t index) const {
    assert(index <= 1);
    const uint8_t shift = 4 * static_cast<uint8_t>(index);
    const std::byte val = (bits_ >> shift) & std::byte{0xF};

    if constexpr (Signed) {
      return SignExtendLower4Bits(val);
    } else {
      return static_cast<UnpackedType>(val);
    }
  }

  inline void SetElem(size_t index, UnpackedType val) {
    assert(index <= 1);
    const uint8_t shift = 4 * static_cast<uint8_t>(index);
    const std::byte mask = std::byte{0xF0} >> shift;

    bits_ &= mask;                                          // Clear 4-bit element to 0
    bits_ |= static_cast<std::byte>((val & 0xF) << shift);  // Set 4-bit element to val
  }

  inline std::byte ToBits() const {
    return bits_;
  }

  static size_t CalcNumInt4Pairs(size_t num_int4_elems) {
    return (num_int4_elems + 1) / 2;
  }

  /// <summary>
  /// Copy a source buffer of 4-bit elements (packed) into a destination buffer of 8-bit elements (unpacked).
  /// </summary>
  /// <param name="dst">Destination buffer to store unpacked 8-bit elements</param>
  /// <param name="src">Source buffer with 4-bit elements</param>
  /// <returns>True on success</returns>
  static bool Unpack(gsl::span<UnpackedType> dst, gsl::span<const Int4x2Base<Signed>> src) {
    if (CalcNumInt4Pairs(dst.size()) != src.size()) {
      return false;
    }

    if (src.empty()) {
      return true;
    }

    for (size_t i = 0; i < dst.size(); i++) {
      size_t r = i >> 1;   // i / 2;
      size_t c = i & 0x1;  // i % 2;
      dst[i] = src[r].GetElem(c);
    }

    return true;
  }

  /// <summary>
  /// Copy a source buffer of 8-bit elements (unpacked) into a destination buffer of 4-bit elements (packed).
  /// </summary>
  /// <param name="dst">Destination buffer to store packed 4-bit elements</param>
  /// <param name="src">Source buffer with 8-bit elements</param>
  /// <returns>True on success</returns>
  static bool Pack(gsl::span<Int4x2Base<Signed>> dst, gsl::span<const UnpackedType> src) {
    if (CalcNumInt4Pairs(src.size()) != dst.size()) {
      return false;
    }

    if (src.empty()) {
      return true;
    }

    size_t src_i = 0;
    size_t dst_i = 0;

    for (; src_i < src.size() - 1; src_i += 2) {
      dst[dst_i++] = Int4x2Base<Signed>(src[src_i], src[src_i + 1]);
    }

    if (src_i < src.size()) {
      dst[dst_i] = Int4x2Base<Signed>(src[src_i], 0);
    }

    return true;
  }

  /// <summary>
  /// Returns hierarchical indices for a packed int4 element from the given element index.
  ///
  /// Usage:
  ///   Int4x2* data = ...;
  ///   auto indices = GetTensorElemIndices(3);  // 4th int4 element
  ///   int8_t elem = data[indices.first].GetElem(indices.second);
  /// </summary>
  /// <param name="index">Index of 4-bit element</param>
  /// <returns>Unpacked element</returns>
  static inline std::pair<size_t, size_t> GetTensorElemIndices(size_t index) {
    return {index >> 1, index & 0x1};
  }
};

using Int4x2 = Int4x2Base<true>;
using UInt4x2 = Int4x2Base<false>;
static_assert(sizeof(Int4x2) == sizeof(std::byte));
static_assert(sizeof(UInt4x2) == sizeof(std::byte));
}  // namespace onnxruntime
