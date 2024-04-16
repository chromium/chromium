// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURE_DETECTION_GESTURE_CONFIGURATION_H_
#define UI_EVENTS_GESTURE_DETECTION_GESTURE_CONFIGURATION_H_

#include "ui/events/gesture_detection/gesture_detection_export.h"
#include "ui/events/velocity_tracker/velocity_tracker.h"

namespace ui {

class GESTURE_DETECTION_EXPORT GestureConfiguration {
 public:
  // Returns the singleton GestureConfiguration.
  static GestureConfiguration* GetInstance();

  GestureConfiguration(const GestureConfiguration&) = delete;
  GestureConfiguration& operator=(const GestureConfiguration&) = delete;

  // Ordered alphabetically ignoring underscores.
  float default_radius() const { return default_radius_; }
  void set_default_radius(float radius) {
    default_radius_ = radius;
    min_gesture_bounds_length_ = default_radius_;
  }
  bool double_tap_enabled() const { return double_tap_enabled_; }
  void set_double_tap_enabled(bool enabled) { double_tap_enabled_ = enabled; }
  int double_tap_timeout_in_ms() const { return double_tap_timeout_in_ms_; }
  void set_double_tap_timeout_in_ms(int val) {
    double_tap_timeout_in_ms_ = val;
  }
  bool fling_touchpad_tap_suppression_enabled() const {
    return fling_touchpad_tap_suppression_enabled_;
  }
  void set_fling_touchpad_tap_suppression_enabled(bool enabled) {
    fling_touchpad_tap_suppression_enabled_ = enabled;
  }
  bool fling_touchscreen_tap_suppression_enabled() const {
    return fling_touchscreen_tap_suppression_enabled_;
  }
  void set_fling_touchscreen_tap_suppression_enabled(bool enabled) {
    fling_touchscreen_tap_suppression_enabled_ = enabled;
  }
  int fling_max_cancel_to_down_time_in_ms() const {
    return fling_max_cancel_to_down_time_in_ms_;
  }
  void set_fling_max_cancel_to_down_time_in_ms(int val) {
    fling_max_cancel_to_down_time_in_ms_ = val;
  }
  int fling_max_tap_gap_time_in_ms() const {
    return fling_max_tap_gap_time_in_ms_;
  }
  void set_fling_max_tap_gap_time_in_ms(int val) {
    fling_max_tap_gap_time_in_ms_ = val;
  }
  bool stylus_scale_enabled() const { return stylus_scale_enabled_; }
  void set_stylus_scale_enabled(bool enabled) {
    stylus_scale_enabled_ = enabled;
  }
  bool gesture_begin_end_types_enabled() const {
    return gesture_begin_end_types_enabled_;
  }
  void set_gesture_begin_end_types_enabled(bool val) {
    gesture_begin_end_types_enabled_ = val;
  }
  base::TimeDelta short_press_time() const { return short_press_time_; }
  void set_short_press_time(base::TimeDelta val) { short_press_time_ = val; }
  int long_press_time_in_ms() const { return long_press_time_in_ms_; }
  void set_long_press_time_in_ms(int val) { long_press_time_in_ms_ = val; }
  float max_distance_between_taps_for_double_tap() const {
    return max_distance_between_taps_for_double_tap_;
  }
  void set_max_distance_between_taps_for_double_tap(float val) {
    max_distance_between_taps_for_double_tap_ = val;
  }
  float max_distance_for_two_finger_tap_in_pixels() const {
    return max_distance_for_two_finger_tap_in_pixels_;
  }
  void set_max_distance_for_two_finger_tap_in_pixels(float val) {
    max_distance_for_two_finger_tap_in_pixels_ = val;
  }
  int max_tap_count() const { return max_tap_count_; }
  void set_max_tap_count(int count) { max_tap_count_ = count; }
  float max_fling_velocity() const { return max_fling_velocity_; }
  void set_max_fling_velocity(float val) { max_fling_velocity_ = val; }
  float max_gesture_bounds_length() const {
    return max_gesture_bounds_length_;
  }
  void set_max_gesture_bounds_length(float val) {
    max_gesture_bounds_length_ = val;
  }
  float max_separation_for_gesture_touches_in_pixels() const {
    return max_separation_for_gesture_touches_in_pixels_;
  }
  void set_max_separation_for_gesture_touches_in_pixels(float val) {
    max_separation_for_gesture_touches_in_pixels_ = val;
  }
  float max_swipe_deviation_angle() const {
    return max_swipe_deviation_angle_;
  }
  void set_max_swipe_deviation_angle(float val) {
    max_swipe_deviation_angle_ = val;
  }
  int max_time_between_double_click_in_ms() const {
    return max_time_between_double_click_in_ms_;
  }
  void set_max_time_between_double_click_in_ms(int val) {
    max_time_between_double_click_in_ms_ = val;
  }
  int max_touch_down_duration_for_click_in_ms() const {
    return max_touch_down_duration_for_click_in_ms_;
  }
  void set_max_touch_down_duration_for_click_in_ms(int val) {
    max_touch_down_duration_for_click_in_ms_ = val;
  }
  float max_stylus_move_in_pixels_for_click() const {
    return max_stylus_move_in_pixels_for_click_;
  }
  void set_max_stylus_move_in_pixels_for_click(float val) {
    max_stylus_move_in_pixels_for_click_ = val;
  }
  float max_touch_move_in_pixels_for_click() const {
    return max_touch_move_in_pixels_for_click_;
  }
  void set_max_touch_move_in_pixels_for_click(float val) {
    max_touch_move_in_pixels_for_click_ = val;
    span_slop_ = max_touch_move_in_pixels_for_click_ * 2;
  }
  float min_distance_for_pinch_scroll_in_pixels() const {
    return min_distance_for_pinch_scroll_in_pixels_;
  }
  void set_min_distance_for_pinch_scroll_in_pixels(float val) {
    min_distance_for_pinch_scroll_in_pixels_ = val;
  }
  float min_fling_velocity() const { return min_fling_velocity_; }
  void set_min_fling_velocity(float val) { min_fling_velocity_ = val; }
  float min_gesture_bounds_length() const {
    return min_gesture_bounds_length_;
  }
  float min_pinch_update_span_delta() const {
    return min_pinch_update_span_delta_;
  }
  void set_min_pinch_update_span_delta(float val) {
    min_pinch_update_span_delta_ = val;
  }
  float min_scaling_span_in_pixels() const {
    return min_scaling_span_in_pixels_;
  }
  void set_min_scaling_span_in_pixels(float val) {
    min_scaling_span_in_pixels_ = val;
  }
  float min_swipe_velocity() const { return min_swipe_velocity_; }
  void set_min_swipe_velocity(float val) { min_swipe_velocity_ = val; }
  int scroll_debounce_interval_in_ms() const {
    return scroll_debounce_interval_in_ms_;
  }
  int set_scroll_debounce_interval_in_ms(int val) {
    return scroll_debounce_interval_in_ms_ = val;
  }
  int show_press_delay_in_ms() const { return show_press_delay_in_ms_; }
  int set_show_press_delay_in_ms(int val) {
    return show_press_delay_in_ms_ = val;
  }
  bool single_pointer_cancel_enabled() const {
    return single_pointer_cancel_enabled_;
  }
  void set_single_pointer_cancel_enabled(bool enabled) {
    single_pointer_cancel_enabled_ = enabled;
  }

  float span_slop() const { return span_slop_; }
  bool swipe_enabled() const { return swipe_enabled_; }
  void set_swipe_enabled(bool val) { swipe_enabled_ = val; }
  bool two_finger_tap_enabled() const { return two_finger_tap_enabled_; }
  void set_two_finger_tap_enabled(bool val) { two_finger_tap_enabled_ = val; }
  VelocityTracker::Strategy velocity_tracker_strategy() const {
    return velocity_tracker_strategy_;
  }
  void set_velocity_tracker_strategy(VelocityTracker::Strategy val) {
    velocity_tracker_strategy_ = val;
  }

 protected:
  GestureConfiguration();
  virtual ~GestureConfiguration();

  void set_min_gesture_bounds_length(float val) {
    min_gesture_bounds_length_ = val;
  }
  void set_span_slop(float val) { span_slop_ = val; }

 private:
  // Returns the platform specific instance. This is invoked if a specific
  // instance has not been set.
  static GestureConfiguration* GetPlatformSpecificInstance();

  // These are listed in alphabetical order ignoring underscores.
  // NOTE: Adding new configuration parameters requires initializing
  // corresponding entries in aura_test_base.cc's SetUp().

  // The default touch radius length used when the only information given
  // by the device is the touch center.
  float default_radius_ = 25;

  bool double_tap_enabled_ = false;
  int double_tap_timeout_in_ms_ = 400;

  // Whether to suppress touchscreen/touchpad taps that occur during a fling (
  // in particular, when such taps cancel the active fling).
  bool fling_touchpad_tap_suppression_enabled_ = false;
  bool fling_touchscreen_tap_suppression_enabled_ = false;

  // Maximum time between a GestureFlingCancel and a mousedown such that the
  // mousedown is considered associated with the cancel event.
  int fling_max_cancel_to_down_time_in_ms_ = 400;

  // Maxium time between a mousedown/mouseup pair that is considered to be a
  // suppressable tap.
  int fling_max_tap_gap_time_in_ms_ = 200;

  bool stylus_scale_enabled_ = false;
  bool gesture_begin_end_types_enabled_ = false;

  base::TimeDelta short_press_time_ = base::Milliseconds(400);
  // TODO(crbug.com/40820441): All time fields here should be of type
  // |base::TimeDiff| instead of |int|.

  int long_press_time_in_ms_ = 500;
  float max_distance_between_taps_for_double_tap_ = 20;

  // The max length of a repeated tap sequence, e.g., to support double-click
  // only this is 2, to support triple-click it's 3.
  int max_tap_count_ = 3;

  // The maximum allowed distance between two fingers for a two finger tap. If
  // the distance between two fingers is greater than this value, we will not
  // recognize a two finger tap.
  float max_distance_for_two_finger_tap_in_pixels_ = 300;
  float max_fling_velocity_ = 17000;
  float max_gesture_bounds_length_ = 0;
  float max_separation_for_gesture_touches_in_pixels_ = 150;
  float max_swipe_deviation_angle_ = 20;
  int max_time_between_double_click_in_ms_ = 700;
  int max_touch_down_duration_for_click_in_ms_ = 800;
  float max_stylus_move_in_pixels_for_click_ = 20;
  float max_touch_move_in_pixels_for_click_ = 15;
  float min_distance_for_pinch_scroll_in_pixels_ = 20;
  float min_fling_velocity_ = 30;
  float min_gesture_bounds_length_ = 0;
  // Only used with --compensate-for-unstable-pinch-zoom.
  float min_pinch_update_span_delta_ = 0;
  // If this is too small, we currently can get single finger pinch zoom.  See
  // https://crbug.com/376618 for details.
  float min_scaling_span_in_pixels_ = 125;
  float min_swipe_velocity_ = 20;
  // TODO(crbug.com/41095532): Disable and remove entirely when issues
  // with intermittent scroll end detection on the Pixel are resolved.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  int scroll_debounce_interval_in_ms_ = 30;
#else
  int scroll_debounce_interval_in_ms_ = 0;
#endif
  int show_press_delay_in_ms_ = 150;

  // When enabled, a cancel action affects only the corresponding pointer (vs
  // all pointers active at that time).
#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool single_pointer_cancel_enabled_ = true;
#else
  bool single_pointer_cancel_enabled_ = false;
#endif

  // The default value of span_slop_ is 2 * max_touch_move_in_pixels_for_click_.
  float span_slop_ = 30;
  bool swipe_enabled_ = false;
  bool two_finger_tap_enabled_ = false;
  VelocityTracker::Strategy velocity_tracker_strategy_ =
      VelocityTracker::Strategy::STRATEGY_DEFAULT;
};

}  // namespace ui

#endif  // UI_EVENTS_GESTURE_DETECTION_GESTURE_CONFIGURATION_H_
