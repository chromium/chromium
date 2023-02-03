// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/gesture_configuration.h"

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "ui/events/event_switches.h"

namespace ui {
namespace {

class GestureConfigurationIOS : public GestureConfiguration {
 public:
  GestureConfigurationIOS(const GestureConfigurationIOS&) = delete;
  GestureConfigurationIOS& operator=(const GestureConfigurationIOS&) = delete;

  ~GestureConfigurationIOS() override {}

  static GestureConfigurationIOS* GetInstance() {
    return base::Singleton<GestureConfigurationIOS>::get();
  }

 private:
  GestureConfigurationIOS() : GestureConfiguration() {
    set_double_tap_enabled(true);
    set_stylus_scale_enabled(false);
    set_gesture_begin_end_types_enabled(false);
    set_fling_touchscreen_tap_suppression_enabled(true);
    set_fling_touchpad_tap_suppression_enabled(false);

    set_short_press_time(base::Milliseconds(
        std::max(long_press_time_in_ms() - 100, long_press_time_in_ms() / 2)));
  }

  friend struct base::DefaultSingletonTraits<GestureConfigurationIOS>;
};

}  // namespace

// Create a GestureConfigurationIOS singleton instance when using iOS.
GestureConfiguration* GestureConfiguration::GetPlatformSpecificInstance() {
  return GestureConfigurationIOS::GetInstance();
}

}  // namespace ui
