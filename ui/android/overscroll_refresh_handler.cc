// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/overscroll_refresh_handler.h"

#include <utility>

#include "base/android/jni_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/android/overscroll_refresh.h"
#include "ui/android/ui_android_jni_headers/OverscrollRefreshHandler_jni.h"
#include "ui/events/back_gesture_event.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace ui {

OverscrollRefreshHandler::OverscrollRefreshHandler(
    const JavaRef<jobject>& j_overscroll_refresh_handler) {
  if (j_overscroll_refresh_handler) {
    has_handler_ = true;
    auto* env = AttachCurrentThread();
    Java_OverscrollRefreshHandler_setRef(env, reinterpret_cast<intptr_t>(this),
                                         j_overscroll_refresh_handler);
  }
}

OverscrollRefreshHandler::~OverscrollRefreshHandler() {
  if (has_handler_) {
    auto* env = AttachCurrentThread();
    Java_OverscrollRefreshHandler_removeRef(env,
                                            reinterpret_cast<intptr_t>(this));
  }
}

bool OverscrollRefreshHandler::PullStart(
    OverscrollAction type,
    std::optional<BackGestureEventSwipeEdge> initiating_edge) {
  CHECK_EQ(type == OverscrollAction::kHistoryNavigation,
           initiating_edge.has_value());
  auto* env = AttachCurrentThread();
  return Java_OverscrollRefreshHandler_start(
      env, GetRefreshHandlerChecked(env), std::to_underlying(type),
      static_cast<int>(initiating_edge ? initiating_edge.value()
                                       : BackGestureEventSwipeEdge::RIGHT));
}

void OverscrollRefreshHandler::PullUpdate(float x_delta, float y_delta) {
  auto* env = AttachCurrentThread();
  Java_OverscrollRefreshHandler_pull(env, GetRefreshHandlerChecked(env),
                                     x_delta, y_delta);
}

void OverscrollRefreshHandler::PullRelease(bool allow_refresh) {
  auto* env = AttachCurrentThread();
  Java_OverscrollRefreshHandler_release(env, GetRefreshHandlerChecked(env),
                                        allow_refresh);
}

void OverscrollRefreshHandler::PullReset() {
  auto* env = AttachCurrentThread();
  Java_OverscrollRefreshHandler_reset(env, GetRefreshHandlerChecked(env));
}

ScopedJavaLocalRef<jobject> OverscrollRefreshHandler::GetRefreshHandlerChecked(
    JNIEnv* env) const {
  auto refresh_handler = Java_OverscrollRefreshHandler_getRef(
      env, reinterpret_cast<intptr_t>(this));
  DCHECK(!refresh_handler.is_null());
  return refresh_handler;
}

}  // namespace ui

DEFINE_JNI(OverscrollRefreshHandler)
