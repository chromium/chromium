// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_SAFE_PRESENTER_H_
#define UI_OZONE_PLATFORM_SCENIC_SAFE_PRESENTER_H_

#include <lib/ui/scenic/cpp/session.h>

namespace ui {

using QueuePresentCallback = fit::function<void()>;

// Helper class used to call Present2() in fuchsia.ui.scenic.Session. By
// limiting the number of Present2 calls, SafePresenter ensures that the Session
// will not be shut down, thus, users of SafePresenter should not call Present2
// on their own.
//
// More information can be found in the fuchsia.scenic.scheduling FIDL library,
// in the prediction_info.fidl file.
class SafePresenter {
 public:
  explicit SafePresenter(scenic::Session* session);
  ~SafePresenter();

  SafePresenter(const SafePresenter&) = delete;
  SafePresenter& operator=(const SafePresenter&) = delete;

  // If possible, QueuePresent() immediately presents to the underlying Session.
  // If the maximum amount of pending Present2()s has been reached,
  // SafePresenter presents at the next earliest possible time. QueuePresent()
  // ensures that callbacks get processed in FIFO order.
  void QueuePresent();

 private:
  void QueuePresentHelper();
  void OnFramePresented(fuchsia::scenic::scheduling::FramePresentedInfo info);

  scenic::Session* const session_ = nullptr;

  // |presents_allowed_| is true if Scenic allows at least one more Present2()
  // call. Scenic ensures a session will have a Present2 budget of at least 1 to
  // begin with.
  bool presents_allowed_ = true;

  // |present_queued_| is true if there are unhandled QueuePresent() calls.
  bool present_queued_ = false;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_SAFE_PRESENTER_H_
