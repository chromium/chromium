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
  return gfx::Rect(JNI_Rect::Java_Rect_GetField_left(env, j_rect),
                   JNI_Rect::Java_Rect_GetField_top(env, j_rect),
                   JNI_Rect::Java_Rect_width(env, j_rect),
                   JNI_Rect::Java_Rect_height(env, j_rect));
}

template <>
base::android::ScopedJavaLocalRef<jobject> ToJniType<gfx::Rect>(
    JNIEnv* env,
    const gfx::Rect& rect) {
  return JNI_Rect::Java_Rect_Constructor(env, rect.x(), rect.y(), rect.right(),
                                         rect.bottom());
}

template <>
gfx::RectF FromJniType<gfx::RectF>(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_rect) {
  return gfx::RectF(JNI_RectF::Java_RectF_GetField_left(env, j_rect),
                    JNI_RectF::Java_RectF_GetField_top(env, j_rect),
                    JNI_RectF::Java_RectF_width(env, j_rect),
                    JNI_RectF::Java_RectF_height(env, j_rect));
}

template <>
base::android::ScopedJavaLocalRef<jobject> ToJniType<gfx::RectF>(
    JNIEnv* env,
    const gfx::RectF& rect) {
  return JNI_RectF::Java_RectF_Constructor(env, rect.x(), rect.y(),
                                           rect.right(), rect.bottom());
}

}  // namespace jni_zero
