// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_UNIQUE_ID_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_UNIQUE_ID_H_

#include <stdint.h>

#include <limits>
#include <utility>

#include "base/component_export.h"

namespace ui {

// AXUniqueID provides IDs for accessibility objects that are guaranteed to be
// unique for the entire Chrome instance. New IDs are generated via `Create()`,
// and the ID is freed when the instance is destroyed.
//
// The unique id is guaranteed to be a positive number. Because some platforms
// want to negate it, we ensure the range is below the signed int max.
//
// These ids must not be conflated with the int id, that comes with web node
// data, which are only unique within their source frame.
// TODO(accessibility) We should be able to get rid of this, because node IDs
// are actually unique within their own OS-level window.
class COMPONENT_EXPORT(AX_PLATFORM) AXUniqueId final {
 public:
  static constexpr int32_t kInvalidId = 0;

  static AXUniqueId Create() {
    return AXUniqueId(GetNextAXUniqueId(std::numeric_limits<int32_t>::max()));
  }

  AXUniqueId() = delete;
  AXUniqueId(AXUniqueId&& other) noexcept
      : id_(std::exchange(other.id_, kInvalidId)) {}
  AXUniqueId& operator=(AXUniqueId&& other) noexcept {
    id_ = std::exchange(other.id_, kInvalidId);
    return *this;
  }

  ~AXUniqueId();

  int32_t Get() const { return id_; }
  operator int32_t() const { return id_; }

  friend bool operator==(const AXUniqueId&, const AXUniqueId&) = default;
  friend bool operator<=>(const AXUniqueId&, const AXUniqueId&) = default;

  static AXUniqueId CreateForTest(int32_t max_id) {
    return AXUniqueId(GetNextAXUniqueId(max_id));
  }

 private:
  explicit AXUniqueId(int32_t id) : id_(id) {}

  // Returns the next available value given a max of `max_id`.
  static int32_t GetNextAXUniqueId(int32_t max_id);

  int32_t id_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_UNIQUE_ID_H_
