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

}  // namespace ui
