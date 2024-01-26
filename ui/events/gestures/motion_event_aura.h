// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURES_MOTION_EVENT_AURA_H_
#define UI_EVENTS_GESTURES_MOTION_EVENT_AURA_H_

#include <stddef.h>

#include "ui/events/event.h"
#include "ui/events/events_export.h"
#include "ui/events/velocity_tracker/motion_event_generic.h"

namespace ui {

// Implementation of MotionEvent which takes a stream of ui::TouchEvents.
class EVENTS_EXPORT MotionEventAura : public MotionEventGeneric {
 public:
  MotionEventAura();

  MotionEventAura(const MotionEventAura&) = delete;
  MotionEventAura& operator=(const MotionEventAura&) = delete;

  ~MotionEventAura() override;

  // MotionEventGeneric:
  int GetSourceDeviceId(size_t pointer_index) const override;

  // Returns true iff the touch was valid.
  bool OnTouch(const TouchEvent& touch);

  // We can't cleanup removed touch points immediately upon receipt of a
  // TouchCancel or TouchRelease, as the MotionEvent needs to be able to report
  // information about those touch events. Once the MotionEvent has been
  // processed, we call CleanupRemovedTouchPoints to do the required
  // book-keeping.
  void CleanupRemovedTouchPoints(const TouchEvent& event);

 private:
  bool AddTouch(const TouchEvent& touch);
  void UpdateTouch(const TouchEvent& touch);
  void UpdateCachedAction(const TouchEvent& touch);
  int GetIndexFromId(int id) const;
};

}  // namespace ui

#endif  // UI_EVENTS_GESTURES_MOTION_EVENT_AURA_H_
