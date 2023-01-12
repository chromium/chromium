// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_PLATFORM_X11_X11_EVENT_SOURCE_H_
#define UI_EVENTS_PLATFORM_X11_X11_EVENT_SOURCE_H_

#include <stdint.h>

#include <memory>

#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/events_export.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"

namespace gfx {
class Point;
}

namespace x11 {
class XScopedEventSelector;
}

namespace ui {

class X11HotplugEventHandler;

// Responsible for notifying X11EventSource when new x11::Events are available
// to be processed/dispatched.
class X11EventWatcher {
 public:
  X11EventWatcher() = default;

  X11EventWatcher(const X11EventWatcher&) = delete;
  X11EventWatcher& operator=(const X11EventWatcher&) = delete;

  virtual ~X11EventWatcher() = default;

  // Starts watching for X Events and feeding them into X11EventSource to be
  // processed, through XEventSource::ProcessXEvent(), as they come in.
  virtual void StartWatching() = 0;

  // Stops watching for X Events.
  virtual void StopWatching() = 0;
};

// PlatformEventSource implementation for X11, both Ozone and non-Ozone.
// Receives X11 events from X11EventWatcher and sends them to registered
// PlatformEventDispatchers/x11::EventObservers. Handles receiving, pre-process,
// translation and post-processing of x11::Events.
class EVENTS_EXPORT X11EventSource : public PlatformEventSource,
                                     x11::EventObserver {
 public:
  explicit X11EventSource(x11::Connection* connection);

  X11EventSource(const X11EventSource&) = delete;
  X11EventSource& operator=(const X11EventSource&) = delete;

  ~X11EventSource() override;

  static bool HasInstance();
  static X11EventSource* GetInstance();

  x11::Connection* connection() { return connection_; }

  // Returns the timestamp of the event currently being dispatched.  Falls back
  // on GetCurrentServerTime() if there's no event being dispatched, or if the
  // current event does not have a timestamp.
  x11::Time GetTimestamp();

  // Returns the root pointer location only if there is an event being
  // dispatched that contains that information.
  absl::optional<gfx::Point> GetRootCursorLocationFromCurrentEvent() const;

  // Explicitly asks the X11 server for the current timestamp, and updates
  // |last_seen_server_time_| with this value.
  x11::Time GetCurrentServerTime();

 private:
  // x11::EventObserver:
  void OnEvent(const x11::Event& event) override;

  // PlatformEventSource:
  void OnDispatcherListChanged() override;

  std::unique_ptr<X11EventWatcher> watcher_;

  // The connection to the X11 server used to receive the events.
  raw_ptr<x11::Connection> connection_;

  // State necessary for UpdateLastSeenServerTime
  bool dummy_initialized_;
  x11::Window dummy_window_;
  x11::Atom dummy_atom_;
  std::unique_ptr<x11::XScopedEventSelector> dummy_window_events_;

  std::unique_ptr<X11HotplugEventHandler> hotplug_event_handler_;
};

}  // namespace ui

#endif  // UI_EVENTS_PLATFORM_X11_X11_EVENT_SOURCE_H_
