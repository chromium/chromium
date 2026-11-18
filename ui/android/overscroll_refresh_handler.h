// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_OVERSCROLL_REFRESH_HANDLER_H_
#define UI_ANDROID_OVERSCROLL_REFRESH_HANDLER_H_

#include <optional>

#include "base/android/scoped_java_ref.h"
#include "base/sequence_checker.h"
#include "ui/android/overscroll_refresh.h"
#include "ui/android/ui_android_export.h"
#include "ui/events/back_gesture_event.h"

namespace ui {

// Must be constructed, used, and destroyed on the same sequence (the UI thread
// when backed by a Java handler).
class UI_ANDROID_EXPORT OverscrollRefreshHandler {
 public:
  explicit OverscrollRefreshHandler(
      const base::android::JavaRef<jobject>& j_overscroll_refresh_handler);

  OverscrollRefreshHandler(const OverscrollRefreshHandler&) = delete;
  OverscrollRefreshHandler& operator=(const OverscrollRefreshHandler&) = delete;

  // Note: the following methods are virtual because this class is overridden
  // for testing in overscroll_refresh_unittest.cc

  virtual ~OverscrollRefreshHandler();

  // Signals the start of an overscrolling pull. Returns whether the handler
  // will consume the overscroll gesture, in which case it will receive the
  // remaining pull updates.
  virtual bool PullStart(
      OverscrollAction type,
      std::optional<BackGestureEventSwipeEdge> initiating_edge);

  // Signals a pull update, where |x_delta| and |y_delta| are in device pixels.
  virtual void PullUpdate(float x_delta, float y_delta);

  // Signals the release of the pull, and whether the release is allowed to or
  // force to trigger the refresh action.
  virtual void PullRelease(OverscrollActivationStatus status);

  // Reset the active pull state.
  virtual void PullReset();

 private:
  bool has_handler_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ui

#endif  // UI_ANDROID_OVERSCROLL_REFRESH_HANDLER_H_
