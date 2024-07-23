// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURES_GESTURE_RECOGNIZER_H_
#define UI_EVENTS_GESTURES_GESTURE_RECOGNIZER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "ui/events/event_constants.h"
#include "ui/events/events_export.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {

// A GestureRecognizer is an abstract base class for conversion of touch events
// into gestures.
class EVENTS_EXPORT GestureRecognizer {
 public:
  using Gestures = std::vector<std::unique_ptr<GestureEvent>>;

  GestureRecognizer();

  GestureRecognizer(const GestureRecognizer&) = delete;
  GestureRecognizer& operator=(const GestureRecognizer&) = delete;

  virtual ~GestureRecognizer();

  // Invoked before event dispatch. If the event is invalid given the current
  // touch sequence, returns false.
  virtual bool ProcessTouchEventPreDispatch(TouchEvent* event,
                                            GestureConsumer* consumer) = 0;

  // Returns a list of zero or more GestureEvents. Acks the gesture packet in
  // the queue which matches with unique_event_id.
  virtual Gestures AckTouchEvent(uint32_t unique_event_id,
                                 ui::EventResult result,
                                 bool is_source_touch_event_set_blocking,
                                 GestureConsumer* consumer) = 0;

  // This is called when the consumer is destroyed. So this should cleanup any
  // internal state maintained for |consumer|. Returns true iff there was
  // state relating to |consumer| to clean up.
  virtual bool CleanupStateForConsumer(GestureConsumer* consumer) = 0;

  // Return the window which should handle this TouchEvent, in the case where
  // the touch is already associated with a target.
  // Otherwise, returns null.
  virtual GestureConsumer* GetTouchLockedTarget(const TouchEvent& event) = 0;

  // Returns the target of the nearest active touch with source device of
  // |source_device_id|, within
  // GestureConfiguration::max_separation_for_gesture_touches_in_pixels of
  // |location|, or NULL if no such point exists.
  virtual GestureConsumer* GetTargetForLocation(const gfx::PointF& location,
                                                int source_device_id) = 0;

  // Cancels all touches except those targeted to |not_cancelled|. If
  // |not_cancelled| == nullptr, cancels all touches.
  virtual void CancelActiveTouchesExcept(GestureConsumer* not_cancelled) = 0;

  // Cancels all touches to the specified consumers.
  virtual void CancelActiveTouchesOn(
      const std::vector<GestureConsumer*>& consumers) = 0;

  // Transfer the gesture stream from the drag source (current_consumer) to the
  // consumer used for dragging (new_consumer). If |transfer_touches_behavior|
  // is kCancel, dispatches cancel events to |current_consumer| to ensure that
  // its touch stream remains valid.
  virtual void TransferEventsTo(
      GestureConsumer* current_consumer,
      GestureConsumer* new_consumer,
      TransferTouchesBehavior transfer_touches_behavior) = 0;

  // If a gesture is underway for |consumer| |point| is set to the last touch
  // point and true is returned. If no touch events have been processed for
  // |consumer| false is returned and |point| is untouched.
  virtual bool GetLastTouchPointForTarget(GestureConsumer* consumer,
                                          gfx::PointF* point) = 0;

  // Sends a touch cancel event for every active touch. Returns true iff any
  // touch cancels were sent.
  virtual bool CancelActiveTouches(GestureConsumer* consumer) = 0;

  // Subscribes |helper| for dispatching async gestures such as long press.
  // The Gesture Recognizer does NOT take ownership of |helper| and it is the
  // responsibility of the |helper| to call |RemoveGestureEventHelper()| on
  // destruction.
  virtual void AddGestureEventHelper(GestureEventHelper* helper) = 0;

  // Unsubscribes |helper| from async gesture dispatch.
  // Since the GestureRecognizer does not own the |helper|, it is not deleted
  // and must be cleaned up appropriately by the caller.
  virtual void RemoveGestureEventHelper(GestureEventHelper* helper) = 0;

  // Returns whether `consumer` has active touch or not.
  virtual bool DoesConsumerHaveActiveTouch(GestureConsumer* consumer) const = 0;

  // Synthesizes gesture end events (including EventType::kGestureEnd and
  // EventType::kGestureScrollEnd) and send to `consumer`.
  virtual void SendSynthesizedEndEvents(GestureConsumer* consumer) = 0;
};

}  // namespace ui

#endif  // UI_EVENTS_GESTURES_GESTURE_RECOGNIZER_H_
