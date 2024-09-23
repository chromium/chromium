// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/overscroll_refresh_handler.h"

#include "base/android/jni_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/android/overscroll_refresh.h"
#include "ui/android/ui_android_jni_headers/OverscrollRefreshHandler_jni.h"
#include "ui/events/back_gesture_event.h"

using base::android::AttachCurrentThread;

namespace ui {

OverscrollRefreshHandler::OverscrollRefreshHandler(
    const base::android::JavaRef<jobject>& j_overscroll_refresh_handler) {
  j_overscroll_refresh_handler_.Reset(AttachCurrentThread(),
                                      j_overscroll_refresh_handler.obj());
}

OverscrollRefreshHandler::~OverscrollRefreshHandler() {}

bool OverscrollRefreshHandler::PullStart(
    OverscrollAction type,
    std::optional<BackGestureEventSwipeEdge> initiating_edge) {
  CHECK_EQ(type == OverscrollAction::HISTORY_NAVIGATION,
           initiating_edge.has_value());
  return Java_OverscrollRefreshHandler_start(
      AttachCurrentThread(), j_overscroll_refresh_handler_, type,
      static_cast<int>(initiating_edge ? initiating_edge.value()
                                       : BackGestureEventSwipeEdge::RIGHT));
}

void OverscrollRefreshHandler::PullUpdate(float x_delta, float y_delta) {
  Java_OverscrollRefreshHandler_pull(
      AttachCurrentThread(), j_overscroll_refresh_handler_, x_delta, y_delta);
}

void OverscrollRefreshHandler::PullRelease(bool allow_refresh) {
  Java_OverscrollRefreshHandler_release(
      AttachCurrentThread(), j_overscroll_refresh_handler_, allow_refresh);
}

void OverscrollRefreshHandler::PullReset() {
  Java_OverscrollRefreshHandler_reset(AttachCurrentThread(),
                                      j_overscroll_refresh_handler_);
}

}  // namespace ui
