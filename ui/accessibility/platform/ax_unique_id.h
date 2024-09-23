// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_UNIQUE_ID_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_UNIQUE_ID_H_

#include <stdint.h>

#include <limits>
#include <utility>

#include "base/component_export.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/platform/ax_platform_node_id.h"

namespace ui {

// AXUniqueId provides IDs for accessibility objects that are guaranteed to be
// unique for the entire Chrome instance. New values are generated via
// `Create()`, and an instance's value is available for reuse when the instance
// is destroyed. Instances are implicitly convertible to (but not from)
// AXPlatformNodeId.
class COMPONENT_EXPORT(AX_PLATFORM) AXUniqueId final {
 public:
  static AXUniqueId Create() {
    return AXUniqueId(GetNextAXUniqueId(std::numeric_limits<int32_t>::max()));
  }

  AXUniqueId() = delete;
  AXUniqueId(AXUniqueId&& other) noexcept
      : id_(std::exchange(other.id_, AXPlatformNodeId())) {}
  AXUniqueId& operator=(AXUniqueId&& other) noexcept {
    id_ = std::exchange(other.id_, AXPlatformNodeId());
    return *this;
  }

  ~AXUniqueId();

  AXPlatformNodeId Get() const { return id_; }
  constexpr operator const AXPlatformNodeId&() const { return id_; }

  friend bool operator==(const AXUniqueId&, const AXUniqueId&) = default;
  friend bool operator<=>(const AXUniqueId&, const AXUniqueId&) = default;

  static AXUniqueId CreateForTest(int32_t max_id) {
    return AXUniqueId(GetNextAXUniqueId(max_id));
  }

 private:
  explicit AXUniqueId(AXPlatformNodeId id) : id_(id) {}

  // Returns the next available value given a max of `max_id`.
  static AXPlatformNodeId GetNextAXUniqueId(int32_t max_id);

  AXPlatformNodeId id_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_UNIQUE_ID_H_
