// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_EVENT_HANDLER_H_
#define UI_EVENTS_EVENT_HANDLER_H_

#include <string_view>
#include <vector>

#include "base/containers/stack.h"
#include "base/memory/raw_ptr.h"
#include "ui/events/events_export.h"

namespace ui {

class CancelModeEvent;
class Event;
class EventDispatcher;
class EventTarget;
class GestureEvent;
class KeyEvent;
class MouseEvent;
class ScrollEvent;
class TouchEvent;

// Dispatches events to appropriate targets.  The default implementations of
// all of the specific handlers (e.g. OnKeyEvent, OnMouseEvent) do nothing.
class EVENTS_EXPORT EventHandler {
 public:
  EventHandler();
  EventHandler(const EventHandler&) = delete;
  EventHandler& operator=(const EventHandler&) = delete;
  virtual ~EventHandler();

  // This is called for all events. The default implementation routes the event
  // to one of the event-specific callbacks (OnKeyEvent, OnMouseEvent etc.). If
  // this is overridden, then normally, the override should chain into the
  // default implementation for un-handled events.
  virtual void OnEvent(Event* event);

  virtual void OnKeyEvent(KeyEvent* event);

  virtual void OnMouseEvent(MouseEvent* event);

  virtual void OnScrollEvent(ScrollEvent* event);

  virtual void OnTouchEvent(TouchEvent* event);

  virtual void OnGestureEvent(GestureEvent* event);

  virtual void OnCancelMode(CancelModeEvent* event);

  // Returns information about the implementing class or scope for diagnostic
  // logging purposes.
  virtual std::string_view GetLogContext() const;

 private:
  friend class EventDispatcher;
  friend class EventTarget;

  // EventDispatcher pushes itself on the top of this stack while dispatching
  // events to this then pops itself off when done.
  base::stack<raw_ptr<EventDispatcher, CtnExperimental>> dispatchers_;
};

using EventHandlerList = std::vector<raw_ptr<EventHandler, VectorExperimental>>;

}  // namespace ui

#endif  // UI_EVENTS_EVENT_HANDLER_H_
