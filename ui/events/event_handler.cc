// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_handler.h"

#include <string_view>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "ui/events/event.h"
#include "ui/events/event_dispatcher.h"

namespace ui {

EventHandler::EventHandler() = default;

EventHandler::~EventHandler() {
  while (!dispatchers_.empty()) {
    EventDispatcher* dispatcher = dispatchers_.top();
    dispatchers_.pop();
    dispatcher->OnHandlerDestroyed(this);
  }
}

void EventHandler::OnEvent(Event* event) {
  // You may uncomment the following line if more detailed logging is necessary
  // for diagnosing event processing. This code is a critical path and the added
  // overhead from the logging can introduce other issues. Please do not commit
  // with the following line commented without first discussing with OWNERs.
  // See crbug/1210633 for details.
  // VLOG(5) << GetLogContext() << "::OnEvent(" << event->ToString() << ")";
  if (event->IsKeyEvent())
    OnKeyEvent(event->AsKeyEvent());
  else if (event->IsMouseEvent())
    OnMouseEvent(event->AsMouseEvent());
  else if (event->IsScrollEvent())
    OnScrollEvent(event->AsScrollEvent());
  else if (event->IsTouchEvent())
    OnTouchEvent(event->AsTouchEvent());
  else if (event->IsGestureEvent())
    OnGestureEvent(event->AsGestureEvent());
  else if (event->IsCancelModeEvent())
    OnCancelMode(event->AsCancelModeEvent());
}

void EventHandler::OnKeyEvent(KeyEvent* event) {
}

void EventHandler::OnMouseEvent(MouseEvent* event) {
}

void EventHandler::OnScrollEvent(ScrollEvent* event) {
}

void EventHandler::OnTouchEvent(TouchEvent* event) {
}

void EventHandler::OnGestureEvent(GestureEvent* event) {
}

void EventHandler::OnCancelMode(CancelModeEvent* event) {
}

std::string_view EventHandler::GetLogContext() const {
  return "(Unknown EventHandler)"; // Please override
}

}  // namespace ui
