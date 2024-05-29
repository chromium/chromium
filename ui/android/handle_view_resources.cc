// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/handle_view_resources.h"

#include "base/trace_event/trace_event.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/android/ui_android_jni_headers/HandleViewResources_jni.h"

namespace {

static SkBitmap CreateSkBitmapFromJavaBitmap(
    base::android::ScopedJavaLocalRef<jobject> jbitmap) {
  return jbitmap.is_null()
             ? SkBitmap()
             : CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(jbitmap));
}

}  // namespace

namespace ui {

HandleViewResources::HandleViewResources() {}

void HandleViewResources::LoadIfNecessary(const JavaRef<jobject>& context) {
  if (loaded_)
    return;

  loaded_ = true;

  TRACE_EVENT0("ui", "HandleViewResources::Create");
  JNIEnv* env = base::android::AttachCurrentThread();

  left_bitmap_ = CreateSkBitmapFromJavaBitmap(
      Java_HandleViewResources_getLeftHandleBitmap(env, context));
  right_bitmap_ = CreateSkBitmapFromJavaBitmap(
      Java_HandleViewResources_getRightHandleBitmap(env, context));
  center_bitmap_ = CreateSkBitmapFromJavaBitmap(
      Java_HandleViewResources_getCenterHandleBitmap(env, context));

  left_bitmap_.setImmutable();
  right_bitmap_.setImmutable();
  center_bitmap_.setImmutable();

  drawable_horizontal_padding_ratio_ =
      Java_HandleViewResources_getHandleHorizontalPaddingRatio(env);
}

const SkBitmap& HandleViewResources::GetBitmap(
    ui::TouchHandleOrientation orientation) {
  DCHECK(loaded_);
  switch (orientation) {
    case ui::TouchHandleOrientation::LEFT:
      return left_bitmap_;
    case ui::TouchHandleOrientation::RIGHT:
      return right_bitmap_;
    case ui::TouchHandleOrientation::CENTER:
      return center_bitmap_;
    case ui::TouchHandleOrientation::UNDEFINED:
      NOTREACHED_IN_MIGRATION() << "Invalid touch handle orientation.";
  };
  return center_bitmap_;
}

float HandleViewResources::GetDrawableHorizontalPaddingRatio() const {
  DCHECK(loaded_);
  return drawable_horizontal_padding_ratio_;
}

}  // namespace ui
