// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/platform/x11/x11_event_source_default.h"

#include <memory>

#include "base/message_loop/message_loop_current.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/x/events_x_utils.h"
#include "ui/events/x/x11_event_translation.h"
#include "ui/gfx/x/x11.h"

#if defined(OS_CHROMEOS)
#include "ui/events/ozone/chromeos/cursor_controller.h"
#endif

namespace ui {

X11EventSourceDefault::X11EventSourceDefault(XDisplay* display)
    : event_source_(this, display), watcher_controller_(FROM_HERE) {
  AddEventWatcher();
}

X11EventSourceDefault::~X11EventSourceDefault() {}

// static
X11EventSourceDefault* X11EventSourceDefault::GetInstance() {
  return static_cast<X11EventSourceDefault*>(
      PlatformEventSource::GetInstance());
}

void X11EventSourceDefault::AddXEventDispatcher(XEventDispatcher* dispatcher) {
  dispatchers_xevent_.AddObserver(dispatcher);
  PlatformEventDispatcher* event_dispatcher =
      dispatcher->GetPlatformEventDispatcher();
  if (event_dispatcher)
    AddPlatformEventDispatcher(event_dispatcher);
}

void X11EventSourceDefault::RemoveXEventDispatcher(
    XEventDispatcher* dispatcher) {
  dispatchers_xevent_.RemoveObserver(dispatcher);
  PlatformEventDispatcher* event_dispatcher =
      dispatcher->GetPlatformEventDispatcher();
  if (event_dispatcher)
    RemovePlatformEventDispatcher(event_dispatcher);
}

void X11EventSourceDefault::AddXEventObserver(XEventObserver* observer) {
  CHECK(observer);
  observers_.AddObserver(observer);
}

void X11EventSourceDefault::RemoveXEventObserver(XEventObserver* observer) {
  CHECK(observer);
  observers_.RemoveObserver(observer);
}

std::unique_ptr<ScopedXEventDispatcher>
X11EventSourceDefault::OverrideXEventDispatcher(XEventDispatcher* dispatcher) {
  CHECK(dispatcher);
  overridden_dispatcher_restored_ = false;
  return std::make_unique<ScopedXEventDispatcher>(&overridden_dispatcher_,
                                                  dispatcher);
}

void X11EventSourceDefault::RestoreOverridenXEventDispatcher() {
  CHECK(overridden_dispatcher_);
  overridden_dispatcher_restored_ = true;
}

void X11EventSourceDefault::ProcessXEvent(XEvent* xevent) {
  auto translated_event = ui::BuildEventFromXEvent(*xevent);
  if (translated_event) {
#if defined(OS_CHROMEOS)
    if (translated_event->IsLocatedEvent()) {
      ui::CursorController::GetInstance()->SetCursorLocation(
          translated_event->AsLocatedEvent()->location_f());
    }
#endif
    DispatchPlatformEvent(translated_event.get(), xevent);
  } else {
    // Only if we can't translate XEvent into ui::Event, try to dispatch XEvent
    // directly to XEventDispatchers.
    DispatchXEventToXEventDispatchers(xevent);
  }
}

void X11EventSourceDefault::AddEventWatcher() {
  if (initialized_)
    return;
  if (!base::MessageLoopCurrent::Get())
    return;

  int fd = ConnectionNumber(event_source_.display());
  base::MessageLoopCurrentForUI::Get()->WatchFileDescriptor(
      fd, true, base::MessagePumpForUI::WATCH_READ, &watcher_controller_, this);
  initialized_ = true;
}

void X11EventSourceDefault::DispatchPlatformEvent(const PlatformEvent& event,
                                                  XEvent* xevent) {
  // First, tell the XEventDispatchers, which can have PlatformEventDispatcher,
  // an ui::Event is going to be sent next. It must make a promise to handle
  // next translated |event| sent by PlatformEventSource based on a XID in
  // |xevent| tested in CheckCanDispatchNextPlatformEvent(). This is needed
  // because it is not possible to access |event|'s associated NativeEvent* and
  // check if it is the event's target window (XID).
  for (XEventDispatcher& dispatcher : dispatchers_xevent_)
    dispatcher.CheckCanDispatchNextPlatformEvent(xevent);

  DispatchEvent(event);

  // Explicitly reset a promise to handle next translated event.
  for (XEventDispatcher& dispatcher : dispatchers_xevent_)
    dispatcher.PlatformEventDispatchFinished();
}

void X11EventSourceDefault::DispatchXEventToXEventDispatchers(XEvent* xevent) {
  bool stop_dispatching = false;

  for (auto& observer : observers_)
    observer.WillProcessXEvent(xevent);

  if (overridden_dispatcher_) {
    stop_dispatching = overridden_dispatcher_->DispatchXEvent(xevent);
  }

  if (!stop_dispatching) {
    for (XEventDispatcher& dispatcher : dispatchers_xevent_) {
      if (dispatcher.DispatchXEvent(xevent))
        break;
    }
  }

  for (auto& observer : observers_)
    observer.DidProcessXEvent(xevent);

  // If an overridden dispatcher has been destroyed, then the event source
  // should halt dispatching the current stream of events, and wait until the
  // next message-loop iteration for dispatching events. This lets any nested
  // message-loop to unwind correctly and any new dispatchers to receive the
  // correct sequence of events.
  if (overridden_dispatcher_restored_)
    StopCurrentEventStream();

  overridden_dispatcher_restored_ = false;
}

void X11EventSourceDefault::StopCurrentEventStream() {
  event_source_.StopCurrentEventStream();
}

void X11EventSourceDefault::OnDispatcherListChanged() {
  AddEventWatcher();
  event_source_.OnDispatcherListChanged();
}

void X11EventSourceDefault::OnFileCanReadWithoutBlocking(int fd) {
  event_source_.DispatchXEvents();
}

void X11EventSourceDefault::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

void XEventDispatcher::CheckCanDispatchNextPlatformEvent(XEvent* xev) {}

void XEventDispatcher::PlatformEventDispatchFinished() {}

PlatformEventDispatcher* XEventDispatcher::GetPlatformEventDispatcher() {
  return nullptr;
}

}  // namespace ui
