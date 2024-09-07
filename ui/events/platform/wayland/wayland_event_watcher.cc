// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/platform/wayland/wayland_event_watcher.h"

#include <wayland-client-core.h>

#include <cstring>

#include "base/check.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/current_thread.h"
#include "base/threading/thread.h"
#include "components/crash/core/common/crash_key.h"
#include "third_party/re2/src/re2/re2.h"

namespace ui {

namespace {

// Formats the |message| by removing '@' char followed by any digits until the
// next char. Also removes new line, tab and carriage-return.
void FormatErrorMessage(std::string* message) {
  const re2::RE2 kInvalidChars[] = {"\n", "\r", "\t", "[@]+[0-9]+"};
  if (message) {
    for (const auto& pattern : kInvalidChars) {
      re2::RE2::Replace(message, pattern, "");
    }
  }
}

std::optional<std::string>& GetErrorLog() {
  // Wayland error log that will be stored if the client (Chromium) is
  // disconnected due to a protocol error.
  static std::optional<std::string> g_error_log;
  return g_error_log;
}

void wayland_log(const char* fmt, va_list argp) {
  std::string error_log(base::StringPrintV(fmt, argp));
  LOG(ERROR) << "libwayland: " << error_log;
  // Format the error message only after it's printed. Otherwise, object id will
  // be lost and local development and debugging will be harder to do.
  FormatErrorMessage(&error_log);
  if (GetErrorLog().has_value()) {
    GetErrorLog()->append("\n");
    GetErrorLog()->append(std::move(error_log));
  } else {
    GetErrorLog() = std::move(error_log);
  }
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

// A dedicated thread for watching wl_display's file descriptor. The reason why
// watching happens on a separate thread is that the thread mustn't be blocked.
// Otherwise, if Chromium is used with Wayland EGL, a deadlock may happen. The
// deadlock happened when the thread that had been watching the file descriptor
// (it used to be the browser's UI thread) called wl_display_prepare_read, and
// then started to wait until the thread, which was used by the gpu service,
// completed a buffer swap and shutdowned itself (for example, a menu window is
// in the process of closing). However, that gpu thread hanged as it called
// Wayland EGL that also called wl_display_prepare_read internally and started
// to wait until the previous caller of the wl_display_prepare_read (that's by
// the design of the wayland-client library). This situation causes a deadlock
// as the first caller of the wl_display_prepare_read is unable to complete
// reading as it waits for another thread to complete, and that another thread
// is also unable to complete reading as it waits until the first caller reads
// the display's file descriptor. For more details, see the implementation of
// the wl_display_prepare_read in third_party/wayland/src/src/wayland-client.c.
class WaylandEventWatcherThread : public base::Thread {
 public:
  explicit WaylandEventWatcherThread(
      base::OnceClosure start_processing_events_cb)
      : base::Thread("wayland-fd"),
        start_processing_events_cb_(std::move(start_processing_events_cb)) {
    wl_log_set_handler_client(wayland_log);
  }
  ~WaylandEventWatcherThread() override { Stop(); }

  void Init() override {
    DCHECK(!start_processing_events_cb_.is_null());
    std::move(start_processing_events_cb_).Run();
  }

 private:
  base::OnceClosure start_processing_events_cb_;
};

WaylandEventWatcher::WaylandEventWatcher(wl_display* display,
                                         wl_event_queue* event_queue,
                                         bool use_threaded_polling)
    : display_(display),
      event_queue_(event_queue),
      use_threaded_polling_(use_threaded_polling) {
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
  if (watching_) {
    return;
  }

  if (use_threaded_polling_) {
    if (!ui_thread_task_runner_) {
      ui_thread_task_runner_ =
          base::SingleThreadTaskRunner::GetCurrentDefault();
      weak_this_ = weak_factory_.GetWeakPtr();
    }

    if (!thread_) {
      // FD watching will happen on a different thread.
      DETACH_FROM_THREAD(thread_checker_);

      thread_ = std::make_unique<WaylandEventWatcherThread>(
          base::BindOnce(&WaylandEventWatcher::StartProcessingEventsThread,
                         base::Unretained(this)));
      base::Thread::Options thread_options;
      thread_options.message_pump_type = base::MessagePumpType::UI;

      if (!thread_->StartWithOptions(std::move(thread_options))) {
        LOG(FATAL) << "Failed to create input thread";
      }
    } else if (watching_thread_task_runner_) {
      watching_thread_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&WaylandEventWatcher::StartProcessingEventsInternal,
                         base::Unretained(this)));
    }
  } else {
    StartProcessingEventsInternal();
  }
}

void WaylandEventWatcher::StartProcessingEventsInternal() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Set the log handler right before starting to watch the fd so that Wayland
  // is able to send us nicely formatted error messages.
  wl_log_set_handler_client(wayland_log);

  DCHECK(display_);
  watching_ = StartWatchingFD(wl_display_get_fd(display_));
  CHECK(watching_) << "Unable to start watching the wl_display's file "
                      "descriptor.";
}

void WaylandEventWatcher::StartProcessingEventsThread() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  watching_thread_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  WaylandEventWatcher::StartProcessingEventsInternal();
}

void WaylandEventWatcher::RoundTripQueue() {
  // NOTE: When we are using dedicated polling thread, RoundTripQueue
  // expected to be called by different thread from dedicated polling
  // thread so here we can call wl_display_roundtrip_queue directly
  // without canceling read because multiple thread can call
  // wl_display_prepare_read_queue.
  if (!use_threaded_polling_) {
    // Read must be cancelled. Otherwise, wl_display_roundtrip_queue might block
    // as its internal implementation also reads events, which may block if
    // there are more than one preparation for reading within the same thread.
    //
    // TODO(crbug.com/40816750): this won't be needed once libevent is updated.
    // See WaylandEventWatcherFdWatch::OnFileCanReadWithoutBlocking for more
    // details.
    WlDisplayCancelRead();
  }
  wl_display_roundtrip_queue(display_, event_queue_);
}

void WaylandEventWatcher::StopProcessingEvents() {
  if (!watching_) {
    return;
  }

  if (use_threaded_polling_) {
    watching_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WaylandEventWatcher::StopProcessingEventsInternal,
                       base::Unretained(this)));
  } else {
    StopProcessingEventsInternal();
  }
}

void WaylandEventWatcher::StopProcessingEventsInternal() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Cancel read before stopping to watch.
  if (prepared_) {
    WlDisplayCancelRead();
  }

  StopWatchingFD();

  watching_ = false;
}

void WaylandEventWatcher::Flush() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  wl_display_flush(display_);
}

bool WaylandEventWatcher::WlDisplayPrepareToRead() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (prepared_) {
    return true;
  }

  // Nothing to read. According to the spec, we must notify the caller it must
  // dispatch the events.
  if (wl_display_prepare_read_queue(display_, event_queue_) != 0) {
    return false;
  }

  prepared_ = true;

  // Automatic flush.
  Flush();

  return true;
}

void WaylandEventWatcher::WlDisplayReadEvents() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!prepared_) {
    return;
  }

  prepared_ = false;

  wl_display_read_events(display_);
}

void WaylandEventWatcher::WlDisplayCancelRead() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!prepared_) {
    return;
  }

  prepared_ = false;

  wl_display_cancel_read(display_);
}

void WaylandEventWatcher::WlDisplayDispatchPendingQueue() {
  if (use_threaded_polling_ &&
      watching_thread_task_runner_->BelongsToCurrentThread()) {
    base::WaitableEvent event;
    auto cb = base::BindOnce(
        &WaylandEventWatcher::WlDisplayDispatchPendingQueueInternal, weak_this_,
        &event);
    ui_thread_task_runner_->PostTask(FROM_HERE, std::move(cb));
    event.Wait();
  } else {
    WlDisplayDispatchPendingQueueInternal(nullptr);
  }
}

void WaylandEventWatcher::WlDisplayDispatchPendingQueueInternal(
    base::WaitableEvent* event) {
  // wl_display_dispatch_queue_pending may block if dispatching events results
  // in a tab dragging that spins a run loop, which doesn't return until it's
  // over. Thus, signal before this function is called.
  if (event) {
    event->Signal();
  }
  // If the dispatch fails, it must set the errno. Check that and stop the
  // browser as this is an unrecoverable error.
  if (wl_display_dispatch_queue_pending(display_, event_queue_) < 0) {
    WlDisplayCheckForErrors();
  }
}

void WaylandEventWatcher::WlDisplayCheckForErrors() {
  // Errors are fatal. If this function returns non-zero the display can no
  // longer be used.
  if (int err = wl_display_get_error(display_)) {
    std::string error_string;
    // It's not expected that Wayland doesn't send a wayland log. However, to
    // avoid any possible cases (which are unknown. The unittests exercise all
    // the known ways how a Wayland compositor sends errors) when GetErrorLog()
    // returns NULL, get protocol errors (though, without an explicit
    // description of an error) from the display.
    if (GetErrorLog().has_value()) {
      error_string = std::move(*GetErrorLog());
      GetErrorLog().reset();
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
