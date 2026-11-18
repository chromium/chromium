// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/overscroll_refresh_handler.h"

#include <cstdint>

#include "base/android/jni_android.h"
#include "base/check.h"
#include "base/check_op.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/android/ui_android_jni_headers/OverscrollRefreshHandlerHolder_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;

namespace ui {

OverscrollRefreshHandler::OverscrollRefreshHandler(
    const JavaRef<jobject>& j_overscroll_refresh_handler) {
  if (j_overscroll_refresh_handler) {
    has_handler_ = true;
    Java_OverscrollRefreshHandlerHolder_setRef(AttachCurrentThread(),
                                               reinterpret_cast<intptr_t>(this),
                                               j_overscroll_refresh_handler);
  }
}

OverscrollRefreshHandler::~OverscrollRefreshHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (has_handler_) {
    Java_OverscrollRefreshHandlerHolder_removeRef(
        AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
  }
}

bool OverscrollRefreshHandler::PullStart(
    OverscrollAction type,
    std::optional<BackGestureEventSwipeEdge> initiating_edge) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(type == OverscrollAction::kHistoryNavigation,
           initiating_edge.has_value());
  CHECK(has_handler_);
  return Java_OverscrollRefreshHandlerHolder_onPullStart(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this), type,
      initiating_edge.value_or(BackGestureEventSwipeEdge::RIGHT));
}

void OverscrollRefreshHandler::PullUpdate(float x_delta, float y_delta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(has_handler_);
  Java_OverscrollRefreshHandlerHolder_onPullUpdate(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this), x_delta,
      y_delta);
}

void OverscrollRefreshHandler::PullRelease(OverscrollActivationStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(has_handler_);
  Java_OverscrollRefreshHandlerHolder_onPullRelease(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this), status);
}

void OverscrollRefreshHandler::PullReset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(has_handler_);
  Java_OverscrollRefreshHandlerHolder_onPullReset(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

}  // namespace ui

DEFINE_JNI(OverscrollRefreshHandlerHolder)
