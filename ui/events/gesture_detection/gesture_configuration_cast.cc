// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/gesture_configuration.h"

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "ui/events/event_switches.h"

namespace ui {
namespace {

class GestureConfigurationCast : public GestureConfiguration {
 public:
  GestureConfigurationCast(const GestureConfigurationCast&) = delete;
  GestureConfigurationCast& operator=(const GestureConfigurationCast&) = delete;

  ~GestureConfigurationCast() override {
  }

  static GestureConfigurationCast* GetInstance() {
    return base::Singleton<GestureConfigurationCast>::get();
  }

 private:
  GestureConfigurationCast() : GestureConfiguration() {
    set_double_tap_enabled(false);
    set_double_tap_timeout_in_ms(double_tap_timeout_in_ms());
    set_gesture_begin_end_types_enabled(true);
    set_min_gesture_bounds_length(default_radius());
    set_min_pinch_update_span_delta(
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kCompensateForUnstablePinchZoom)
            ? 5
            : 0);
    set_velocity_tracker_strategy(VelocityTracker::Strategy::LSQ2_RESTRICTED);
    set_span_slop(max_touch_move_in_pixels_for_click() * 2);
    set_swipe_enabled(true);
    set_two_finger_tap_enabled(true);
    set_fling_touchpad_tap_suppression_enabled(true);
    set_fling_touchscreen_tap_suppression_enabled(true);
    set_max_fling_velocity(5000.0f);
  }

  friend struct base::DefaultSingletonTraits<GestureConfigurationCast>;
};

}  // namespace

// Create a GestureConfigurationCast singleton instance when using Chromecast.
GestureConfiguration* GestureConfiguration::GetPlatformSpecificInstance() {
  return GestureConfigurationCast::GetInstance();
}

}  // namespace ui
