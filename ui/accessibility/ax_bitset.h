// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_BITSET_H_
#define UI_ACCESSIBILITY_AX_BITSET_H_

#include <stdint.h>

#include <bit>
#include <optional>

#include "base/functional/function_ref.h"

namespace ui {

// A helper class to store AX-related boolean enums.
// IMPORTANT: This AXBitset implementation uses single uint32_t bitmasks and is
// therefore limited to managing enums whose underlying integer values are
// strictly less than 32 (i.e., in the range [0, 31]). Enum values outside
// this range will lead to incorrect behavior or will be ignored.
template <typename T>
class AXBitset {
 public:
  AXBitset() = default;
  AXBitset(uint32_t initial_set_bits, uint32_t initial_values)
      : set_bits_(initial_set_bits), values_(initial_values) {}
  ~AXBitset() = default;

  uint32_t GetSetBits() const { return set_bits_; }
  uint32_t GetValues() const { return values_; }

  // Returns whether enum T at |value| is set to true, false or unset.
  std::optional<bool> Get(T enum_value) const {
    uint32_t index = static_cast<uint32_t>(enum_value);
    uint32_t mask = 1u << index;
    // Check if the value is set.
    if (set_bits_ & mask) {
      return values_ & mask;
    }
    return std::nullopt;
  }

  // Sets the enum T at |enum_value| to true or false.
  void Set(T enum_value, bool bool_value) {
    uint32_t index = static_cast<uint32_t>(enum_value);
    uint32_t mask = 1u << index;
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
    uint32_t index = static_cast<uint32_t>(enum_value);
    uint32_t mask = 1u << index;
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
    uint32_t remainder = set_bits_;

    while (remainder) {
      // Find the index (0-31) of the least significant bit that is set to 1
      // in 'remainder'. This corresponds to the enum's integer value.
      // std::countr_zero counts trailing zeros; e.g., for 0b...1000, it
      // returns 3.
      int index = std::countr_zero(remainder);

      T attribute = static_cast<T>(index);
      uint32_t mask = 1u << index;
      bool attribute_value = static_cast<bool>(values_ & mask);

      function(attribute, attribute_value);

      // Clear the least significant set bit in 'remainder' to prepare for the
      // next iteration. This ensures that each set bit is processed exactly
      // once.
      remainder &= remainder - 1;
    }
  }

  // Merges the set attributes from another AXBitset into this one.
  void Append(const AXBitset<T>& other) {
    // Clear positions in 'this->values_' that will be overridden by 'other'.
    // These are positions where 'other.set_bits_' has a '1'.
    // `~other.set_bits_` has '0's at these positions, so ANDing clears them in
    // `this->values_`.
    values_ &= ~other.set_bits_;

    // OR in the relevant values from 'other'.
    // `(other.values_ & other.set_bits_)` isolates T/F values only for
    // attributes actually set in 'other'.
    values_ |= (other.values_ & other.set_bits_);

    // Ensure attributes set in 'other' are now also marked as set in 'this'.
    set_bits_ |= other.set_bits_;
  }

  // Returns the number of attributes that are currently explicitly set
  // (i.e., have been Set to true or false and not subsequently Unset).
  size_t Size() const { return std::popcount(set_bits_); }

  template <typename U>
  friend bool operator==(const AXBitset<U>& lhs, const AXBitset<U>& rhs);

 private:
  uint32_t set_bits_ = 0;
  uint32_t values_ = 0;
};

template <typename T>
bool operator==(const AXBitset<T>& lhs, const AXBitset<T>& rhs) {
  // Check if the set of active attributes is the same.
  if (lhs.set_bits_ != rhs.set_bits_) {
    return false;
  }

  // If the set_bits_ are identical, then  compare the values for the bits that
  // are actually set.
  return (lhs.values_ & lhs.set_bits_) == (rhs.values_ & lhs.set_bits_);
}

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_BITSET_H_
