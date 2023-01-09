// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCELERATED_WIDGET_MAC_WINDOW_RESIZE_HELPER_MAC_H_
#define UI_ACCELERATED_WIDGET_MAC_WINDOW_RESIZE_HELPER_MAC_H_

#include "base/lazy_instance.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac_export.h"

namespace base {
class TimeDelta;
class WaitableEvent;
}  // namespace base

namespace ui {

// WindowResizeHelperMac is used to make resize appear smooth. That is to
// say, make sure that the window size and the size of the contents being drawn
// in that window are resized in lock-step. This is accomplished by waiting
// inside AppKit drawing routines on the UI thread for the compositor to produce
// a frame of same size as the NSView that hosts an AcceleratedWidgetMac. When a
// resize occurs, the view controller can wait for a frame of the correct size
// by calling WindowResizeHelperMac::WaitForSingleTaskToRun() until a timeout
// occurs, or the corresponding AcceleratedWidgetMac has a renderer frame of the
// same size as its NSView.
//
// By posting tasks to the custom task_runner(), other threads indicate tasks
// that are required to pick up a new frame. In the ordinary run of things these
// would be posted to the |target_task_runner|; the UI thread given as an
// argument to Init(). Posting instead to task_runner() will cause the tasks to
// be posted to the UI thread (as usual), and will also enqueue them into a
// queue which will be read and run in WaitForSingleTaskToRun(), potentially
// before the task posted to the UI thread is run. Some care is taken (see
// WrappedTask) to make sure that the messages are only executed once.
//
// This is further complicated because, in order for a frame to appear, it is
// also necessary to run tasks posted by the ui::Compositor. To accomplish this,
// the task_runner() that WindowResizeHelperMac provides can be used to
// construct a ui::Compositor. When the Compositor posts tasks to it, they are
// enqueued in the aforementioned queue, which may be pumped by
// WindowResizeHelperMac::WaitForSingleTaskToRun().
class ACCELERATED_WIDGET_MAC_EXPORT WindowResizeHelperMac {
 public:
  static WindowResizeHelperMac* Get();

  WindowResizeHelperMac(const WindowResizeHelperMac&) = delete;
  WindowResizeHelperMac& operator=(const WindowResizeHelperMac&) = delete;

  // Initializes the pumpable task_runner(), providing it with the task runner
  // for UI thread tasks. task_runner() will be null before Init() is called,
  // and WaitForSingleTaskToRun() will immediately return false.
  void Init(
      const scoped_refptr<base::SingleThreadTaskRunner>& target_task_runner);

  // Because this class is global, many tests may want to do this setup
  // repeatedly, so need some way to uninitialize as well.
  void ShutdownForTests();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const;

  // UI THREAD ONLY -----------------------------------------------------------

  // Waits at most |max_delay| for a task to run. Returns true if a task ran,
  // false if no task ran.
  bool WaitForSingleTaskToRun(const base::TimeDelta& max_delay);

 private:
  friend struct base::LazyInstanceTraitsBase<WindowResizeHelperMac>;
  WindowResizeHelperMac();
  ~WindowResizeHelperMac();

  // This helper is needed to create a ScopedAllowWait inside the scope of a
  // class where it is allowed.
  static void EventTimedWait(base::WaitableEvent* event, base::TimeDelta delay);

  // The task runner to which the helper will post tasks. This also maintains
  // the task queue and does the actual work for WaitForSingleTaskToRun.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace ui

#endif  // UI_ACCELERATED_WIDGET_MAC_WINDOW_RESIZE_HELPER_MAC_H_
