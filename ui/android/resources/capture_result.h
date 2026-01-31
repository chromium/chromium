// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_RESOURCES_CAPTURE_RESULT_H_
#define UI_ANDROID_RESOURCES_CAPTURE_RESULT_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback_forward.h"
#include "base/memory/stack_allocated.h"
#include "third_party/jni_zero/jni_zero.h"
#include "ui/android/ui_android_export.h"

namespace base {
class ScopedClosureRunner;
}  // namespace base

class SkBitmap;

namespace ui {

// This class is a mirror of
// org.chromium.ui.resources.dynamics.CaptureResult.
// It uses ScopedJavaLocalRef, meaning that the class is only usable as
// a stack-based object in a single thread.
class UI_ANDROID_EXPORT CaptureResult {
  STACK_ALLOCATED();

 public:
  explicit CaptureResult(const jni_zero::JavaRef<jobject>& obj);
  CaptureResult(CaptureResult&& other) = delete;
  CaptureResult(const CaptureResult&) = delete;
  CaptureResult operator=(const CaptureResult&) = delete;
  ~CaptureResult();

  // Returns whether there is a non-null result.
  explicit operator bool() const;

  SkBitmap GetBitmap() const;

  base::android::ScopedHardwareBufferHandle GetHardwareBuffer() const;

  base::ScopedClosureRunner GetReleaseCallback() const;

 private:
  base::android::ScopedJavaLocalRef<jobject> java_capture_result_;
};

}  // namespace ui

namespace jni_zero {

template <>
inline ui::CaptureResult FromJniType(JNIEnv* env,
                                     const jni_zero::JavaRef<jobject>& obj) {
  return ui::CaptureResult(obj);
}

}  // namespace jni_zero

#endif  // UI_ANDROID_RESOURCES_CAPTURE_RESULT_H_
