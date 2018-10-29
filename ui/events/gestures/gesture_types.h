// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURES_GESTURE_TYPES_H_
#define UI_EVENTS_GESTURES_GESTURE_TYPES_H_

#include "ui/events/events_export.h"

namespace ui {

class GestureEvent;
class TouchEvent;

// TransferTouchesBehavior customizes the behavior of
// GestureRecognizer::TransferEventsTo.
enum class TransferTouchesBehavior {
  // Dispatches the cancel events to the current consumer on transfer to ensure
  // its touch stream remains valid.
  kCancel,

  // Do not dispatch cancel events.
  kDontCancel
};

// An abstract type for consumers of gesture-events created by the
// gesture-recognizer.
class EVENTS_EXPORT GestureConsumer {
 public:
  virtual ~GestureConsumer() {}

  // Supporting double tap events requires adding some extra delay before
  // sending single-tap events in order to determine whether its a potential
  // double tap or not. This delay may be undesirable in many UI components and
  // should be avoided if not needed.
  // Returns true if the consumer wants to receive double tap gesture events.
  // Defaults to false.
  virtual bool RequiresDoubleTapGestureEvents() const;
};

// GestureEventHelper creates implementation-specific gesture events and
// can dispatch them.
class EVENTS_EXPORT GestureEventHelper {
 public:
  virtual ~GestureEventHelper() {
  }

  // Returns true if this helper can dispatch events to |consumer|.
  virtual bool CanDispatchToConsumer(GestureConsumer* consumer) = 0;
  virtual void DispatchGestureEvent(GestureConsumer* raw_input_consumer,
                                    GestureEvent* event) = 0;
  virtual void DispatchSyntheticTouchEvent(TouchEvent* event) = 0;
};

}  // namespace ui

#endif  // UI_EVENTS_GESTURES_GESTURE_TYPES_H_
