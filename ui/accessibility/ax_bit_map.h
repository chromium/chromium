// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef UI_ACCESSIBILITY_AX_BIT_MAP_H_
#define UI_ACCESSIBILITY_AX_BIT_MAP_H_

#include <memory>
#include <optional>
#include <string>
#include <tuple>

#include "base/check.h"

namespace ui {

// A helper class to store AX-related boolean enums.
template <typename T>
class AXBitMap {
 public:
  static const size_t kElementsPerMapBucket = 64;

  AXBitMap() {
    for (size_t i = 0;
         i <= static_cast<size_t>(T::kMaxValue) / kElementsPerMapBucket; ++i) {
      true_map_[i] = 0;
      false_map_[i] = 0;
    }
  }
  ~AXBitMap() = default;

  // Returns whether enum T at |value| is set to true, false or unset.
  std::optional<bool> Has(const T enum_value) {
    auto [value_position, true_map, false_map] = GetPositionAndMaps(enum_value);
    const bool is_in_true_map = (*true_map) >> value_position & 1ull;
    const bool is_in_false_map = (*false_map) >> value_position & 1ull;

    CHECK(!(is_in_true_map && is_in_false_map))
        << std::string("A value can't be true and false at the same time.");

    if (is_in_true_map) {
      return true;
    }

    if (is_in_false_map) {
      return false;
    }

    return std::nullopt;
  }

  // Sets the enum T at |enum_value| to true or false.
  void Set(const T enum_value, const bool bool_value) {
    auto [value_position, true_map, false_map] = GetPositionAndMaps(enum_value);
    uint64_t* map_to_set_true;
    uint64_t* map_to_set_false;
    if (bool_value) {
      map_to_set_true = true_map;
      map_to_set_false = false_map;
    } else {
      map_to_set_true = false_map;
      map_to_set_false = true_map;
    }

    *map_to_set_true |= 1ull << value_position;
    *map_to_set_false &= ~(1ull << value_position);
  }

  // Unsets the enum T at |enum_value|. If it is not set, this is a no-op.
  void Unset(const T enum_value) {
    auto [value_position, true_map, false_map] = GetPositionAndMaps(enum_value);
    (*true_map) &= ~(1ull << value_position);
    (*false_map) &= ~(1ull << value_position);
  }

 private:
  // Helper function that returns a tuple with the position of the value in the
  // maps, a pointer to the correct bucket in |true_map_| and |false_map_|.
  std::tuple<uint64_t, uint64_t*, uint64_t*> GetPositionAndMaps(const T value) {
    uint64_t absolute_value_position = static_cast<uint64_t>(value);
    const size_t map_bucket = absolute_value_position / kElementsPerMapBucket;
    uint64_t* true_map = &(true_map_[map_bucket]);
    uint64_t* false_map = &(false_map_[map_bucket]);

    // Subtracts map_bucket * 64 from |value_position| so that it references the
    // correct place in the map variables.
    uint64_t relative_value_position =
        absolute_value_position - map_bucket * kElementsPerMapBucket;

    return {relative_value_position, true_map, false_map};
  }

  // Indicates that the enum T is true at the bit shifted value. This array
  // holds 64 enum values per position, and will contains as many entries to
  // hold all enum possible values.
  uint64_t true_map_[static_cast<size_t>(
      static_cast<size_t>(T::kMaxValue) / kElementsPerMapBucket + 1)];

  // Indicates that the enum T is false at the bit shifted value. This array
  // holds 64 enum values per position, and will contains as many entries to
  // hold all enum possible values.
  uint64_t false_map_[static_cast<size_t>(
      static_cast<size_t>(T::kMaxValue) / kElementsPerMapBucket + 1)];

  // Undefined/unset implied by not in *_true and *_false;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_BIT_MAP_H_
