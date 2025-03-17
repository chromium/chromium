// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_LOCATION_AND_SCROLL_UPDATES_H_
#define UI_ACCESSIBILITY_AX_LOCATION_AND_SCROLL_UPDATES_H_

#include <vector>

#include "ui/accessibility/ax_base_export.h"
#include "ui/accessibility/ax_relative_bounds.h"

namespace ui {
struct AX_BASE_EXPORT AXLocationChange {
 public:
  AXLocationChange() = default;
  AXLocationChange(int id, AXRelativeBounds bounds)
      : id(id), new_location(std::move(bounds)) {}

  AXLocationChange(const AXLocationChange& other);
  AXLocationChange& operator=(const AXLocationChange& other);

  AXLocationChange(AXLocationChange&& other) noexcept = default;
  AXLocationChange& operator=(AXLocationChange&& other) noexcept = default;

  int id;
  AXRelativeBounds new_location;
};

struct AX_BASE_EXPORT AXScrollChange {
 public:
  AXScrollChange() = default;
  AXScrollChange(int id, int x, int y);

  AXScrollChange(const AXScrollChange& other);
  AXScrollChange& operator=(const AXScrollChange& other);

  AXScrollChange(AXScrollChange&& other);
  AXScrollChange& operator=(AXScrollChange&& other);

  ~AXScrollChange();

  int id;
  int scroll_x;
  int scroll_y;
};

struct AX_BASE_EXPORT AXLocationAndScrollUpdates {
 public:
  AXLocationAndScrollUpdates();

  AXLocationAndScrollUpdates(const AXLocationAndScrollUpdates& other) = delete;
  AXLocationAndScrollUpdates& operator=(
      const AXLocationAndScrollUpdates& other) = delete;

  AXLocationAndScrollUpdates(AXLocationAndScrollUpdates&& other);
  AXLocationAndScrollUpdates& operator=(AXLocationAndScrollUpdates&& other);

  ~AXLocationAndScrollUpdates();

  std::vector<AXLocationChange> location_changes;
  std::vector<AXScrollChange> scroll_changes;
};
}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_LOCATION_AND_SCROLL_UPDATES_H_
