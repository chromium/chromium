// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/resources/nine_patch_resource.h"
#include "ui/gfx/geometry/rect.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/android/ui_android_jni_headers/ResourceFactory_jni.h"

using jni_zero::JavaRef;

namespace ui {

static int64_t JNI_ResourceFactory_CreateBitmapResource(JNIEnv* env) {
  return reinterpret_cast<intptr_t>(new Resource());
}

static int64_t JNI_ResourceFactory_CreateNinePatchBitmapResource(
    JNIEnv* env,
    int32_t padding_left,
    int32_t padding_top,
    int32_t padding_right,
    int32_t padding_bottom,
    int32_t aperture_left,
    int32_t aperture_top,
    int32_t aperture_right,
    int32_t aperture_bottom) {
  gfx::Rect padding(padding_left, padding_top, padding_right - padding_left,
                    padding_bottom - padding_top);
  gfx::Rect aperture(aperture_left, aperture_top,
                     aperture_right - aperture_left,
                     aperture_bottom - aperture_top);
  return reinterpret_cast<intptr_t>(new NinePatchResource(padding, aperture));
}

}  // namespace ui

DEFINE_JNI(ResourceFactory)
