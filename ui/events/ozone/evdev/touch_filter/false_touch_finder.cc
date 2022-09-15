// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/touch_filter/false_touch_finder.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "ui/events/event_switches.h"
#include "ui/events/event_utils.h"
#include "ui/events/ozone/evdev/touch_filter/edge_touch_filter.h"
#include "ui/events/ozone/evdev/touch_filter/touch_filter.h"

namespace ui {

FalseTouchFinder::~FalseTouchFinder() {}

std::unique_ptr<FalseTouchFinder> FalseTouchFinder::Create(
    gfx::Size touchscreen_size) {
  bool edge_filtering = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEdgeTouchFiltering);
  if (edge_filtering) {
    return base::WrapUnique(
        new FalseTouchFinder(edge_filtering, touchscreen_size));
  }
  return nullptr;
}

void FalseTouchFinder::HandleTouches(
    const std::vector<InProgressTouchEvdev>& touches,
    base::TimeTicks time) {
  for (const InProgressTouchEvdev& touch : touches) {
    slots_should_delay_.set(touch.slot, false);
  }

  for (const auto& filter : delay_filters_)
    filter->Filter(touches, time, &slots_should_delay_);
}

bool FalseTouchFinder::SlotShouldDelay(size_t slot) const {
  return slots_should_delay_.test(slot);
}

FalseTouchFinder::FalseTouchFinder(bool edge_filtering,
                                   gfx::Size touchscreen_size)
    : last_noise_time_(ui::EventTimeForNow()) {
  if (edge_filtering) {
    delay_filters_.push_back(
        std::make_unique<EdgeTouchFilter>(touchscreen_size));
  }
}

}  // namespace ui
