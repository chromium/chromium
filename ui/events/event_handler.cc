// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_handler.h"

#include "ui/events/event.h"
#include "ui/events/event_dispatcher.h"

namespace ui {

// static
bool EventHandler::check_targets_ = true;

EventHandler::EventHandler() {
}

EventHandler::~EventHandler() {
  while (!dispatchers_.empty()) {
    EventDispatcher* dispatcher = dispatchers_.top();
    dispatchers_.pop();
    dispatcher->OnHandlerDestroyed(this);
  }

  // Should have been removed from all pre-target handlers.
  CHECK(!check_targets_ || targets_installed_on_.empty());
}

void EventHandler::OnEvent(Event* event) {
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

}  // namespace ui
