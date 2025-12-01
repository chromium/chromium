// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/android/view_configuration.h"

#include "base/android/jni_android.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/gfx/gfx_jni_headers/ViewConfigurationHelper_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;

namespace gfx {

namespace {

struct ViewConfigurationData {
  ViewConfigurationData() {
    JNIEnv* env = AttachCurrentThread();
    j_view_configuration_helper_.Reset(
        Java_ViewConfigurationHelper_createWithListener(env));

    double_tap_timeout_in_ms_ =
        Java_ViewConfigurationHelper_getDoubleTapTimeout(env);
    long_press_timeout_in_ms_ =
        Java_ViewConfigurationHelper_getLongPressTimeout(env);
    tap_timeout_in_ms_ = Java_ViewConfigurationHelper_getTapTimeout(env);

    Update(Java_ViewConfigurationHelper_getMaximumFlingVelocity(
               env, j_view_configuration_helper_),
           Java_ViewConfigurationHelper_getMinimumFlingVelocity(
               env, j_view_configuration_helper_),
           Java_ViewConfigurationHelper_getTouchSlop(
               env, j_view_configuration_helper_),
           Java_ViewConfigurationHelper_getDoubleTapSlop(
               env, j_view_configuration_helper_),
           Java_ViewConfigurationHelper_getMinScalingSpan(
               env, j_view_configuration_helper_),
           Java_ViewConfigurationHelper_getTextCursorBlinkInterval(
               env, j_view_configuration_helper_));
  }

  ViewConfigurationData(const ViewConfigurationData&) = delete;
  ViewConfigurationData& operator=(const ViewConfigurationData&) = delete;

  ~ViewConfigurationData() {}

  void SynchronizedUpdate(float maximum_fling_velocity,
                          float minimum_fling_velocity,
                          float touch_slop,
                          float double_tap_slop,
                          float min_scaling_span,
                          int text_cursor_blink_interval) {
    base::AutoLock autolock(lock_);
    Update(maximum_fling_velocity, minimum_fling_velocity, touch_slop,
           double_tap_slop, min_scaling_span, text_cursor_blink_interval);
  }

  int double_tap_timeout_in_ms() const { return double_tap_timeout_in_ms_; }
  int long_press_timeout_in_ms() const { return long_press_timeout_in_ms_; }
  int tap_timeout_in_ms() const { return tap_timeout_in_ms_; }

  int max_fling_velocity_in_dips_s() {
    base::AutoLock autolock(lock_);
    return max_fling_velocity_in_dips_s_;
  }

  int min_fling_velocity_in_dips_s() {
    base::AutoLock autolock(lock_);
    return min_fling_velocity_in_dips_s_;
  }

  int touch_slop_in_dips() {
    base::AutoLock autolock(lock_);
    return touch_slop_in_dips_;
  }

  int double_tap_slop_in_dips() {
    base::AutoLock autolock(lock_);
    return double_tap_slop_in_dips_;
  }

  int min_scaling_span_in_dips() {
    base::AutoLock autolock(lock_);
    return min_scaling_span_in_dips_;
  }

  base::TimeDelta text_cursor_blink_interval() {
    base::AutoLock autolock(lock_);
    return text_cursor_blink_interval_;
  }

 private:
  void Update(float maximum_fling_velocity,
              float minimum_fling_velocity,
              float touch_slop,
              float double_tap_slop,
              float min_scaling_span,
              int text_cursor_blink_interval) {
    DCHECK_LE(minimum_fling_velocity, maximum_fling_velocity);
    max_fling_velocity_in_dips_s_ = maximum_fling_velocity;
    min_fling_velocity_in_dips_s_ = minimum_fling_velocity;
    touch_slop_in_dips_ = touch_slop;
    double_tap_slop_in_dips_ = double_tap_slop;
    min_scaling_span_in_dips_ = min_scaling_span;
    text_cursor_blink_interval_ =
        base::Milliseconds(text_cursor_blink_interval);
  }

  base::Lock lock_;
  base::android::ScopedJavaGlobalRef<jobject> j_view_configuration_helper_;

  // These values will remain constant throughout the lifetime of the app, so
  // read-access needn't be synchronized.
  int double_tap_timeout_in_ms_ = 0;
  int long_press_timeout_in_ms_ = 0;
  int tap_timeout_in_ms_ = 0;

  // These values may vary as view-specific parameters change, so read/write
  // access must be synchronized.
  int max_fling_velocity_in_dips_s_ = 0;
  int min_fling_velocity_in_dips_s_ = 0;
  int touch_slop_in_dips_ = 0;
  int double_tap_slop_in_dips_ = 0;
  int min_scaling_span_in_dips_ = 0;
  base::TimeDelta text_cursor_blink_interval_ = base::Milliseconds(500);
};

ViewConfigurationData& GetViewConfigurationData() {
  static base::NoDestructor<ViewConfigurationData> view_configuration;
  return *view_configuration;
}

}  // namespace

static void JNI_ViewConfigurationHelper_UpdateSharedViewConfiguration(
    JNIEnv* env,
    jfloat maximum_fling_velocity,
    jfloat minimum_fling_velocity,
    jfloat touch_slop,
    jfloat double_tap_slop,
    jfloat min_scaling_span,
    jint text_cursor_blink_interval) {
  GetViewConfigurationData().SynchronizedUpdate(
      maximum_fling_velocity, minimum_fling_velocity, touch_slop,
      double_tap_slop, min_scaling_span, text_cursor_blink_interval);
}

int ViewConfiguration::GetDoubleTapTimeoutInMs() {
  return GetViewConfigurationData().double_tap_timeout_in_ms();
}

int ViewConfiguration::GetLongPressTimeoutInMs() {
  return GetViewConfigurationData().long_press_timeout_in_ms();
}

int ViewConfiguration::GetTapTimeoutInMs() {
  return GetViewConfigurationData().tap_timeout_in_ms();
}

int ViewConfiguration::GetMaximumFlingVelocityInDipsPerSecond() {
  return GetViewConfigurationData().max_fling_velocity_in_dips_s();
}

int ViewConfiguration::GetMinimumFlingVelocityInDipsPerSecond() {
  return GetViewConfigurationData().min_fling_velocity_in_dips_s();
}

int ViewConfiguration::GetTouchSlopInDips() {
  return GetViewConfigurationData().touch_slop_in_dips();
}

int ViewConfiguration::GetDoubleTapSlopInDips() {
  return GetViewConfigurationData().double_tap_slop_in_dips();
}

int ViewConfiguration::GetMinScalingSpanInDips() {
  return GetViewConfigurationData().min_scaling_span_in_dips();
}

base::TimeDelta ViewConfiguration::GetTextCursorBlinkInterval() {
  return GetViewConfigurationData().text_cursor_blink_interval();
}

}  // namespace gfx

DEFINE_JNI(ViewConfigurationHelper)
