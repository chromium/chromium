// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURE_DETECTION_GESTURE_CONFIGURATION_H_
#define UI_EVENTS_GESTURE_DETECTION_GESTURE_CONFIGURATION_H_

#include "base/macros.h"
#include "ui/events/gesture_detection/gesture_detection_export.h"
#include "ui/events/gesture_detection/velocity_tracker.h"

namespace ui {

class GESTURE_DETECTION_EXPORT GestureConfiguration {
 public:
  // Sets the shared instance. This does not take ownership of |config|.
  static void SetInstance(GestureConfiguration* config);
  // Returns the singleton GestureConfiguration.
  static GestureConfiguration* GetInstance();

  // Ordered alphabetically ignoring underscores.
  float default_radius() const { return default_radius_; }
  void set_default_radius(float radius) {
    default_radius_ = radius;
    min_gesture_bounds_length_ = default_radius_;
  }
  bool double_tap_enabled() const { return double_tap_enabled_; }
  void set_double_tap_enabled(bool enabled) { double_tap_enabled_ = enabled; }
  int double_tap_timeout_in_ms() const { return double_tap_timeout_in_ms_; }
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
  int semi_long_press_time_in_ms() const {
    return semi_long_press_time_in_ms_;
  }
  void set_semi_long_press_time_in_ms(int val) {
    semi_long_press_time_in_ms_ = val;
    double_tap_timeout_in_ms_ = val;
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

  // The below configuration parameters are dependent on other parameters,
  // whose setter functions will setup these values as well, so we will not
  // provide public setter functions for them.
  void set_double_tap_timeout_in_ms(int val) {
    double_tap_timeout_in_ms_ = val;
  }
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
  float default_radius_;

  bool double_tap_enabled_;
  int double_tap_timeout_in_ms_;

  // Whether to suppress touchscreen/touchpad taps that occur during a fling (
  // in particular, when such taps cancel the active fling).
  bool fling_touchpad_tap_suppression_enabled_;
  bool fling_touchscreen_tap_suppression_enabled_;

  // Maximum time between a GestureFlingCancel and a mousedown such that the
  // mousedown is considered associated with the cancel event.
  int fling_max_cancel_to_down_time_in_ms_;

  // Maxium time between a mousedown/mouseup pair that is considered to be a
  // suppressable tap.
  int fling_max_tap_gap_time_in_ms_;

  bool stylus_scale_enabled_;
  bool gesture_begin_end_types_enabled_;
  int long_press_time_in_ms_;
  float max_distance_between_taps_for_double_tap_;

  // The max length of a repeated tap sequence, e.g., to support double-click
  // only this is 2, to support triple-click it's 3.
  int max_tap_count_;

  // The maximum allowed distance between two fingers for a two finger tap. If
  // the distance between two fingers is greater than this value, we will not
  // recognize a two finger tap.
  float max_distance_for_two_finger_tap_in_pixels_;
  float max_fling_velocity_;
  float max_gesture_bounds_length_;
  float max_separation_for_gesture_touches_in_pixels_;
  float max_swipe_deviation_angle_;
  int max_time_between_double_click_in_ms_;
  int max_touch_down_duration_for_click_in_ms_;
  float max_touch_move_in_pixels_for_click_;
  float min_distance_for_pinch_scroll_in_pixels_;
  float min_fling_velocity_;
  float min_gesture_bounds_length_;
  // Only used with --compensate-for-unstable-pinch-zoom.
  float min_pinch_update_span_delta_;
  float min_scaling_span_in_pixels_;
  float min_swipe_velocity_;
  int scroll_debounce_interval_in_ms_;
  int semi_long_press_time_in_ms_;
  int show_press_delay_in_ms_;
  // When enabled, a cancel action affects only the corresponding pointer (vs
  // all pointers active at that time).
  bool single_pointer_cancel_enabled_;
  float span_slop_;
  bool swipe_enabled_;
  bool two_finger_tap_enabled_;
  VelocityTracker::Strategy velocity_tracker_strategy_;

  DISALLOW_COPY_AND_ASSIGN(GestureConfiguration);
};

}  // namespace ui

#endif  // UI_EVENTS_GESTURE_DETECTION_GESTURE_CONFIGURATION_H_
