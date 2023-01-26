// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/gesture_configuration.h"

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "build/chromeos_buildflags.h"
#include "ui/events/event_switches.h"

namespace ui {
namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr bool kDoubleTapAuraSupport = true;
#else
constexpr bool kDoubleTapAuraSupport = false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class GestureConfigurationAura : public GestureConfiguration {
 public:
  GestureConfigurationAura(const GestureConfigurationAura&) = delete;
  GestureConfigurationAura& operator=(const GestureConfigurationAura&) = delete;

  ~GestureConfigurationAura() override {
  }

  static GestureConfigurationAura* GetInstance() {
    return base::Singleton<GestureConfigurationAura>::get();
  }

 private:
  GestureConfigurationAura() : GestureConfiguration() {
#if BUILDFLAG(IS_CHROMEOS)
    // On ChromeOS, the touch slop value of 6 is derived from the android's
    // default(8), multiplied by base dpi ratio(0.75).  See crbug.com/1083120
    // for more details.
    set_max_touch_move_in_pixels_for_click(6);
    // The default stylus slop value of 10 is derived from the UMA data analysis
    // for making stylus clicks easier for users.  See crbug.com/1181872.
    set_max_stylus_move_in_pixels_for_click(10);
#endif
    double touch_slop_distance;
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kTouchSlopDistance) &&
        base::StringToDouble(
            command_line->GetSwitchValueASCII(switches::kTouchSlopDistance),
            &touch_slop_distance)) {
      set_max_touch_move_in_pixels_for_click(touch_slop_distance);
    }

    set_double_tap_enabled(kDoubleTapAuraSupport);
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
  }

  friend struct base::DefaultSingletonTraits<GestureConfigurationAura>;
};

}  // namespace

// Create a GestureConfigurationAura singleton instance when using aura.
GestureConfiguration* GestureConfiguration::GetPlatformSpecificInstance() {
  return GestureConfigurationAura::GetInstance();
}

}  // namespace ui
