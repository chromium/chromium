// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <tuple>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_conversions.h"

namespace gfx {

namespace {
// Helper struct for testing purposes.
struct ColorTest {
  std::tuple<float, float, float> input;
  std::tuple<float, float, float> expected;
};
}  // namespace

TEST(ColorConversions, LabToXYZD50) {
  // Color conversions obtained from
  // https://www.nixsensor.com/free-color-converter/
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},                // black
      {{100.0f, 0.0f, 0.0f}, {0.9642f, 1.0f, 0.8252f}},        // white
      {{33.0f, 0.0f, 0.0f}, {0.0727f, 0.0754f, 0.0622f}},      // gray1
      {{66.0f, 0.0f, 0.0f}, {0.3406f, 0.3532f, 0.2915f}},      // gray2
      {{20.0f, -35.0f, 45.0f}, {0.0134f, 0.0299f, -0.0056f}},  // dark_green
      {{80.0f, -60.0f, 70.0f}, {0.3416f, 0.5668f, 0.0899f}},   // ligth_green
      {{35.0f, 60.0f, 70.0f}, {0.1690f, 0.0850f, -0.0051f}},   // purple
      {{75.0f, 45.0f, -100.0f}, {0.6448f, 0.4828f, 1.7488f}},  // lile
      {{75.0f, 100.0f, 80.0f}, {0.92f, 0.4828f, 0.0469f}}};    // red

  for (auto& color_pair : colors_tests) {
    auto [input_l, input_a, input_b] = color_pair.input;
    auto [expected_x, expected_y, expected_z] = color_pair.expected;
    auto [output_x, output_y, output_z] =
        LabToXYZD50(input_l, input_a, input_b);
    EXPECT_NEAR(output_x, expected_x, 0.001f)
        << input_l << ' ' << input_a << ' ' << input_b << " to " << expected_x
        << ' ' << expected_y << ' ' << expected_z << " produced " << output_x
        << ' ' << output_y << ' ' << output_z;
    EXPECT_NEAR(output_y, expected_y, 0.001f)
        << input_l << ' ' << input_a << ' ' << input_b << " to " << expected_x
        << ' ' << expected_y << ' ' << expected_z << " produced " << output_x
        << ' ' << output_y << ' ' << output_z;
    EXPECT_NEAR(output_z, expected_z, 0.001f)
        << input_l << ' ' << input_a << ' ' << input_b << " to " << expected_x
        << ' ' << expected_y << ' ' << expected_z << " produced " << output_x
        << ' ' << output_y << ' ' << output_z;
  }
}

TEST(ColorConversions, XYZD50toD65) {
  // Color conversions obtained from
  // https://www.nixsensor.com/free-color-converter/
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},                      // black
      {{0.95047f, 1.0f, 1.0888f}, {0.95392f, 1.00594f, 1.439698f}},  // white
      {{0.412, 0.213f, 0.019f}, {0.389938f, 0.20384f, 0.025982f}},
      {{0.358f, 0.715f, 0.119f}, {0.33307f, 0.714494f, 0.1480589f}},
      {{0.18f, 0.072f, 0.95f}, {0.23041847f, 0.087602f, 1.264587f}},
      {{0.23f, 0.107f, 0.555f}, {0.252396f, 0.113222f, 0.73899f}},
      {{0.114f, 0.09f, 0.087f}, {0.112348f, 0.089496f, 0.115299f}}};

  for (auto& color_pair : colors_tests) {
    auto [input_x, input_y, input_z] = color_pair.input;
    auto [expected_x, expected_y, expected_z] = color_pair.expected;
    auto [output_x, output_y, output_z] =
        XYZD50toD65(input_x, input_y, input_z);
    EXPECT_NEAR(output_x, expected_x, 0.001f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_x
        << ' ' << expected_y << ' ' << expected_z << " produced " << output_x
        << ' ' << output_y << ' ' << output_z;
    EXPECT_NEAR(output_y, expected_y, 0.001f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_x
        << ' ' << expected_y << ' ' << expected_z << " produced " << output_x
        << ' ' << output_y << ' ' << output_z;
    EXPECT_NEAR(output_z, expected_z, 0.001f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_x
        << ' ' << expected_y << ' ' << expected_z << " produced " << output_x
        << ' ' << output_y << ' ' << output_z;
  }
}

TEST(ColorConversions, XYZD50tosRGBLinear) {
  // Color conversions obtained from
  // https://www.nixsensor.com/free-color-converter/
  std::tuple<float, float, float> colors_tests[] = {
      {0.0f, 0.0f, 0.0f},         // black
      {0.95047f, 1.0f, 1.0888f},  // white
      {0.412, 0.213f, 0.019f},   {0.358f, 0.715f, 0.119f},
      {0.18f, 0.072f, 0.95f},    {0.23f, 0.107f, 0.555f},
      {0.114f, 0.09f, 0.087f}};

  for (auto [input_x, input_y, input_z] : colors_tests) {
    auto [output_r, output_g, output_b] =
        XYZD50tosRGBLinear(input_x, input_y, input_z);
    auto [x, y, z] = XYZD50toD65(input_x, input_y, input_z);
    auto [expected_r, expected_g, expected_b] = XYZD65tosRGBLinear(x, y, z);
    EXPECT_NEAR(output_r, expected_r, 0.1f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << output_r
        << ' ' << output_g << ' ' << output_b;
    EXPECT_NEAR(output_g, expected_g, 0.1f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << output_r
        << ' ' << output_g << ' ' << output_b;
    EXPECT_NEAR(output_b, expected_b, 0.1f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << output_r
        << ' ' << output_g << ' ' << output_b;
  }
}

TEST(ColorConversions, LabTosRGB) {
  // Color conversions obtained from
  // https://www.nixsensor.com/free-color-converter/
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},              // black
      {{100.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},            // white
      {{46.2775f, -47.521f, 48.5837f}, {0.0f, 0.5f, 0.0f}},  // green
      {{50.0f, 50.0f, 0.0f}, {0.756208f, 0.304487f, 0.475634f}},
      {{70.0f, -45.0f, 0.0f}, {0.10751f, 0.75558f, 0.66398f}},
      {{70.0f, 0.0f, 70.0f}, {0.766254f, 0.663607f, 0.055775f}},
      {{55.0f, 0.0f, -60.0f}, {0.128128f, 0.53105f, 0.927645f}}};

  for (auto& color_pair : colors_tests) {
    auto [input_l, input_a, input_b] = color_pair.input;
    auto [expected_r, expected_g, expected_b] = color_pair.expected;
    SkColor4f color = LabToSkColor4f(input_l, input_a, input_b, 1.0f);
    EXPECT_NEAR(color.fR, expected_r, 0.01f)
        << input_l << ' ' << input_a << ' ' << input_b << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
    EXPECT_NEAR(color.fG, expected_g, 0.01f)
        << input_l << ' ' << input_a << ' ' << input_b << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
    EXPECT_NEAR(color.fB, expected_b, 0.01f)
        << input_l << ' ' << input_a << ' ' << input_b << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
  }
}

}  // namespace gfx
