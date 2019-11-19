// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/android/view_configuration.h"

#include "base/android/jni_android.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "ui/gfx/gfx_jni_headers/ViewConfigurationHelper_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;

namespace gfx {

namespace {

struct ViewConfigurationData {
  ViewConfigurationData()
      : double_tap_timeout_in_ms_(0),
        long_press_timeout_in_ms_(0),
        tap_timeout_in_ms_(0),
        max_fling_velocity_in_dips_s_(0),
        min_fling_velocity_in_dips_s_(0),
        touch_slop_in_dips_(0),
        double_tap_slop_in_dips_(0),
        min_scaling_span_in_dips_(0) {
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
               env, j_view_configuration_helper_));
  }

  ~ViewConfigurationData() {}

  void SynchronizedUpdate(float maximum_fling_velocity,
                          float minimum_fling_velocity,
                          float touch_slop,
                          float double_tap_slop,
                          float min_scaling_span) {
    base::AutoLock autolock(lock_);
    Update(maximum_fling_velocity, minimum_fling_velocity, touch_slop,
           double_tap_slop, min_scaling_span);
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

 private:
  void Update(float maximum_fling_velocity,
              float minimum_fling_velocity,
              float touch_slop,
              float double_tap_slop,
              float min_scaling_span) {
    DCHECK_LE(minimum_fling_velocity, maximum_fling_velocity);
    max_fling_velocity_in_dips_s_ = maximum_fling_velocity;
    min_fling_velocity_in_dips_s_ = minimum_fling_velocity;
    touch_slop_in_dips_ = touch_slop;
    double_tap_slop_in_dips_ = double_tap_slop;
    min_scaling_span_in_dips_ = min_scaling_span;
  }

  base::Lock lock_;
  base::android::ScopedJavaGlobalRef<jobject> j_view_configuration_helper_;

  // These values will remain constant throughout the lifetime of the app, so
  // read-access needn't be synchronized.
  int double_tap_timeout_in_ms_;
  int long_press_timeout_in_ms_;
  int tap_timeout_in_ms_;

  // These values may vary as view-specific parameters change, so read/write
  // access must be synchronized.
  int max_fling_velocity_in_dips_s_;
  int min_fling_velocity_in_dips_s_;
  int touch_slop_in_dips_;
  int double_tap_slop_in_dips_;
  int min_scaling_span_in_dips_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ViewConfigurationData);
};

// Leaky to allow access from any thread.
base::LazyInstance<ViewConfigurationData>::Leaky g_view_configuration =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

static void JNI_ViewConfigurationHelper_UpdateSharedViewConfiguration(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jfloat maximum_fling_velocity,
    jfloat minimum_fling_velocity,
    jfloat touch_slop,
    jfloat double_tap_slop,
    jfloat min_scaling_span) {
  g_view_configuration.Get().SynchronizedUpdate(
      maximum_fling_velocity, minimum_fling_velocity, touch_slop,
      double_tap_slop, min_scaling_span);
}

int ViewConfiguration::GetDoubleTapTimeoutInMs() {
  return g_view_configuration.Get().double_tap_timeout_in_ms();
}

int ViewConfiguration::GetLongPressTimeoutInMs() {
  return g_view_configuration.Get().long_press_timeout_in_ms();
}

int ViewConfiguration::GetTapTimeoutInMs() {
  return g_view_configuration.Get().tap_timeout_in_ms();
}

int ViewConfiguration::GetMaximumFlingVelocityInDipsPerSecond() {
  return g_view_configuration.Get().max_fling_velocity_in_dips_s();
}

int ViewConfiguration::GetMinimumFlingVelocityInDipsPerSecond() {
  return g_view_configuration.Get().min_fling_velocity_in_dips_s();
}

int ViewConfiguration::GetTouchSlopInDips() {
  return g_view_configuration.Get().touch_slop_in_dips();
}

int ViewConfiguration::GetDoubleTapSlopInDips() {
  return g_view_configuration.Get().double_tap_slop_in_dips();
}

int ViewConfiguration::GetMinScalingSpanInDips() {
  return g_view_configuration.Get().min_scaling_span_in_dips();
}

}  // namespace gfx
