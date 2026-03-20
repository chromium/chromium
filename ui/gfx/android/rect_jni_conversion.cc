// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/android/rect_jni_conversion.h"

#include "base/android/jni_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/android/ui_jar_jni/RectF_jni.h"
#include "ui/android/ui_jar_jni/Rect_jni.h"

namespace jni_zero {

template <>
gfx::Rect FromJniType<gfx::Rect>(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_rect) {
  const auto& rect = j_rect.As<JRect>();
  return gfx::Rect(rect->Get_left(env), rect->Get_top(env), rect->width(env),
                   rect->height(env));
}

template <>
base::android::ScopedJavaLocalRef<jobject> ToJniType<gfx::Rect>(
    JNIEnv* env,
    const gfx::Rect& rect) {
  return JRectJni::New(env, rect.x(), rect.y(), rect.right(), rect.bottom());
}

template <>
gfx::RectF FromJniType<gfx::RectF>(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_rect) {
  const auto& rect = j_rect.As<JRectF>();
  return gfx::RectF(rect->Get_left(env), rect->Get_top(env), rect->width(env),
                    rect->height(env));
}

template <>
base::android::ScopedJavaLocalRef<jobject> ToJniType<gfx::RectF>(
    JNIEnv* env,
    const gfx::RectF& rect) {
  return JRectFJni::New(env, rect.x(), rect.y(), rect.right(), rect.bottom());
}

}  // namespace jni_zero
