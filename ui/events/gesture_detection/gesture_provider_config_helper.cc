// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/gesture_provider_config_helper.h"

#include "base/task/sequenced_task_runner.h"
#include "ui/display/screen.h"
#include "ui/events/gesture_detection/gesture_configuration.h"

namespace ui {
namespace {

class GenericDesktopGestureConfiguration : public GestureConfiguration {
 public:
  // The default GestureConfiguration parameters are already tailored for a
  // desktop environment (Aura).
  GenericDesktopGestureConfiguration() {}
  ~GenericDesktopGestureConfiguration() override {}
};

GestureDetector::Config BuildGestureDetectorConfig(
    const GestureConfiguration& gesture_config,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  GestureDetector::Config config;
  config.longpress_timeout =
      base::Milliseconds(gesture_config.long_press_time_in_ms());
  config.shortpress_timeout = gesture_config.short_press_time();
  config.showpress_timeout =
      base::Milliseconds(gesture_config.show_press_delay_in_ms());
  config.double_tap_timeout =
      base::Milliseconds(gesture_config.double_tap_timeout_in_ms());
  config.stylus_slop = gesture_config.max_stylus_move_in_pixels_for_click();
  config.touch_slop = gesture_config.max_touch_move_in_pixels_for_click();
  config.double_tap_slop =
      gesture_config.max_distance_between_taps_for_double_tap();
  config.minimum_fling_velocity = gesture_config.min_fling_velocity();
  config.maximum_fling_velocity = gesture_config.max_fling_velocity();
  config.swipe_enabled = gesture_config.swipe_enabled();
  config.minimum_swipe_velocity = gesture_config.min_swipe_velocity();
  config.maximum_swipe_deviation_angle =
      gesture_config.max_swipe_deviation_angle();
  config.two_finger_tap_enabled = gesture_config.two_finger_tap_enabled();
  config.two_finger_tap_max_separation =
      gesture_config.max_distance_for_two_finger_tap_in_pixels();
  config.two_finger_tap_timeout = base::Milliseconds(
      gesture_config.max_touch_down_duration_for_click_in_ms());
  config.single_tap_repeat_interval = gesture_config.max_tap_count();
  config.velocity_tracker_strategy = gesture_config.velocity_tracker_strategy();
  config.task_runner = task_runner;
  return config;
}

ScaleGestureDetector::Config BuildScaleGestureDetectorConfig(
    const GestureConfiguration& gesture_config) {
  ScaleGestureDetector::Config config;
  config.span_slop = gesture_config.span_slop();
  config.min_scaling_span = gesture_config.min_scaling_span_in_pixels();
  config.min_pinch_update_span_delta =
      gesture_config.min_pinch_update_span_delta();
  config.stylus_scale_enabled = gesture_config.stylus_scale_enabled();
  return config;
}

GestureProvider::Config BuildGestureProviderConfig(
    const GestureConfiguration& gesture_config,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  GestureProvider::Config config;
  config.gesture_detector_config =
      BuildGestureDetectorConfig(gesture_config, task_runner);
  config.scale_gesture_detector_config =
      BuildScaleGestureDetectorConfig(gesture_config);
  config.double_tap_support_for_platform_enabled =
      gesture_config.double_tap_enabled();
  config.gesture_begin_end_types_enabled =
      gesture_config.gesture_begin_end_types_enabled();
  config.min_gesture_bounds_length = gesture_config.min_gesture_bounds_length();
  config.max_gesture_bounds_length = gesture_config.max_gesture_bounds_length();
  return config;
}

}  // namespace

GestureProvider::Config GetGestureProviderConfig(
    GestureProviderConfigType type,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  GestureProvider::Config config;
  switch (type) {
    case GestureProviderConfigType::CURRENT_PLATFORM:
      config = BuildGestureProviderConfig(*GestureConfiguration::GetInstance(),
                                          task_runner);
      break;
    case GestureProviderConfigType::GENERIC_DESKTOP:
      config = BuildGestureProviderConfig(GenericDesktopGestureConfiguration(),
                                          task_runner);
      break;
    case GestureProviderConfigType::GENERIC_MOBILE:
      // The default GestureProvider::Config embeds a mobile configuration.
      break;
  }

  display::Screen* screen = display::Screen::GetScreen();
  // |screen| is sometimes NULL during tests.
  if (screen)
    config.display = screen->GetPrimaryDisplay();

  return config;
}

}  // namespace ui
