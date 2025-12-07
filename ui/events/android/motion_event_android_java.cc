// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/android/motion_event_android_java.h"

#include <android/input.h>

#include <cmath>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/numerics/angle_conversions.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/android/motion_event_android_source.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/events/motionevent_jni_headers/MotionEvent_jni.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace ui {

MotionEventAndroidJava::MotionEventAndroidJava(
    jfloat pix_to_dip,
    jfloat ticks_x,
    jfloat ticks_y,
    jfloat tick_multiplier,
    base::TimeTicks oldest_event_time,
    base::TimeTicks latest_event_time,
    base::TimeTicks down_time_ms,
    jint android_action,
    jint pointer_count,
    jint history_size,
    jint action_index,
    jint android_action_button,
    jint android_gesture_classification,
    jint android_button_state,
    jint meta_state,
    jfloat raw_offset_x_pixels,
    jfloat raw_offset_y_pixels,
    jboolean for_touch_handle,
    const Pointer* const pointer0,
    const Pointer* const pointer1,
    std::unique_ptr<MotionEventAndroidSource> source)
    : MotionEventAndroid(pix_to_dip,
                         ticks_x,
                         ticks_y,
                         tick_multiplier,
                         oldest_event_time,
                         latest_event_time,
                         down_time_ms,
                         android_action,
                         pointer_count,
                         history_size,
                         action_index,
                         android_action_button,
                         android_gesture_classification,
                         android_button_state,
                         meta_state,
                         raw_offset_x_pixels,
                         raw_offset_y_pixels,
                         for_touch_handle,
                         pointer0,
                         pointer1,
                         std::move(source)) {}

MotionEventAndroidJava::MotionEventAndroidJava(const MotionEventAndroidJava& e,
                                               const gfx::PointF& point)
    : MotionEventAndroid(e, point) {}

std::unique_ptr<MotionEventAndroid> MotionEventAndroidJava::CreateFor(
    const gfx::PointF& point) const {
  return base::WrapUnique(new MotionEventAndroidJava(*this, point));
}

MotionEventAndroidJava::~MotionEventAndroidJava() = default;

ScopedJavaLocalRef<jobject> MotionEventAndroidJava::GetJavaObject() const {
  return source()->GetJavaObject();
}

bool MotionEventAndroidJava::IsLatestEventTimeResampled() const {
  return source()->IsLatestEventTimeResampled();
}

}  // namespace ui
