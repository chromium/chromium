// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_event_watcher.h"

#include <wayland-client-core.h>
#include <cstring>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "components/crash/core/common/crash_key.h"

namespace ui {

namespace {

void wayland_log(const char* fmt, va_list argp) {
  LOG(ERROR) << "libwayland: " << base::StringPrintV(fmt, argp);
}

}  // namespace

WaylandEventWatcher::WaylandEventWatcher(wl_display* display,
                                         wl_event_queue* event_queue)
    : display_(display), event_queue_(event_queue) {
  DCHECK(display_);
}

WaylandEventWatcher::~WaylandEventWatcher() {
  DCHECK(!watching_);
}

void WaylandEventWatcher::SetShutdownCb(
    base::OnceCallback<void()> shutdown_cb) {
  DCHECK(shutdown_cb_.is_null());
  shutdown_cb_ = std::move(shutdown_cb);
}

void WaylandEventWatcher::StartProcessingEvents() {
  if (watching_)
    return;

  // Set the log handler right before starting to watch the fd so that Wayland
  // is able to send us nicely formatted error messages.
  wl_log_set_handler_client(wayland_log);

  DCHECK(display_);
  watching_ = StartWatchingFD(wl_display_get_fd(display_));
  CHECK(watching_) << "Unable to start watching the wl_display's file "
                      "descriptor.";
}

void WaylandEventWatcher::RoundTripQueue() {
  // Read must be cancelled. Otherwise, wl_display_roundtrip_queue might block
  // as its internal implementation also reads events, which may block if there
  // are more than one preparation for reading within the same thread.
  //
  // TODO(crbug.com/1288181): this won't be needed once libevent is updated. See
  // WaylandEventWatcherFdWatch::OnFileCanReadWithoutBlocking for more details.
  WlDisplayCancelRead();
  wl_display_roundtrip_queue(display_, event_queue_);
}

void WaylandEventWatcher::StopProcessingEvents() {
  if (!watching_)
    return;

  // Cancel read before stopping to watch.
  if (prepared_)
    WlDisplayCancelRead();

  StopWatchingFD();

  watching_ = false;
}

bool WaylandEventWatcher::WlDisplayPrepareToRead() {
  if (prepared_)
    return true;

  // Nothing to read. According to the spec, we must notify the caller it must
  // dispatch the events.
  if (wl_display_prepare_read_queue(display_, event_queue_) != 0)
    return false;

  prepared_ = true;

  // Automatic flush.
  wl_display_flush(display_);

  return true;
}

void WaylandEventWatcher::WlDisplayReadEvents() {
  if (!prepared_)
    return;

  prepared_ = false;

  wl_display_read_events(display_);
}

void WaylandEventWatcher::WlDisplayCancelRead() {
  if (!prepared_)
    return;

  prepared_ = false;

  wl_display_cancel_read(display_);
}

void WaylandEventWatcher::WlDisplayDispatchPendingQueue() {
  // If the dispatch fails, it must set the errno. Check that and stop the
  // browser as this is an unrecoverable error.
  if (wl_display_dispatch_queue_pending(display_, event_queue_) < 0)
    WlDisplayCheckForErrors();
}

void WaylandEventWatcher::WlDisplayCheckForErrors() {
  // Errors are fatal. If this function returns non-zero the display can no
  // longer be used.
  if (int err = wl_display_get_error(display_)) {
    // When |err| is EPROTO, we can still use the |display_| to retrieve the
    // protocol error. Otherwise, get the error string from strerror and
    // shutdown the browser.
    std::string error_string;
    if (err == EPROTO) {
      uint32_t ec, id;
      const struct wl_interface* intf;
      ec = wl_display_get_protocol_error(display_, &intf, &id);
      if (intf) {
        error_string = base::StringPrintf(
            "Fatal Wayland protocol error %u on interface %s (object %u). "
            "Shutting down..",
            ec, intf->name, id);
        LOG(ERROR) << error_string;
      } else {
        error_string = base::StringPrintf(
            "Fatal Wayland protocol error %u. Shutting down..", ec);
        LOG(ERROR) << error_string;
      }
    } else {
      error_string = base::StringPrintf("Fatal Wayland communication error %s.",
                                        std::strerror(err));
      LOG(ERROR) << error_string;
    }

    // Add a crash key so we can figure out why this is happening.
    static crash_reporter::CrashKeyString<256> wayland_error("wayland_error");
    wayland_error.Set(error_string);

    // This can be null in tests.
    if (!shutdown_cb_.is_null()) {
      // Force a crash so that a crash report is generated.
      CHECK(err == EPIPE || err == ECONNRESET) << "Wayland protocol error.";
      std::move(shutdown_cb_).Run();
    }
    StopProcessingEvents();
  }
}

}  // namespace ui
