// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_EVENT_WATCHER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_EVENT_WATCHER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_for_ui.h"

struct wl_display;
struct wl_event_queue;

namespace ui {

// WaylandEventWatcher is a base class that provides a read/prepare/dispatch
// functionality to derived WaylandEventWatcherFDWatch and
// WaylandEventWatcherGlib classes. These classes serve a single purpose - they
// use libevent or libglib (depends on the build configuration of Chromium) to
// watch a Wayland file descriptor and get notified when they can read events
// from an event queue. They also strictly follow a strict prepare/read/dispatch
// dance to ensure Wayland client event loop's integration into the previously
// mentioned event loop libraries is correct. The events that the instance of
// this class dispatches trigger input objects (e.g: WaylandPointer,
// WaylandKeyboard, and others) callbacks, indirectly leading to calls into
// WaylandEventSource, so feeding the platform events pipeline.
class WaylandEventWatcher {
 public:
  WaylandEventWatcher(const WaylandEventWatcher&) = delete;
  WaylandEventWatcher& operator=(const WaylandEventWatcher&) = delete;
  virtual ~WaylandEventWatcher();

  static std::unique_ptr<WaylandEventWatcher> CreateWaylandEventWatcher(
      wl_display* display,
      wl_event_queue* event_queue);

  // Sets a callback that that shutdowns the browser in case of unrecoverable
  // error. Can only be set once.
  void SetShutdownCb(base::OnceCallback<void()> shutdown_cb);

  // Starts polling for events from the wayland connection file descriptor.
  // This method assumes connection is already estabilished and input objects
  // are already bound and properly initialized.
  void StartProcessingEvents();

  // Calls wl_display_roundtrip_queue. Might be required during initialization
  // of some objects that should block until they are initialized.
  void RoundTripQueue();

 protected:
  WaylandEventWatcher(wl_display* display, wl_event_queue* event_queue);

  // Stops polling for events from input devices.
  void StopProcessingEvents();

  // Starts watching the fd. Returns true on success
  virtual bool StartWatchingFD(int fd) = 0;
  // Stops watching the previously passed fd.
  virtual void StopWatchingFD() = 0;

  // Prepares to read events. Must be called before the event loop goes to
  // sleep. Returns true if prepared and false if prepare to read failed, which
  // means there are events to be dispatched.
  bool WlDisplayPrepareToRead();

  // Read the events. Should be only called after the event loop polled the fd
  // and if WlDisplayPrepareToRead has been called before.
  void WlDisplayReadEvents();

  // Cancels prepare to read.
  void WlDisplayCancelRead();

  // Dispatches all incoming events for objects assigned to the event queue
  // after the events have been read with the WlDisplayReadEvents call. Also,
  // checks for errors by calling WlDisplayCheckForErrors if the Wayland API
  // failed.
  void WlDisplayDispatchPendingQueue();

 private:
  // Checks if |display_| has any error set. If so, |shutdown_cb_| is executed
  // and false is returned.
  void WlDisplayCheckForErrors();

  const raw_ptr<wl_display> display_;          // Owned by WaylandConnection.
  const raw_ptr<wl_event_queue> event_queue_;  // Owned by WaylandConnection.

  bool watching_ = false;
  bool prepared_ = false;

  base::OnceCallback<void()> shutdown_cb_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_EVENT_WATCHER_H_
