// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>

#ifndef UI_ACCESSIBILITY_AX_BITSET_H_
#define UI_ACCESSIBILITY_AX_BITSET_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <tuple>

#include "base/check.h"

namespace ui {

// A helper class to store AX-related boolean enums.
template <typename T>
class AXBitset {
 public:
  AXBitset() = default;
  ~AXBitset() = default;

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

 private:
  uint64_t set_bits_ = 0;
  uint64_t values_ = 0;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_BITSET_H_
