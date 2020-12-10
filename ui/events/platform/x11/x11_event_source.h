// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_PLATFORM_X11_X11_EVENT_SOURCE_H_
#define UI_EVENTS_PLATFORM_X11_X11_EVENT_SOURCE_H_

#include <stdint.h>

#include <memory>

#include "base/auto_reset.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "ui/events/events_export.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"

namespace gfx {
class Point;
}

namespace ui {

class X11HotplugEventHandler;
class XScopedEventSelector;
class PlatformEventDispatcher;

// The XEventDispatcher interface is used by classes wanting to receive
// x11::Events directly
class EVENTS_EXPORT XEventDispatcher {
 public:
  // Sends x11::Event to XEventDispatcher for handling. Returns true if the
  // x11::Event was dispatched, otherwise false. After the first
  // XEventDispatcher returns true x11::Event dispatching stops.
  virtual bool DispatchXEvent(x11::Event* xevent) = 0;

 protected:
  virtual ~XEventDispatcher() = default;
};

// XEventObserver can be installed on a X11EventSource, and it
// receives all events that are dispatched to the dispatchers.
class EVENTS_EXPORT XEventObserver {
 public:
  // Called before the dispatchers receive the event.
  virtual void WillProcessXEvent(x11::Event* event) {}

 protected:
  virtual ~XEventObserver() = default;
};

// Responsible for notifying X11EventSource when new x11::Events are available
// to be processed/dispatched.
class X11EventWatcher {
 public:
  X11EventWatcher() = default;
  virtual ~X11EventWatcher() = default;

  // Starts watching for X Events and feeding them into X11EventSource to be
  // processed, through XEventSource::ProcessXEvent(), as they come in.
  virtual void StartWatching() = 0;

  // Stops watching for X Events.
  virtual void StopWatching() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(X11EventWatcher);
};

// PlatformEventSource implementation for X11, both Ozone and non-Ozone.
// Receives X11 events from X11EventWatcher and sends them to registered
// {Platform,X}EventDispatchers. Handles receiving, pre-process, translation
// and post-processing of XEvents.
class EVENTS_EXPORT X11EventSource : public PlatformEventSource,
                                     public x11::Connection::Delegate {
 public:
  explicit X11EventSource(x11::Connection* connection);
  ~X11EventSource() override;

  static bool HasInstance();
  static X11EventSource* GetInstance();

  // Called when there is a new XEvent available. Processes all (if any)
  // available X events.
  void DispatchXEvents();

  x11::Connection* connection() { return connection_; }

  x11::Event* dispatching_event() { return dispatching_event_; }

  // Returns the timestamp of the event currently being dispatched.  Falls back
  // on GetCurrentServerTime() if there's no event being dispatched, or if the
  // current event does not have a timestamp.
  x11::Time GetTimestamp();

  // Returns the root pointer location only if there is an event being
  // dispatched that contains that information.
  base::Optional<gfx::Point> GetRootCursorLocationFromCurrentEvent() const;

  // Explicitly asks the X11 server for the current timestamp, and updates
  // |last_seen_server_time_| with this value.
  x11::Time GetCurrentServerTime();

  // Adds a x11::Event dispatcher to the x11::Event dispatcher list.
  // Also calls XEventDispatcher::GetPlatformEventDispatcher
  // to explicitly add this |dispatcher| to a list of PlatformEventDispatchers
  // in case if XEventDispatcher has a PlatformEventDispatcher. Thus,
  // there is no need to separately add self to the list of
  // PlatformEventDispatchers. This is needed because XEventDispatchers are
  // tested if they can receive an x11::Event based on a x11::Window target. If
  // so, the translated x11::Event into a PlatformEvent is sent to that
  // PlatformEventDispatcher.
  void AddXEventDispatcher(XEventDispatcher* dispatcher);

  // Removes an x11::Event dispatcher from the x11::Event dispatcher list.
  // Also explicitly removes an XEventDispatcher from a PlatformEventDispatcher
  // list if the XEventDispatcher has a PlatformEventDispatcher.
  void RemoveXEventDispatcher(XEventDispatcher* dispatcher);

  void AddXEventObserver(XEventObserver* observer);
  void RemoveXEventObserver(XEventObserver* observer);

  // x11::Connection::Delegate:
  bool ShouldContinueStream() const override;
  void DispatchXEvent(x11::Event* event) override;

 protected:
  // Handles updates after event has been dispatched.
  void PostDispatchEvent(x11::Event* xevent);

 private:
  void ProcessXEvent(x11::Event* xevent);

  // Sends XEvent to registered XEventDispatchers.
  void DispatchXEventToXEventDispatchers(x11::Event* xevent);

  // PlatformEventSource:
  void StopCurrentEventStream() override;
  void OnDispatcherListChanged() override;

  std::unique_ptr<X11EventWatcher> watcher_;

  // The connection to the X11 server used to receive the events.
  x11::Connection* connection_;

  // Event currently being dispatched.
  x11::Event* dispatching_event_;

  // State necessary for UpdateLastSeenServerTime
  bool dummy_initialized_;
  x11::Window dummy_window_;
  x11::Atom dummy_atom_;
  std::unique_ptr<XScopedEventSelector> dummy_window_events_;

  // Keeps track of whether this source should continue to dispatch all the
  // available events.
  bool continue_stream_ = true;

  std::unique_ptr<X11HotplugEventHandler> hotplug_event_handler_;

  // Keep track of all XEventDispatcher to send XEvents directly to.
  base::ObserverList<XEventDispatcher>::Unchecked dispatchers_xevent_;

  base::ObserverList<XEventObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(X11EventSource);
};

}  // namespace ui

#endif  // UI_EVENTS_PLATFORM_X11_X11_EVENT_SOURCE_H_
