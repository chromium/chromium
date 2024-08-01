// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/ozone/evdev/touch_filter/edge_touch_filter.h"

#include <stddef.h>

#include <cmath>

#include "base/metrics/histogram_macros.h"
#include "ui/gfx/geometry/insets.h"

namespace ui {

namespace {

// The maximum distance from the border to be considered for filtering
const int kMaxBorderDistance = 1;

bool IsNearBorder(const gfx::Point& point, gfx::Size touchscreen_size) {
  gfx::Rect inner_bound = gfx::Rect(touchscreen_size);
  inner_bound.Inset(gfx::Insets(kMaxBorderDistance));
  return !inner_bound.Contains(point);
}

}  // namespace

EdgeTouchFilter::EdgeTouchFilter(const gfx::Size& touchscreen_size)
    : touchscreen_size_(touchscreen_size) {}

EdgeTouchFilter::~EdgeTouchFilter() {}

void EdgeTouchFilter::Filter(
    const std::vector<InProgressTouchEvdev>& touches,
    base::TimeTicks time,
    std::bitset<kNumTouchEvdevSlots>* slots_should_delay) {

  for (const InProgressTouchEvdev& touch : touches) {
    size_t slot = touch.slot;
    gfx::Point touch_pos = gfx::Point(touch.x, touch.y);

    if (!touch.touching && !touch.was_touching)
      continue;  // Only look at slots with active touches.

    if (!touch.was_touching) {
      // Track new contact and delay if near border.
      bool near_border = IsNearBorder(touch_pos, touchscreen_size_);
      slots_filtered_.set(slot, near_border);
      if (near_border)
        start_positions_[slot] = touch_pos;
    }

    if (touch_pos != start_positions_[slot])
      slots_filtered_.set(slot, false);  // Stop delaying contacts that move.
  }

  (*slots_should_delay) |= slots_filtered_;
}

}  // namespace ui
