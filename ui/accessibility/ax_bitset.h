// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_BITSET_H_
#define UI_ACCESSIBILITY_AX_BITSET_H_

#include <stdint.h>

#include <optional>

#include "base/functional/function_ref.h"

namespace ui {

// A helper class to store AX-related boolean enums.
template <typename T>
class AXBitset {
 public:
  AXBitset() = default;
  ~AXBitset() = default;

  uint64_t GetSetBits() const { return set_bits_; }
  uint64_t GetValues() const { return values_; }

  // Returns whether enum T at |value| is set to true, false or unset.
  std::optional<bool> Has(T enum_value) const {
    uint64_t index = static_cast<uint64_t>(enum_value);
    uint64_t mask = 1ULL << index;
    // Check if the value is set.
    if (set_bits_ & mask) {
      return values_ & mask;
    }
    return std::nullopt;
  }

  // Sets the enum T at |enum_value| to true or false.
  void Set(T enum_value, bool bool_value) {
    uint64_t index = static_cast<uint64_t>(enum_value);
    uint64_t mask = 1ULL << index;
    // Mark as set.
    set_bits_ |= mask;
    if (bool_value) {
      // Set the value bit to 1 for true.
      values_ |= mask;
    } else {
      // Clear the value bit to 0 for false.
      values_ &= ~mask;
    }
  }

  void Unset(T enum_value) {
    uint64_t index = static_cast<uint64_t>(enum_value);
    uint64_t mask = 1ULL << index;
    // Mark as not set.
    set_bits_ &= ~mask;
  }

  // Iterates over each attribute that is currently "set" (i.e., has been
  // explicitly set to true or false and not subsequently unset) and invokes
  // the provided 'function' with the attribute and its boolean value.
  // The order of iteration is from the least significant bit (lowest enum
  // value) to the most significant bit (highest enum value) among the set
  // attributes.
  void ForEach(
      base::FunctionRef<void(T attribute, bool value)> function) const {
    uint64_t remainder = set_bits_;

    while (remainder) {
      // Find the index (0-63) of the least significant bit that is set to 1
      // in 'remainder'. This corresponds to the enum's integer value.
      // std::countr_zero counts trailing zeros; e.g., for 0b...1000, it
      // returns 3.
      uint64_t index = std::countr_zero(remainder);

      T attribute = static_cast<T>(index);
      uint64_t mask = 1ULL << index;
      bool attribute_value = static_cast<bool>(values_ & mask);

      function(attribute, attribute_value);

      // Clear the least significant set bit in 'remainder' to prepare for the
      // next iteration. This ensures that each set bit is processed exactly
      // once.
      remainder &= remainder - 1;
    }
  }

  // Returns the number of attributes that are currently explicitly set
  // (i.e., have been Set to true or false and not subsequently Unset).
  size_t Size() const { return std::popcount(set_bits_); }

 private:
  uint64_t set_bits_ = 0;
  uint64_t values_ = 0;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_BITSET_H_
