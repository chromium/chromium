// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_SCOPED_TARGET_HANDLER_H_
#define UI_EVENTS_SCOPED_TARGET_HANDLER_H_

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
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

  ScopedTargetHandler(const ScopedTargetHandler&) = delete;
  ScopedTargetHandler& operator=(const ScopedTargetHandler&) = delete;

  ~ScopedTargetHandler() override;

  // EventHandler:
  void OnEvent(Event* event) override;
  std::string_view GetLogContext() const override;

 private:

  // An EventTarget that has its target handler replaced with |this| for a life
  // time of |this|.
  raw_ptr<EventTarget> target_;

  // An EventHandler that gets restored on |view_| when |this| is destroyed.
  raw_ptr<EventHandler> original_handler_;

  // A new handler that gets events in addition to the |original_handler_|.
  raw_ptr<EventHandler> new_handler_;

  // Used to detect if handling an event has caused |this| to be deleted. Must
  // be last.
  base::WeakPtrFactory<ScopedTargetHandler> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_EVENTS_SCOPED_TARGET_HANDLER_H_
