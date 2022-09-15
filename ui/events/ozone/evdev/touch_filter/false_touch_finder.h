// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_FALSE_TOUCH_FINDER_H_
#define UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_FALSE_TOUCH_FINDER_H_

#include <stddef.h>

#include <bitset>
#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/time/time.h"
#include "ui/events/event_utils.h"
#include "ui/events/ozone/evdev/touch_evdev_types.h"

namespace ui {

class TouchFilter;

// Finds touches which are should be filtered.
class COMPONENT_EXPORT(EVDEV) FalseTouchFinder {
 public:
  FalseTouchFinder(const FalseTouchFinder&) = delete;
  FalseTouchFinder& operator=(const FalseTouchFinder&) = delete;

  ~FalseTouchFinder();

  static std::unique_ptr<FalseTouchFinder> Create(gfx::Size touchscreen_size);

  // Updates which ABS_MT_SLOTs should be filtered. |touches| should contain
  // all of the in-progress touches at |time| (including filtered touches).
  // |touches| should have at most one entry per ABS_MT_SLOT.
  void HandleTouches(const std::vector<InProgressTouchEvdev>& touches,
                     base::TimeTicks time);

  // Returns whether the in-progress touch at ABS_MT_SLOT |slot| should delay
  // reporting. They may be later reported.
  bool SlotShouldDelay(size_t slot) const;

 private:
  FalseTouchFinder(bool edge_filtering, gfx::Size touchscreen_size);

  friend class TouchEventConverterEvdevTouchNoiseTest;

  // The slots which should delay.
  std::bitset<kNumTouchEvdevSlots> slots_should_delay_;

  // The time of the previous noise occurrence in any of the slots.
  base::TimeTicks last_noise_time_;

  // Delay filters may filter questionable new touches for an indefinite time,
  // but should not start filtering a touch that it had previously allowed.
  std::vector<std::unique_ptr<TouchFilter>> delay_filters_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_FALSE_TOUCH_FINDER_H_
