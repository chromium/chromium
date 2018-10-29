// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURES_GESTURE_RECOGNIZER_OBSERVER_H_
#define UI_EVENTS_GESTURES_GESTURE_RECOGNIZER_OBSERVER_H_

#include "base/observer_list_types.h"
#include "ui/events/events_export.h"
#include "ui/events/gestures/gesture_types.h"

namespace ui {

class EVENTS_EXPORT GestureRecognizerObserver : public base::CheckedObserver {
 public:
  virtual void OnActiveTouchesCanceledExcept(
      GestureConsumer* not_cancelled) = 0;
  virtual void OnEventsTransferred(
      GestureConsumer* current_consumer,
      GestureConsumer* new_consumer,
      TransferTouchesBehavior transfer_touches_behavior) = 0;
  virtual void OnActiveTouchesCanceled(GestureConsumer* consumer) = 0;

 protected:
  ~GestureRecognizerObserver() override;
};

}  // namespace ui

#endif  // UI_EVENTS_GESTURES_GESTURE_RECOGNIZER_OBSERVER_H_
