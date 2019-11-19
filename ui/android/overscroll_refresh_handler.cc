// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/overscroll_refresh_handler.h"

#include "base/android/jni_android.h"
#include "ui/android/ui_android_jni_headers/OverscrollRefreshHandler_jni.h"

using base::android::AttachCurrentThread;

namespace ui {

OverscrollRefreshHandler::OverscrollRefreshHandler(
    const base::android::JavaRef<jobject>& j_overscroll_refresh_handler) {
  j_overscroll_refresh_handler_.Reset(AttachCurrentThread(),
                                      j_overscroll_refresh_handler.obj());
}

OverscrollRefreshHandler::~OverscrollRefreshHandler() {}

bool OverscrollRefreshHandler::PullStart(OverscrollAction type,
                                         float startx,
                                         float starty,
                                         bool navigate_forward) {
  return Java_OverscrollRefreshHandler_start(
      AttachCurrentThread(), j_overscroll_refresh_handler_, type, startx,
      starty, navigate_forward);
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
