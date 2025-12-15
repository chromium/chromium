// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANDROID_RECT_JNI_CONVERSION_H_
#define UI_GFX_ANDROID_RECT_JNI_CONVERSION_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/gfx/gfx_jni_headers/RectJniConversion_jni.h"

namespace jni_zero {

template <>
inline gfx::Rect FromJniType<gfx::Rect>(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_rect) {
  DCHECK(env);
  return gfx::Rect(gfx::Java_RectJniConversion_getX(env, j_rect),
                   gfx::Java_RectJniConversion_getY(env, j_rect),
                   gfx::Java_RectJniConversion_getWidth(env, j_rect),
                   gfx::Java_RectJniConversion_getHeight(env, j_rect));
}

template <>
inline base::android::ScopedJavaLocalRef<jobject> ToJniType<gfx::Rect>(
    JNIEnv* env,
    const gfx::Rect& rect) {
  DCHECK(env);
  return gfx::Java_RectJniConversion_createRect(env, rect.x(), rect.y(),
                                                rect.right(), rect.bottom());
}

template <>
inline gfx::RectF FromJniType<gfx::RectF>(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_rect) {
  DCHECK(env);
  return gfx::RectF(gfx::Java_RectJniConversion_getXF(env, j_rect),
                    gfx::Java_RectJniConversion_getYF(env, j_rect),
                    gfx::Java_RectJniConversion_getWidthF(env, j_rect),
                    gfx::Java_RectJniConversion_getHeightF(env, j_rect));
}

template <>
inline base::android::ScopedJavaLocalRef<jobject> ToJniType<gfx::RectF>(
    JNIEnv* env,
    const gfx::RectF& rect) {
  DCHECK(env);
  return gfx::Java_RectJniConversion_createRectF(env, rect.x(), rect.y(),
                                                 rect.right(), rect.bottom());
}

}  // namespace jni_zero

#endif  // UI_GFX_ANDROID_RECT_JNI_CONVERSION_H_
