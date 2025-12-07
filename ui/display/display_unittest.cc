// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/display.h"

#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_switches.h"
#include "ui/display/types/display_color_management.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace display {

TEST(DisplayTest, WorkArea) {
  Display display(0, gfx::Rect(0, 0, 100, 100));
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), display.bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), display.work_area());

  display.set_work_area(gfx::Rect(3, 4, 90, 80));
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), display.bounds());
  EXPECT_EQ(gfx::Rect(3, 4, 90, 80), display.work_area());

  display.SetScaleAndBounds(1.0f, gfx::Rect(10, 20, 50, 50));
  EXPECT_EQ(gfx::Rect(10, 20, 50, 50), display.bounds());
  EXPECT_EQ(gfx::Rect(13, 24, 40, 30), display.work_area());

  display.SetSize(gfx::Size(200, 200));
  EXPECT_EQ(gfx::Rect(13, 24, 190, 180), display.work_area());

  display.UpdateWorkAreaFromInsets(gfx::Insets::TLBR(3, 4, 5, 6));
  EXPECT_EQ(gfx::Rect(14, 23, 190, 192), display.work_area());
}

TEST(DisplayTest, Scale) {
  Display display(0, gfx::Rect(0, 0, 100, 100));
  display.set_work_area(gfx::Rect(10, 10, 80, 80));
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), display.bounds());
  EXPECT_EQ(gfx::Rect(10, 10, 80, 80), display.work_area());

  // Scale it back to 2x
  display.SetScaleAndBounds(2.0f, gfx::Rect(0, 0, 140, 140));
  EXPECT_EQ(gfx::Rect(0, 0, 70, 70), display.bounds());
  EXPECT_EQ(gfx::Rect(10, 10, 50, 50), display.work_area());

  // Scale it back to 1x
  display.SetScaleAndBounds(1.0f, gfx::Rect(0, 0, 100, 100));
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), display.bounds());
  EXPECT_EQ(gfx::Rect(10, 10, 80, 80), display.work_area());
}

// https://crbug.com/517944
TEST(DisplayTest, ForcedDeviceScaleFactorByCommandLine) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();

  Display::ResetForceDeviceScaleFactorForTesting();

  command_line->AppendSwitch(switches::kForceDeviceScaleFactor);

  EXPECT_EQ(1, Display::GetForcedDeviceScaleFactor());
  Display::ResetForceDeviceScaleFactorForTesting();
}

TEST(DisplayTest, ForcedDeviceScaleFactor) {
  Display::SetForceDeviceScaleFactor(2);

  EXPECT_EQ(2, Display::GetForcedDeviceScaleFactor());
  Display::ResetForceDeviceScaleFactorForTesting();
}

TEST(DisplayTest, DisplayFrequency) {
  Display display(0, gfx::Rect(0, 0, 100, 100));

  display.set_display_frequency(60.0f);
  EXPECT_EQ(60.0f, display.display_frequency());

  display.set_display_frequency(120.0f);
  EXPECT_EQ(120.0f, display.display_frequency());
}

TEST(DisplayTest, DisplayLabel) {
  Display display(0, gfx::Rect(0, 0, 100, 100));

  display.set_label("Display 1");
  EXPECT_EQ("Display 1", display.label());

  display.set_label("Display 2");
  EXPECT_EQ("Display 2", display.label());
}

TEST(DisplayTest, GammaCurve) {
  std::vector<GammaRampRGBEntry> lut({
      {0, 1, 1},
      {32768, 2, 5},
      {65535, 3, 9},
  });
  GammaCurve curve(std::move(lut));
  uint16_t r, g, b;

  // Evaluate at the control points.
  curve.Evaluate(0 / 2.f, r, g, b);
  EXPECT_EQ(0, r);
  EXPECT_EQ(1, g);
  EXPECT_EQ(1, b);

  curve.Evaluate(1 / 2.f, r, g, b);
  EXPECT_EQ(32768, r);
  EXPECT_EQ(2, g);
  EXPECT_EQ(5, b);

  curve.Evaluate(2 / 2.f, r, g, b);
  EXPECT_EQ(65535, r);
  EXPECT_EQ(3, g);
  EXPECT_EQ(9, b);

  // Evaluate between points.
  curve.Evaluate(0.25f / 2.f, r, g, b);
  EXPECT_EQ(2, b);
  curve.Evaluate(0.50f / 2.f, r, g, b);
  EXPECT_EQ(3, b);
  curve.Evaluate(0.75f / 2.f, r, g, b);
  EXPECT_EQ(4, b);
  curve.Evaluate(1.00f / 2.f, r, g, b);
  EXPECT_EQ(5, b);
  curve.Evaluate(1.25f / 2.f, r, g, b);
  EXPECT_EQ(6, b);
  curve.Evaluate(1.50f / 2.f, r, g, b);
  EXPECT_EQ(7, b);
  curve.Evaluate(1.75f / 2.f, r, g, b);
  EXPECT_EQ(8, b);
}

TEST(DisplayTest, GammaCurveMakeConcat) {
  std::vector<GammaRampRGBEntry> lut_f;
  std::vector<GammaRampRGBEntry> lut_g;
  lut_f.resize(1024);
  for (size_t i = 0; i < lut_f.size(); ++i) {
    float x = i / (lut_f.size() - 1.f);
    float r = x * x;
    float g = x * x * x;
    float b = x * x * x * x;
    lut_f[i].r = static_cast<uint16_t>(std::round(65535.f * r));
    lut_f[i].g = static_cast<uint16_t>(std::round(65535.f * g));
    lut_f[i].b = static_cast<uint16_t>(std::round(65535.f * b));
  }

  lut_g.resize(512);
  for (size_t i = 0; i < lut_g.size(); ++i) {
    float x = i / (lut_g.size() - 1.f);
    float r = 0.5f * x;
    float g = 0.5f + 0.5f * x;
    float b = x;
    lut_g[i].r = static_cast<uint16_t>(std::round(65535.f * r));
    lut_g[i].g = static_cast<uint16_t>(std::round(65535.f * g));
    lut_g[i].b = static_cast<uint16_t>(std::round(65535.f * b));
  }

  GammaCurve curve_f(std::move(lut_f));
  GammaCurve curve_g(std::move(lut_g));
  GammaCurve curve = GammaCurve::MakeConcat(curve_f, curve_g);

  for (size_t i = 0; i < 256; ++i) {
    float x = i / 255.f;

    // Apply g.
    float r = 0.5f * x;
    float g = 0.5f + 0.5f * x;
    float b = x;

    // Apply f.
    r = r * r;
    g = g * g * g;
    b = b * b * b * b;

    // Compare.
    uint16_t actual_r;
    uint16_t actual_g;
    uint16_t actual_b;
    curve.Evaluate(x, actual_r, actual_g, actual_b);

    EXPECT_LT(std::abs(r - actual_r / 65535.f), 0.01);
    EXPECT_LT(std::abs(g - actual_g / 65535.f), 0.01);
    EXPECT_LT(std::abs(b - actual_b / 65535.f), 0.01);
  }
}

}  // namespace display
