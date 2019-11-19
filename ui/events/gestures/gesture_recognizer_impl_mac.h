// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURES_GESTURE_RECOGNIZER_IMPL_MAC_H_
#define UI_EVENTS_GESTURES_GESTURE_RECOGNIZER_IMPL_MAC_H_

#include <stdint.h>

#include "base/macros.h"
#include "ui/events/event.h"
#include "ui/events/events_export.h"
#include "ui/events/gestures/gesture_recognizer.h"

namespace ui {

// Stub implementation of GestureRecognizer for Mac. Currently only serves to
// provide a no-op implementation of TransferEventsTo().
class EVENTS_EXPORT GestureRecognizerImplMac : public GestureRecognizer {
 public:
  GestureRecognizerImplMac();
  ~GestureRecognizerImplMac() override;

 private:
  // GestureRecognizer:
  bool ProcessTouchEventPreDispatch(TouchEvent* event,
                                    GestureConsumer* consumer) override;
  Gestures AckTouchEvent(uint32_t unique_event_id,
                         ui::EventResult result,
                         bool is_source_touch_event_set_non_blocking,
                         GestureConsumer* consumer) override;
  bool CleanupStateForConsumer(GestureConsumer* consumer) override;
  GestureConsumer* GetTouchLockedTarget(const TouchEvent& event) override;
  GestureConsumer* GetTargetForLocation(const gfx::PointF& location,
                                        int source_device_id) override;
  void CancelActiveTouchesExcept(GestureConsumer* not_cancelled) override;
  void CancelActiveTouchesOn(
      const std::vector<GestureConsumer*>& consumers) override;
  void TransferEventsTo(
      GestureConsumer* current_consumer,
      GestureConsumer* new_consumer,
      TransferTouchesBehavior transfer_touches_behavior) override;
  std::vector<std::unique_ptr<ui::TouchEvent>> ExtractTouches(
      GestureConsumer* consumer) override;
  void TransferTouches(GestureConsumer* consumer,
                       const std::vector<std::unique_ptr<ui::TouchEvent>>&
                           touch_events) override;
  bool GetLastTouchPointForTarget(GestureConsumer* consumer,
                                  gfx::PointF* point) override;
  bool CancelActiveTouches(GestureConsumer* consumer) override;
  void AddGestureEventHelper(GestureEventHelper* helper) override;
  void RemoveGestureEventHelper(GestureEventHelper* helper) override;

  DISALLOW_COPY_AND_ASSIGN(GestureRecognizerImplMac);
};

}  // namespace ui

#endif  // UI_EVENTS_GESTURES_GESTURE_RECOGNIZER_IMPL_MAC_H_
