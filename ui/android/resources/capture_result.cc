// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/resources/capture_result.h"

#include <android/hardware_buffer_jni.h>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "ui/gfx/android/java_bitmap.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/android/ui_android_jni_headers/CaptureResult_jni.h"

namespace ui {

using base::android::ScopedHardwareBufferHandle;
using base::android::ScopedJavaGlobalRef;

CaptureResult::CaptureResult(const jni_zero::JavaRef<jobject>& obj)
    : java_capture_result_(obj) {}

CaptureResult::~CaptureResult() = default;

CaptureResult::operator bool() const {
  return !java_capture_result_.is_null();
}

SkBitmap CaptureResult::GetBitmap() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  gfx::JavaBitmap j_bitmap(
      Java_CaptureResult_getBitmap(env, java_capture_result_));
  return gfx::CreateSkBitmapFromJavaBitmap(j_bitmap);
}

ScopedHardwareBufferHandle CaptureResult::GetHardwareBuffer() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto j_hardware_buffer =
      Java_CaptureResult_getHardwareBuffer(env, java_capture_result_);
  return ScopedHardwareBufferHandle::Create(
      AHardwareBuffer_fromHardwareBuffer(env, j_hardware_buffer.obj()));
}

base::ScopedClosureRunner CaptureResult::GetReleaseCallback() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::OnceClosure release_callback =
      Java_CaptureResult_getReleaseCallback(env, java_capture_result_);
  return base::ScopedClosureRunner(std::move(release_callback));
}

}  // namespace ui
