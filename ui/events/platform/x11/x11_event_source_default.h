// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_PLATFORM_X11_X11_EVENT_SOURCE_DEFAULT_H_
#define UI_EVENTS_PLATFORM_X11_X11_EVENT_SOURCE_DEFAULT_H_

#include "base/macros.h"
#include "base/message_loop/message_pump_for_ui.h"
#include "base/observer_list.h"
#include "ui/events/events_export.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/x11/x11_event_source.h"

namespace ui {

// PlatformEventSource implementation which uses MessagePumpForUI::FdWatcher to
// be notified about incoming XEvents and converts XEvents to ui::Events before
// dispatching. For X11 specific events a separate list of XEventDispatchers is
// maintained.
class EVENTS_EXPORT X11EventSourceDefault
    : public X11EventSourceDelegate,
      public PlatformEventSource,
      public base::MessagePumpForUI::FdWatcher {
 public:
  explicit X11EventSourceDefault(XDisplay* display);
  ~X11EventSourceDefault() override;

  static X11EventSourceDefault* GetInstance();

  // X11EventSourceDelegate:
  void ProcessXEvent(XEvent* xevent) override;
  void AddXEventDispatcher(XEventDispatcher* dispatcher) override;
  void RemoveXEventDispatcher(XEventDispatcher* dispatcher) override;
  void AddXEventObserver(XEventObserver* observer) override;
  void RemoveXEventObserver(XEventObserver* observer) override;
  std::unique_ptr<ScopedXEventDispatcher> OverrideXEventDispatcher(
      XEventDispatcher* dispatcher) override;
  void RestoreOverridenXEventDispatcher() override;

 private:
  // Registers event watcher with Libevent.
  void AddEventWatcher();

  // Tells XEventDispatchers, which can also have PlatformEventDispatchers, that
  // a translated event is going to be sent next, then dispatches the event and
  // notifies XEventDispatchers the event has been sent out and, most probably,
  // consumed.
  void DispatchPlatformEvent(const PlatformEvent& event, XEvent* xevent);

  // Sends XEvent to registered XEventDispatchers.
  void DispatchXEventToXEventDispatchers(XEvent* xevent);

  // PlatformEventSource:
  void StopCurrentEventStream() override;
  void OnDispatcherListChanged() override;

  // base::MessagePumpForUI::FdWatcher:
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  X11EventSource event_source_;

  // Keep track of all XEventDispatcher to send XEvents directly to.
  base::ObserverList<XEventDispatcher>::Unchecked dispatchers_xevent_;

  base::MessagePumpForUI::FdWatchController watcher_controller_;
  bool initialized_ = false;

  base::ObserverList<XEventObserver>::Unchecked observers_;

  XEventDispatcher* overridden_dispatcher_ = nullptr;
  bool overridden_dispatcher_restored_ = false;

  DISALLOW_COPY_AND_ASSIGN(X11EventSourceDefault);
};

}  // namespace ui

#endif  // UI_EVENTS_PLATFORM_X11_X11_EVENT_SOURCE_DEFAULT_H_
