// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/gesture_configuration.h"

#include "base/android/build_info.h"
#include "base/memory/singleton.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/screen.h"
#include "ui/gfx/android/view_configuration.h"

using gfx::ViewConfiguration;

namespace ui {
namespace {

// Touch radii on Android can be both noisy and inaccurate. The old Java
// gesture detection pipeline used a fixed value of 24 as the gesture bounds.
// We relax that value somewhat, but not by much; there's a fairly small window
// within which gesture bounds are useful for features like touch adjustment.
const float kMinGestureBoundsLengthDips = 20.f;
const float kMaxGestureBoundsLengthDips = 32.f;

class GestureConfigurationAndroid : public GestureConfiguration {
 public:
  GestureConfigurationAndroid(const GestureConfigurationAndroid&) = delete;
  GestureConfigurationAndroid& operator=(const GestureConfigurationAndroid&) =
      delete;

  ~GestureConfigurationAndroid() override {
  }

  static GestureConfigurationAndroid* GetInstance() {
    return base::Singleton<GestureConfigurationAndroid>::get();
  }

 private:
  GestureConfigurationAndroid() : GestureConfiguration() {
    set_double_tap_enabled(true);
    set_double_tap_timeout_in_ms(ViewConfiguration::GetDoubleTapTimeoutInMs());
    // TODO(jdduke): Enable this on Android M after the implicit conflict with
    // stylus selection is resolved.
    set_stylus_scale_enabled(false);
#if defined(USE_AURA)
    set_gesture_begin_end_types_enabled(true);
#else
    if (base::FeatureList::IsEnabled(features::kEnableGestureBeginEndTypes)) {
      set_gesture_begin_end_types_enabled(true);
    } else {
      set_gesture_begin_end_types_enabled(false);
    }
#endif
    set_long_press_time_in_ms(ViewConfiguration::GetLongPressTimeoutInMs());
    set_max_distance_between_taps_for_double_tap(
        ViewConfiguration::GetDoubleTapSlopInDips());
    set_max_fling_velocity(
        ViewConfiguration::GetMaximumFlingVelocityInDipsPerSecond());
    set_max_gesture_bounds_length(kMaxGestureBoundsLengthDips);
    set_max_touch_move_in_pixels_for_click(
        ViewConfiguration::GetTouchSlopInDips());
    set_max_stylus_move_in_pixels_for_click(
        ViewConfiguration::GetTouchSlopInDips() * 1.5f);
    set_min_fling_velocity(
        ViewConfiguration::GetMinimumFlingVelocityInDipsPerSecond());
    set_min_gesture_bounds_length(kMinGestureBoundsLengthDips);
    set_min_pinch_update_span_delta(0.f);
    set_min_scaling_span_in_pixels(
        ViewConfiguration::GetMinScalingSpanInDips());
    set_show_press_delay_in_ms(ViewConfiguration::GetTapTimeoutInMs());
    set_span_slop(ViewConfiguration::GetTouchSlopInDips() * 2.f);
    set_fling_touchscreen_tap_suppression_enabled(true);
    set_fling_touchpad_tap_suppression_enabled(false);
    set_fling_max_cancel_to_down_time_in_ms(
        ViewConfiguration::GetTapTimeoutInMs());
    set_fling_max_tap_gap_time_in_ms(
        ViewConfiguration::GetLongPressTimeoutInMs());

    // Android ViewConfiguration doesn't have a short-press timeout.  We are
    // using the heuristic that it would be 100ms less than the long-press
    // provided this is not too close with show-press.
    //
    // TODO(crbug.com/40820457): Replace this with platform-defined
    // timeout when available.
    set_short_press_time(base::Milliseconds(
        std::max(long_press_time_in_ms() - 100, long_press_time_in_ms() / 2)));
  }

  friend struct base::DefaultSingletonTraits<GestureConfigurationAndroid>;
};

}  // namespace

// Create a GestureConfigurationAura singleton instance when using Android.
GestureConfiguration* GestureConfiguration::GetPlatformSpecificInstance() {
  return GestureConfigurationAndroid::GetInstance();
}

}  // namespace ui
