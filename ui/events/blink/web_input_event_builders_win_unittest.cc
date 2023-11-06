// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/web_input_event_builders_win.h"
#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/events/event_constants.h"

using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;

namespace ui {

// This test validates that Pixel to DIP conversion occurs as needed in the
// WebMouseEventBuilder::Build function.
TEST(WebInputEventBuilderTest, TestMouseEventScale) {
  display::Display::ResetForceDeviceScaleFactorForTesting();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "2");

  // Synthesize a mouse move with x = 300 and y = 200.
  WebMouseEvent mouse_move = ui::WebMouseEventBuilder::Build(
      ::GetDesktopWindow(), WM_MOUSEMOVE, 0, MAKELPARAM(300, 200),
      base::TimeTicks() + base::Seconds(100),
      blink::WebPointerProperties::PointerType::kMouse);

  // The WebMouseEvent.position field should be in pixels on return and hence
  // should be the same value as the x and y coordinates passed in to the
  // WebMouseEventBuilder::Build function.
  EXPECT_EQ(300, mouse_move.PositionInWidget().x());
  EXPECT_EQ(200, mouse_move.PositionInWidget().y());

  // WebMouseEvent.positionInScreen is calculated in DIPs.
  EXPECT_EQ(150, mouse_move.PositionInScreen().x());
  EXPECT_EQ(100, mouse_move.PositionInScreen().y());

  EXPECT_EQ(blink::WebPointerProperties::PointerType::kMouse,
            mouse_move.pointer_type);

  command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "1");
  display::Display::ResetForceDeviceScaleFactorForTesting();
}

TEST(WebInputEventBuilderTest, TestPercentMouseWheelScroll) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kWindowsScrollingPersonality);

  // We must discount the system scroll settings from the test, as we don't them
  // failing if the test machine has different settings.
  unsigned long system_scroll_lines = 3;
  unsigned long system_scroll_chars = 1;
  SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &system_scroll_lines, 0);
  SystemParametersInfo(SPI_GETWHEELSCROLLCHARS, 0, &system_scroll_chars, 0);

  WebMouseWheelEvent mouse_wheel = WebMouseWheelEventBuilder::Build(
      ::GetDesktopWindow(), WM_MOUSEWHEEL, MAKEWPARAM(0, -WHEEL_DELTA),
      MAKELPARAM(0, 0), base::TimeTicks() + base::Seconds(100),
      blink::WebPointerProperties::PointerType::kMouse);
  EXPECT_EQ(ui::ScrollGranularity::kScrollByPercentage,
            mouse_wheel.delta_units);
  EXPECT_FLOAT_EQ(0.f, mouse_wheel.delta_x);
  EXPECT_FLOAT_EQ(
      -0.05f, mouse_wheel.delta_y / static_cast<float>(system_scroll_lines));
  EXPECT_FLOAT_EQ(0.f, mouse_wheel.wheel_ticks_x);
  EXPECT_FLOAT_EQ(-1.f, mouse_wheel.wheel_ticks_y);

  // For a horizontal scroll, Windows is <- -/+ ->, WebKit <- +/- ->.
  mouse_wheel = WebMouseWheelEventBuilder::Build(
      ::GetDesktopWindow(), WM_MOUSEHWHEEL, MAKEWPARAM(0, -WHEEL_DELTA),
      MAKELPARAM(0, 0), base::TimeTicks() + base::Seconds(100),
      blink::WebPointerProperties::PointerType::kMouse);
  EXPECT_EQ(ui::ScrollGranularity::kScrollByPercentage,
            mouse_wheel.delta_units);
  EXPECT_FLOAT_EQ(
      0.05f, mouse_wheel.delta_x / static_cast<float>(system_scroll_chars));
  EXPECT_FLOAT_EQ(0.f, mouse_wheel.delta_y);
  EXPECT_FLOAT_EQ(1.f, mouse_wheel.wheel_ticks_x);
  EXPECT_FLOAT_EQ(0.f, mouse_wheel.wheel_ticks_y);
}

void VerifyWebMouseWheelEventBuilderHistograms(
    UINT message,
    blink::WebPointerProperties::PointerType type,
    const char* histogram,
    std::vector<int>& event_timestamps_in_ms,
    std::map<int, int>& histogram_expectations) {
  base::HistogramTester histogram_tester;

  EXPECT_TRUE(event_timestamps_in_ms.size() > 0 &&
              histogram_expectations.size() > 0);
  for (int event_timestamp : event_timestamps_in_ms) {
    WebMouseWheelEventBuilder::Build(
        ::GetDesktopWindow(), message, MAKEWPARAM(0, -WHEEL_DELTA),
        MAKELPARAM(0, 0),
        base::TimeTicks() + base::Milliseconds(event_timestamp), type);
  }

  for (std::map<int, int>::iterator it = histogram_expectations.begin();
       it != histogram_expectations.end(); ++it) {
    // Key is the (unique) velocity bucket.
    // Value is the count of data points for that bucket.
    EXPECT_EQ(histogram_tester.GetBucketCount(histogram, it->first),
              it->second);
  }
}

}  // namespace ui
