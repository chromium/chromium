// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_SCOPED_TARGET_HANDLER_H_
#define UI_EVENTS_SCOPED_TARGET_HANDLER_H_

#include "base/macros.h"
#include "ui/events/event_handler.h"
#include "ui/events/events_export.h"

namespace ui {

class EventTarget;

// An EventHandler that replaces an EventTarget's target handler with itself to
// pass events first to the original handler and second to an additional new
// EventHandler. The new handler gets called after the original handler even
// if it calls SetHandled() on the event but not if StopPropagation() is called
// on the event.
class EVENTS_EXPORT ScopedTargetHandler : public EventHandler {
 public:
  ScopedTargetHandler(EventTarget* target, EventHandler* new_handler);
  ~ScopedTargetHandler() override;

  // EventHandler:
  void OnEvent(Event* event) override;

 private:
  // If non-null the destructor sets this to true. This is set while handling
  // an event and used to detect if |this| has been deleted.
  bool* destroyed_flag_;

  // An EventTarget that has its target handler replaced with |this| for a life
  // time of |this|.
  EventTarget* target_;

  // An EventHandler that gets restored on |view_| when |this| is destroyed.
  EventHandler* original_handler_;

  // A new handler that gets events in addition to the |original_handler_|.
  EventHandler* new_handler_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTargetHandler);
};

}  // namespace ui

#endif  // UI_EVENTsS_SCOPED_TARGET_HANDLER_H_
