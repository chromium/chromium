// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_event_watcher.h"

#include <cstring>
#include <memory>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/task/current_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/events/event.h"
#include "ui/ozone/platform/wayland/common/wayland.h"

namespace ui {

namespace {

void DispatchPending(wl_display* display, wl_event_queue* event_queue) {
  wl_display_dispatch_queue_pending(display, event_queue);
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
        start_processing_events_cb_(std::move(start_processing_events_cb)) {}
  ~WaylandEventWatcherThread() override { Stop(); }

  void Init() override {
    DCHECK(!start_processing_events_cb_.is_null());
    std::move(start_processing_events_cb_).Run();
  }

 private:
  base::OnceClosure start_processing_events_cb_;
};

WaylandEventWatcher::WaylandEventWatcher(wl_display* display,
                                         wl_event_queue* event_queue)
    : controller_(FROM_HERE), display_(display), event_queue_(event_queue) {
  DCHECK(display_);
}

WaylandEventWatcher::~WaylandEventWatcher() = default;

void WaylandEventWatcher::SetShutdownCb(
    base::OnceCallback<void()> shutdown_cb) {
  DCHECK(shutdown_cb_.is_null());
  shutdown_cb_ = std::move(shutdown_cb);
}

void WaylandEventWatcher::StartProcessingEvents() {
  ui_thread_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  if (use_dedicated_polling_thread_ && !thread_) {
    // FD watching will happen on a different thread.
    DETACH_FROM_THREAD(thread_checker_);

    thread_ = std::make_unique<WaylandEventWatcherThread>(
        base::BindOnce(&WaylandEventWatcher::StartProcessingEventsInternal,
                       weak_factory_.GetWeakPtr()));
    base::Thread::Options thread_options;
    thread_options.message_pump_type = base::MessagePumpType::UI;
    thread_options.priority = base::ThreadPriority::DISPLAY;
    if (!thread_->StartWithOptions(std::move(thread_options)))
      LOG(FATAL) << "Failed to create input thread";

  } else if (!use_dedicated_polling_thread_) {
    StartProcessingEventsInternal();
  }
}

void WaylandEventWatcher::StopProcessingEvents() {
  if (!watching_)
    return;

  if (use_dedicated_polling_thread_) {
    watching_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WaylandEventWatcher::StopProcessingEventsInternal,
                       weak_factory_.GetWeakPtr()));
  } else {
    StopProcessingEventsInternal();
  }
}

void WaylandEventWatcher::UseSingleThreadedPollingForTesting() {
  use_dedicated_polling_thread_ = false;
}

void WaylandEventWatcher::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!CheckForErrors()) {
    StopProcessingEventsInternal();
    return;
  }

  if (prepared_) {
    prepared_ = false;
    // Errors will be checked the next time OnFileCanReadWithoutBlocking calls
    // CheckForErrors.
    if (wl_display_read_events(display_) == -1)
      return;
    DispatchPendingQueue();
  }

  MaybePrepareReadQueue();

  if (!prepared_)
    return;

  // Automatic Flush.
  int ret = wl_display_flush(display_);
  if (ret != -1 || errno != EAGAIN)
    return;

  // if all data could not be written, errno will be set to EAGAIN and -1
  // returned. In that case, use poll on the display file descriptor to wait for
  // it to become writable again.
  StartWatchingFd(base::MessagePumpForUI::WATCH_WRITE);
}

void WaylandEventWatcher::OnFileCanWriteWithoutBlocking(int fd) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  int ret = wl_display_flush(display_);
  if (ret != -1 || errno != EAGAIN)
    StartWatchingFd(base::MessagePumpForUI::WATCH_READ);
  else if (ret < 0 && errno != EPIPE && prepared_)
    wl_display_cancel_read(display_);

  // Otherwise just continue watching in the same mode.
}

void WaylandEventWatcher::StartProcessingEventsInternal() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(display_);
  if (watching_)
    return;

  watching_thread_task_runner_ = base::ThreadTaskRunnerHandle::Get();

  DCHECK(display_);
  MaybePrepareReadQueue();
  wl_display_flush(display_);
  StartWatchingFd(base::MessagePumpForUI::WATCH_READ);
}

void WaylandEventWatcher::StopProcessingEventsInternal() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!watching_)
    return;

  DCHECK(base::CurrentUIThread::IsSet());
  watching_ = !controller_.StopWatchingFileDescriptor();
  DCHECK(!watching_);
}

void WaylandEventWatcher::StartWatchingFd(
    base::WatchableIOMessagePumpPosix::Mode mode) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (watching_) {
    // Stop watching first.
    watching_ = !controller_.StopWatchingFileDescriptor();
    DCHECK(!watching_);
  }

  DCHECK(base::CurrentUIThread::IsSet());
  int display_fd = wl_display_get_fd(display_);
  watching_ = base::CurrentUIThread::Get()->WatchFileDescriptor(
      display_fd, true, mode, &controller_, this);
  CHECK(watching_) << "Unable to start watching the wl_display's file "
                      "descriptor.";
}

void WaylandEventWatcher::MaybePrepareReadQueue() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (prepared_)
    return;

  if (wl_display_prepare_read_queue(display_, event_queue_) != -1) {
    prepared_ = true;
    return;
  }
  // Nothing to read, send events to the queue.
  DispatchPendingQueue();
}

void WaylandEventWatcher::DispatchPendingQueue() {
  if (ui_thread_task_runner_->BelongsToCurrentThread()) {
    DCHECK(!use_dedicated_polling_thread_);
    DispatchPending(display_, event_queue_);
  } else {
    DCHECK(use_dedicated_polling_thread_);
    auto cb = base::BindOnce(&DispatchPending, display_, event_queue_);
    ui_thread_task_runner_->PostTask(FROM_HERE, std::move(cb));
  }
}

bool WaylandEventWatcher::CheckForErrors() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Errors are fatal. If this function returns non-zero the display can no
  // longer be used.
  int err = wl_display_get_error(display_);

  // TODO(crbug.com/1172305): Wayland display_error message should be printed
  // automatically by wl_log(). However, wl_log() does not print anything. Needs
  // investigation.
  if (err) {
    // When |err| is EPROTO, we can still use the |display_| to retrieve the
    // protocol error. Otherwise, get the error string from strerror and
    // shutdown the browser.
    if (err == EPROTO) {
      uint32_t ec, id;
      const struct wl_interface* intf;
      ec = wl_display_get_protocol_error(display_, &intf, &id);
      if (intf) {
        LOG(ERROR) << "Fatal Wayland protocol error " << ec << " on interface "
                   << intf->name << " (object " << id << "). Shutting down..";
      } else {
        LOG(ERROR) << "Fatal Wayland protocol error " << ec
                   << ". Shutting down..";
      }
    } else {
      LOG(ERROR) << "Fatal Wayland communication error: " << std::strerror(err);
    }

    // This can be null in tests.
    if (!shutdown_cb_.is_null())
      std::move(shutdown_cb_).Run();
    return false;
  }

  return true;
}

}  // namespace ui
