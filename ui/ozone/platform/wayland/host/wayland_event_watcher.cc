// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_event_watcher.h"

#include <wayland-client-core.h>
#include <cstring>

#include "base/environment.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/strings/stringprintf.h"
#include "components/crash/core/common/crash_key.h"
#include "third_party/re2/src/re2/re2.h"

namespace ui {

namespace {

// Formats the |message| by removing '@' char followed by any digits until the
// next char. Also removes new line, tab and carriage-return.
void FormatErrorMessage(std::string* message) {
  const re2::RE2 kInvalidChars[] = {"\n", "\r", "\t", "[@]+[0-9]+"};
  if (message) {
    for (const auto& pattern : kInvalidChars)
      re2::RE2::Replace(message, pattern, "");
  }
}

// Wayland error log that will be stored if the client (Chromium) is
// disconnected due to a protocol error.
static std::string* g_error_log = nullptr;

void wayland_log(const char* fmt, va_list argp) {
  DCHECK(!g_error_log);
  g_error_log = new std::string(base::StringPrintV(fmt, argp));
  LOG(ERROR) << "libwayland: " << *g_error_log;
  // Format the error message only after it's printed. Otherwise, object id will
  // be lost and local development and debugging will be harder to do.
  FormatErrorMessage(g_error_log);
}

std::string GetWaylandProtocolError(int err, wl_display* display) {
  std::string error_string;
  if (err == EPROTO) {
    uint32_t ec, id;
    const struct wl_interface* intf;
    ec = wl_display_get_protocol_error(display, &intf, &id);
    if (intf) {
      error_string = base::StringPrintf(
          "Fatal Wayland protocol error %u on interface %s (object %u). "
          "Shutting down..",
          ec, intf->name, id);
    } else {
      error_string = base::StringPrintf(
          "Fatal Wayland protocol error %u. Shutting down..", ec);
    }
  } else {
    error_string = base::StringPrintf("Fatal Wayland communication error: %s.",
                                      std::strerror(err));
  }
  LOG(ERROR) << error_string;
  // Format the error message only after it's printed. Otherwise, object id will
  // be lost and local development and debugging will be harder to do.
  FormatErrorMessage(&error_string);
  return error_string;
}

void RecordCrashKeys(const std::string& error_string) {
  static crash_reporter::CrashKeyString<256> error("wayland_error");
  error.Set(error_string);

  static crash_reporter::CrashKeyString<32> compositor("wayland_compositor");
  std::string compositor_name("Unknown");
  base::Environment::Create()->GetVar(base::nix::kXdgCurrentDesktopEnvVar,
                                      &compositor_name);
  compositor.Set(compositor_name);
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
    std::string error_string;
    // It's not expected that Wayland doesn't send a wayland log. However, to
    // avoid any possible cases (which are unknown. The unittests exercise all
    // the known ways how a Wayland compositor sends errors) when g_error_log
    // is NULL, get protocol errors (though, without an explicit description of
    // an error) from the display.
    if (g_error_log) {
      error_string = *g_error_log;
      delete g_error_log;
      g_error_log = nullptr;
    } else {
      error_string = GetWaylandProtocolError(err, display_);
    }

    // Record the Wayland compositor name as well as the protocol error message
    // into crash keys, so we can figure out why it is happening.
    RecordCrashKeys(error_string);

    // This can be null in tests.
    if (!shutdown_cb_.is_null()) {
      // If Wayland compositor died, it'll be shutdown gracefully. In all the
      // other cases, force a crash so that a crash report is generated.
      CHECK(err == EPIPE || err == ECONNRESET) << "Wayland protocol error.";
      std::move(shutdown_cb_).Run();
    }
    StopProcessingEvents();
  }
}

}  // namespace ui
