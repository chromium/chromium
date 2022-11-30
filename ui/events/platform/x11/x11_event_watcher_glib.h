// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_PLATFORM_X11_X11_EVENT_WATCHER_GLIB_H_
#define UI_EVENTS_PLATFORM_X11_X11_EVENT_WATCHER_GLIB_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/events/platform/x11/x11_event_source.h"

using GPollFD = struct _GPollFD;
using GSource = struct _GSource;

namespace ui {

// A X11EventWatcher implementation which uses a Glib GSource API to be
// notified for incoming XEvents. It must run in the browser process' main
// thread when use_glib GN flag is set (i.e: MessagePumpGlib is in place).
class X11EventWatcherGlib : public X11EventWatcher {
 public:
  explicit X11EventWatcherGlib(X11EventSource* source);

  X11EventWatcherGlib(const X11EventWatcherGlib&) = delete;
  X11EventWatcherGlib& operator=(const X11EventWatcherGlib&) = delete;

  ~X11EventWatcherGlib() override;

  // X11EventWatcher:
  void StartWatching() override;
  void StopWatching() override;

 private:
  raw_ptr<X11EventSource> event_source_;

  // The GLib event source for X events.
  raw_ptr<GSource> x_source_ = nullptr;

  // The poll attached to |x_source_|.
  std::unique_ptr<GPollFD> x_poll_;

  bool started_ = false;
};

}  // namespace ui

#endif  // UI_EVENTS_PLATFORM_X11_X11_EVENT_WATCHER_GLIB_H_
