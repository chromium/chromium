// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANDROID_RECT_JNI_CONVERSION_H_
#define UI_GFX_ANDROID_RECT_JNI_CONVERSION_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/component_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace jni_zero {

template <>
COMPONENT_EXPORT(GFX)
gfx::Rect FromJniType<gfx::Rect>(JNIEnv* env,
                                 const base::android::JavaRef<jobject>& j_rect);

template <>
COMPONENT_EXPORT(GFX)
base::android::ScopedJavaLocalRef<jobject> ToJniType<gfx::Rect>(
    JNIEnv* env,
    const gfx::Rect& rect);

template <>
COMPONENT_EXPORT(GFX)
gfx::RectF
    FromJniType<gfx::RectF>(JNIEnv* env,
                            const base::android::JavaRef<jobject>& j_rect);

template <>
COMPONENT_EXPORT(GFX)
base::android::ScopedJavaLocalRef<jobject> ToJniType<gfx::RectF>(
    JNIEnv* env,
    const gfx::RectF& rect);

}  // namespace jni_zero

#endif  // UI_GFX_ANDROID_RECT_JNI_CONVERSION_H_
