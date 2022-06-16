// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/android/drag_event_android.h"

#include <memory>

#include "base/android/jni_android.h"

using base::android::ScopedJavaLocalRef;
using base::android::AttachCurrentThread;

namespace ui {

DragEventAndroid::DragEventAndroid(
    JNIEnv* env,
    int action,
    const gfx::PointF& location,
    const gfx::PointF& screen_location,
    const std::vector<std::u16string>& mime_types,
    DropDataAndroid drop_data_android)
    : action_(action),
      location_(location),
      screen_location_(screen_location),
      mime_types_(mime_types),
      drop_data_android_(drop_data_android) {}

DragEventAndroid::~DragEventAndroid() {}

std::unique_ptr<DragEventAndroid> DragEventAndroid::CreateFor(
    const gfx::PointF& new_location) const {
  gfx::PointF new_screen_location =
      new_location + (screen_location() - location());
  JNIEnv* env = AttachCurrentThread();
  return std::make_unique<DragEventAndroid>(env, action_, new_location,
                                            new_screen_location, mime_types_,
                                            drop_data_android_);
}

}  // namespace ui
