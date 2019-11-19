// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "ui/events/gesture_detection/gesture_configuration.h"

namespace ui {
namespace {

GestureConfiguration* instance = nullptr;

}  // namespace

// static
void GestureConfiguration::SetInstance(GestureConfiguration* config) {
  instance = config;
}

// static
GestureConfiguration* GestureConfiguration::GetInstance() {
  if (instance)
    return instance;

  return GestureConfiguration::GetPlatformSpecificInstance();
}

GestureConfiguration::GestureConfiguration()
    : default_radius_(25),
      double_tap_enabled_(false),
      double_tap_timeout_in_ms_(400),
      fling_touchpad_tap_suppression_enabled_(false),
      fling_touchscreen_tap_suppression_enabled_(false),
      fling_max_cancel_to_down_time_in_ms_(400),
      fling_max_tap_gap_time_in_ms_(200),
      stylus_scale_enabled_(false),
      gesture_begin_end_types_enabled_(false),
      long_press_time_in_ms_(500),
      max_distance_between_taps_for_double_tap_(20),
      max_tap_count_(3),
      max_distance_for_two_finger_tap_in_pixels_(300),
      max_fling_velocity_(17000.0f),
      max_gesture_bounds_length_(0),
      max_separation_for_gesture_touches_in_pixels_(150),
      max_swipe_deviation_angle_(20),
      max_time_between_double_click_in_ms_(700),
      max_touch_down_duration_for_click_in_ms_(800),
      max_touch_move_in_pixels_for_click_(15),
      min_distance_for_pinch_scroll_in_pixels_(20),
      min_fling_velocity_(30.0f),
      min_gesture_bounds_length_(0),
      min_pinch_update_span_delta_(0),
      // If this is too small, we currently can get single finger pinch zoom.
      // See crbug.com/357237 for details.
      min_scaling_span_in_pixels_(125),
      min_swipe_velocity_(20),
      // TODO(jdduke): Disable and remove entirely when issues with intermittent
      // scroll end detection on the Pixel are resolved, crbug.com/353702.
#if defined(OS_CHROMEOS)
      scroll_debounce_interval_in_ms_(30),
#else
      scroll_debounce_interval_in_ms_(0),
#endif
      semi_long_press_time_in_ms_(400),
      show_press_delay_in_ms_(150),
#if defined(OS_CHROMEOS)
      single_pointer_cancel_enabled_(true),
#else
      single_pointer_cancel_enabled_(false),
#endif
      // The default value of span_slop_ is
      // 2 * max_touch_move_in_pixels_for_click_.
      span_slop_(30),
      swipe_enabled_(false),
      two_finger_tap_enabled_(false),
      velocity_tracker_strategy_(VelocityTracker::Strategy::STRATEGY_DEFAULT) {
}

GestureConfiguration::~GestureConfiguration() {
}

}  // namespace ui
