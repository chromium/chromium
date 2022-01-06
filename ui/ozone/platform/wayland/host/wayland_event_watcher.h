// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_EVENT_WATCHER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_EVENT_WATCHER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_pump_for_ui.h"
#include "base/message_loop/watchable_io_message_pump_posix.h"
#include "base/threading/thread_checker.h"

struct wl_display;
struct wl_event_queue;

namespace base {
class Thread;
class SingleThreadTaskRunner;
class WaitableEvent;
}  // namespace base

namespace ui {

// WaylandEventWatcher serves a single purpose: poll for events in the wayland
// connection file descriptor. Which will then trigger input objects (e.g:
// WaylandPointer, WaylandKeyboard, etc) callbacks, indirectly leading to calls
// into WaylandEventSource, so feeding the platform events pipeline.
class WaylandEventWatcher : public base::MessagePumpForUI::FdWatcher {
 public:
  WaylandEventWatcher(wl_display* display, wl_event_queue* event_queue);
  WaylandEventWatcher(const WaylandEventWatcher&) = delete;
  WaylandEventWatcher& operator=(const WaylandEventWatcher&) = delete;
  ~WaylandEventWatcher() override;

  // Sets a callback that that shutdowns the browser in case of unrecoverable
  // error. Can only be set once.
  void SetShutdownCb(base::OnceCallback<void()> shutdown_cb);

  // Starts polling for events from the wayland connection file descriptor.
  // This method assumes connection is already estabilished and input objects
  // are already bound and properly initialized.
  void StartProcessingEvents();

  // Stops polling for events from input devices.
  void StopProcessingEvents();

  // See the comment near WaylandEventWatcher::use_dedicated_polling_thread_.
  void UseSingleThreadedPollingForTesting();

 private:
  // base::MessagePumpForUI::FdWatcher
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  void StartProcessingEventsInternal();
  void StopProcessingEventsInternal();
  void StartWatchingFd(base::WatchableIOMessagePumpPosix::Mode mode);
  void MaybePrepareReadQueue();
  void DispatchPendingQueue();

  // Dispatches pending tasks. Must only be called on main thread.
  // Signals |event| before dispatching the pending tasks.
  void DispatchPendingMainThread(base::WaitableEvent* event);

  // Checks if |display_| has any error set. If so, |shutdown_cb_| is executed
  // and false is returned.
  bool CheckForErrors();

  base::MessagePumpForUI::FdWatchController controller_;

  wl_display* const display_;  // Owned by WaylandConnection.
  wl_event_queue* const event_queue_;  // Owned by WaylandConnection.
  base::WeakPtr<WaylandEventWatcher> weak_this_;  // Bound to the main thread.

  bool watching_ = false;
  bool prepared_ = false;

  // A separate thread is not used in some tests (ozone_unittests), as it
  // requires additional synchronization from the WaylandTest side. Otherwise,
  // some tests complete without waiting until events come. That is, the tests
  // suppose that our calls/requests are completed after calling Sync(), which
  // resumes our fake Wayland server and sends out events, but as long as there
  // is one additional "polling" thread involved, some additional
  // synchronization mechanisms are needed. At this point, it's easier to
  // continue to watch the file descriptor on the same thread where the
  // ozone_unittests run.
  bool use_dedicated_polling_thread_ = true;

  // Set to true when the event watcher begins to shut down. Setting this to
  // true has two effects:
  // (1) the main thread will stop dispatching queued events
  // (2) the polling thread will stop posting tasks to the main thread.
  std::atomic<bool> shutting_down_{false};

  base::OnceCallback<void()> shutdown_cb_;

  // Used to verify watching the fd happens on a valid thread.
  THREAD_CHECKER(thread_checker_);

  // See the |use_dedicated_polling_thread_| and also the comment in the source
  // file for this header.
  // TODO(crbug.com/1117463): consider polling on I/O instead.
  std::unique_ptr<base::Thread> thread_;

  // The original ui task runner where |this| has been created.
  scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner_;

  // The thread's task runner where the wl_display's fd is being watched.
  scoped_refptr<base::SingleThreadTaskRunner> watching_thread_task_runner_;

  // This weak factory must only be accessed on the main thread.
  base::WeakPtrFactory<WaylandEventWatcher> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_EVENT_WATCHER_H_
