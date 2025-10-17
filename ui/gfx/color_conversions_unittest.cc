// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_conversions.h"

#include <stdlib.h>

#include <optional>
#include <tuple>

#include "testing/gtest/include/gtest/gtest.h"

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

TEST(ColorConversions, XYZD50ToLab) {
  // Color conversions obtained from
  // https://www.nixsensor.com/free-color-converter/
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},                // black
      {{0.9642f, 1.0f, 0.8252f}, {100.0f, 0.0f, 0.0f}},        // white
      {{0.0727f, 0.0754f, 0.0622f}, {33.0f, 0.0f, 0.0f}},      // gray1
      {{0.3406f, 0.3532f, 0.2915f}, {66.0f, 0.0f, 0.0f}},      // gray2
      {{0.0134f, 0.0299f, -0.0056f}, {20.0f, -35.0f, 45.0f}},  // dark_green
      {{0.3416f, 0.5668f, 0.0899f}, {80.0f, -60.0f, 70.0f}},   // ligth_green
      {{0.1690f, 0.0850f, -0.0051f}, {35.0f, 60.0f, 70.0f}},   // purple
      {{0.6448f, 0.4828f, 1.7488f}, {75.0f, 45.0f, -100.0f}},  // lile
      {{0.92f, 0.4828f, 0.0469f}, {75.0f, 100.0f, 80.0f}}};    // red

  for (auto& color_pair : colors_tests) {
    auto [input_x, input_y, input_z] = color_pair.input;
    auto [expected_l, expected_a, expected_b] = color_pair.expected;
    auto [output_l, output_a, output_b] =
        XYZD50ToLab(input_x, input_y, input_z);
    EXPECT_NEAR(output_l, expected_l, 0.1f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_l
        << ' ' << expected_a << ' ' << expected_b << " produced " << output_l
        << ' ' << output_a << ' ' << output_b;
    EXPECT_NEAR(output_a, expected_a, 0.1f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_l
        << ' ' << expected_a << ' ' << expected_b << " produced " << output_l
        << ' ' << output_a << ' ' << output_b;
    EXPECT_NEAR(output_b, expected_b, 0.1f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_l
        << ' ' << expected_a << ' ' << expected_b << " produced " << output_l
        << ' ' << output_a << ' ' << output_b;
  }
}

TEST(ColorConversions, LchToLab) {
  // Color conversions obtained from
  // https://colorjs.io/apps/convert/?color=purple&precision=4
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},  // black
      {{89.11f, 69.04f, 161.5f},
       {89.11f, -65.472265155436f, 21.906713478207564f}},
      {{29.6915239933531f, 66.82572352143814f, 327.1054738802461f},
       {29.6915239933531f, 56.11167248735513f,
        -36.292665028011974f}},  // purple
      {{38.14895894517021f, 59.598372928277406f, 32.286662896162966f},
       {38.14895894517021f, 50.38364171345111f, 31.834803335164764f}},  // brown
      {{46.27770902748027f, 67.9842594463414f, 134.3838583288382f},
       {46.27770902748027f, -47.55240796497723f,
        48.586294664234586f}}};  // green

  for (auto& color_pair : colors_tests) {
    auto [input_l, input_c, input_h] = color_pair.input;
    auto [expected_l, expected_a, expected_b] = color_pair.expected;
    auto [output_l, output_a, output_b] = LchToLab(input_l, input_c, input_h);
    EXPECT_NEAR(output_l, expected_l, 0.001f)
        << input_l << ' ' << input_c << ' ' << input_h << " to " << expected_l
        << ' ' << expected_a << ' ' << expected_b << " produced " << output_l
        << ' ' << output_a << ' ' << output_b;
    EXPECT_NEAR(output_a, expected_a, 0.001f)
        << input_l << ' ' << input_c << ' ' << input_h << " to " << expected_l
        << ' ' << expected_a << ' ' << expected_b << " produced " << output_l
        << ' ' << output_a << ' ' << output_b;
    EXPECT_NEAR(output_b, expected_b, 0.001f)
        << input_l << ' ' << input_c << ' ' << input_h << " to " << expected_l
        << ' ' << expected_a << ' ' << expected_b << " produced " << output_l
        << ' ' << output_a << ' ' << output_b;
  }
}

TEST(ColorConversions, LabToLch) {
  // Color conversions obtained from
  // https://colorjs.io/apps/convert/?color=purple&precision=4
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},  // black
      {{100.0f, 0.0f, 0.0f}, {100.0f, 0.0f, 0.0f}},
      {{89.11f, -65.472265155436f, 21.906713478207564f},
       {89.11f, 69.04f, 161.5f}},
      {{29.6915239933531f, 56.11167248735513f, -36.292665028011974f},
       {29.6915239933531f, 66.82572352143814f,
        -32.894523620605469f}},  // purple
      {{38.14895894517021f, 50.38364171345111f, 31.834803335164764f},
       {38.14895894517021f, 59.598372928277406f,
        32.286662896162966f}},  // brown
      {{46.27770902748027f, -47.55240796497723f, 48.586294664234586f},
       {46.27770902748027f, 67.9842594463414f, 134.3838583288382f}}};  // green

  for (auto& color_pair : colors_tests) {
    auto [input_l, input_a, input_b] = color_pair.input;
    auto [expected_l, expected_c, expected_h] = color_pair.expected;
    auto [output_l, output_c, output_h] = LabToLch(input_l, input_a, input_b);
    EXPECT_NEAR(output_l, expected_l, 0.001f)
        << input_l << ' ' << input_a << ' ' << input_b << " to " << expected_l
        << ' ' << expected_c << ' ' << expected_h << " produced " << output_l
        << ' ' << output_c << ' ' << output_h;
    EXPECT_NEAR(output_c, expected_c, 0.001f)
        << input_l << ' ' << input_a << ' ' << input_b << " to " << expected_l
        << ' ' << expected_c << ' ' << expected_h << " produced " << output_l
        << ' ' << output_c << ' ' << output_h;
    EXPECT_NEAR(output_h, expected_h, 0.001f)
        << input_l << ' ' << input_a << ' ' << input_b << " to " << expected_l
        << ' ' << expected_c << ' ' << expected_h << " produced " << output_l
        << ' ' << output_c << ' ' << output_h;
  }
}

TEST(ColorConversions, SRGBToXYZD50) {
  // Color conversions obtained from
  // https://www.nixsensor.com/free-color-converter/
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},  // black
      {{1.0f, 1.0f, 1.0f},
       {0.9642956660812443f, 1.0000000361162846f,
        0.8251045485672053f}},  // white
      {{0.0f, 1.0f, 0.0f},
       {0.3851514688337912f, 0.7168870538238823f,
        0.09708128566574631f}},  // lime
      {{0.6470588235294118f, 0.16470588235294117f, 0.16470588235294117f},
       {0.1763053229982614f, 0.10171766135467991f,
        0.024020600356509242f}},  // brown
      {{1.0f, 0.7529411764705882f, 0.796078431372549f},
       {0.7245316165924385f, 0.6365774485679174f,
        0.4915583325045292f}}};  // pink

  for (auto& color_pair : colors_tests) {
    auto [input_r, input_g, input_b] = color_pair.input;
    auto [expected_x, expected_y, expected_z] = color_pair.expected;
    auto [output_x, output_y, output_z] =
        SRGBToXYZD50(input_r, input_g, input_b);
    EXPECT_NEAR(output_x, expected_x, 0.001f)
        << input_r << ' ' << input_g << ' ' << input_b << " to " << expected_x
        << ' ' << expected_y << ' ' << expected_z << " produced " << output_x
        << ' ' << output_y << ' ' << output_z;
    EXPECT_NEAR(output_y, expected_y, 0.001f)
        << input_r << ' ' << input_g << ' ' << input_b << " to " << expected_x
        << ' ' << expected_y << ' ' << expected_z << " produced " << output_x
        << ' ' << output_y << ' ' << output_z;
    EXPECT_NEAR(output_z, expected_z, 0.001f)
        << input_r << ' ' << input_g << ' ' << input_b << " to " << expected_x
        << ' ' << expected_y << ' ' << expected_z << " produced " << output_x
        << ' ' << output_y << ' ' << output_z;
  }
}

TEST(ColorConversions, SRGBToHSL) {
  // Color conversions obtained from
  // https://colorjs.io/apps/convert/?color=purple&precision=4
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},                      // black
      {{1.0f, 1.0f, 1.0f}, {0.0f, 0.f, 1.f}},                        // white
      {{0.0f, 1.0f, 0.0f}, {120.0f, 1.0f, 0.5f}},                    // lime
      {{0.64706f, 0.16471f, 0.16471f}, {0.0f, 0.59420f, 0.40588f}},  // brown
      {{0.50196f, 0.0f, 0.50196f}, {300.0f, 1.0f, 0.25098f}},        // purple
      {{1.0f, 0.75294f, 0.79608f}, {349.5238f, 1.00f, 0.87647f}}};   // pink

  for (auto& color_pair : colors_tests) {
    auto [input_r, input_g, input_b] = color_pair.input;
    auto [expected_h, expected_s, expected_l] = color_pair.expected;
    auto [output_h, output_s, output_l] = SRGBToHSL(input_r, input_g, input_b);
    EXPECT_NEAR(output_h, expected_h, 0.01f)
        << input_r << ' ' << input_g << ' ' << input_b << " to " << expected_h
        << ' ' << expected_s << ' ' << expected_l << " produced " << output_h
        << ' ' << output_s << ' ' << output_l;
    EXPECT_NEAR(output_s, expected_s, 0.001f)
        << input_r << ' ' << input_g << ' ' << input_b << " to " << expected_h
        << ' ' << expected_s << ' ' << expected_l << " produced " << output_h
        << ' ' << output_s << ' ' << output_l;
    EXPECT_NEAR(output_l, expected_l, 0.001f)
        << input_r << ' ' << input_g << ' ' << input_b << " to " << expected_h
        << ' ' << expected_s << ' ' << expected_l << " produced " << output_h
        << ' ' << output_s << ' ' << output_l;
  }
}

TEST(ColorConversions, SRGBToHWB) {
  // Color conversions obtained from
  // https://colorjs.io/apps/convert/?color=purple&precision=4
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},                      // black
      {{1.0f, 1.0f, 1.0f}, {0.0f, 1.f, 0.f}},                        // white
      {{0.5, 0.5, 0.5}, {0.0f, 0.5f, 0.5f}},                         // grey
      {{0.0f, 1.0f, 0.0f}, {120.0f, 0.0f, 0.0f}},                    // lime
      {{0.64706f, 0.16471f, 0.16471f}, {0.0f, 0.16471f, 0.35294f}},  // brown
      {{0.50196f, 0.0f, 0.50196f}, {300.0f, 0.0f, 0.49804f}},        // purple
      {{1.0f, 0.75294f, 0.79608f}, {349.5238f, 0.75294f, 0.0f}}};    // pink

  for (auto& color_pair : colors_tests) {
    auto [input_r, input_g, input_b] = color_pair.input;
    auto [expected_h, expected_w, expected_b] = color_pair.expected;
    auto [output_h, output_w, output_b] = SRGBToHWB(input_r, input_g, input_b);
    EXPECT_NEAR(output_h, expected_h, 0.01f)
        << input_r << ' ' << input_g << ' ' << input_b << " to " << expected_h
        << ' ' << expected_w << ' ' << expected_b << " produced " << output_h
        << ' ' << output_w << ' ' << output_b;
    EXPECT_NEAR(output_w, expected_w, 0.001f)
        << input_r << ' ' << input_g << ' ' << input_b << " to " << expected_h
        << ' ' << expected_w << ' ' << expected_b << " produced " << output_h
        << ' ' << output_w << ' ' << output_b;
    EXPECT_NEAR(output_b, expected_b, 0.001f)
        << input_r << ' ' << input_g << ' ' << input_b << " to " << expected_h
        << ' ' << expected_w << ' ' << expected_b << " produced " << output_h
        << ' ' << output_w << ' ' << output_b;
  }
}

TEST(ColorConversions, XYZD50ToSRGB) {
  // Color conversions obtained from
  // https://www.nixsensor.com/free-color-converter/
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},  // black
      {{0.9642956660812443f, 1.0000000361162846f, 0.8251045485672053f},
       {1.0f, 1.0f, 1.0f}},  // white
      {{0.3851514688337912f, 0.7168870538238823f, 0.09708128566574631f},
       {0.0f, 1.0f, 0.0f}},  // lime
      {{0.1763053229982614f, 0.10171766135467991f, 0.024020600356509242f},
       {0.6470588235294118f, 0.16470588235294117f,
        0.16470588235294117f}},  // brown
      {{0.7245316165924385f, 0.6365774485679174f, 0.4915583325045292f},
       {1.0f, 0.7529411764705882f, 0.796078431372549f}}};  // pink

  for (auto& color_pair : colors_tests) {
    auto [input_x, input_y, input_z] = color_pair.input;
    auto [expected_r, expected_g, expected_b] = color_pair.expected;
    auto [fR, fG, fB] = XYZD50ToSRGB(input_x, input_y, input_z);
    EXPECT_NEAR(fR, expected_r, 0.01f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << fR << ' '
        << fG << ' ' << fB;
    EXPECT_NEAR(fG, expected_g, 0.01f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << fR << ' '
        << fG << ' ' << fB;
    EXPECT_NEAR(fB, expected_b, 0.01f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << fR << ' '
        << fG << ' ' << fB;
  }
}

TEST(ColorConversions, HSLToSRGB) {
  // Color conversions obtained from
  // https://colorjs.io/apps/convert/?color=purple&precision=4
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},                      // black
      {{0.0f, 0.f, 1.f}, {1.0f, 1.0f, 1.0f}},                        // white
      {{120.0f, 1.0f, 0.5f}, {0.0f, 1.0f, 0.0f}},                    // lime
      {{0.0f, 0.59420f, 0.40588f}, {0.64706f, 0.16471f, 0.16471f}},  // brown
      {{300.0f, 1.0f, 0.25098f}, {0.50196f, 0.0f, 0.50196f}},        // purple
      {{349.5238f, 1.00f, 0.87647f}, {1.0f, 0.75294f, 0.79608f}}};   // pink

  for (auto& color_pair : colors_tests) {
    auto [input_h, input_s, input_l] = color_pair.input;
    auto [expected_r, expected_g, expected_b] = color_pair.expected;
    auto [fR, fG, fB] = HSLToSRGB(input_h, input_s, input_l);
    EXPECT_NEAR(fR, expected_r, 0.01f)
        << input_h << ' ' << input_s << ' ' << input_l << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << fR << ' '
        << fG << ' ' << fB;
    EXPECT_NEAR(fG, expected_g, 0.01f)
        << input_h << ' ' << input_s << ' ' << input_l << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << fR << ' '
        << fG << ' ' << fB;
    EXPECT_NEAR(fB, expected_b, 0.01f)
        << input_h << ' ' << input_s << ' ' << input_l << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << fR << ' '
        << fG << ' ' << fB;
  }
}

TEST(ColorConversions, HWBToSRGB) {
  // Color conversions obtained from
  // https://colorjs.io/apps/convert/?color=purple&precision=4
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}},                      // black
      {{0.0f, 1.f, 0.f}, {1.0f, 1.0f, 1.0f}},                        // white
      {{0.0f, 0.5f, 0.5f}, {0.5, 0.5, 0.5}},                         // grey
      {{5.0f, 0.5f, 0.5f}, {0.5, 0.5, 0.5}},                         // grey
      {{120.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},                    // lime
      {{0.0f, 0.16471f, 0.35294f}, {0.64706f, 0.16471f, 0.16471f}},  // brown
      {{300.0f, 0.0f, 0.49804f}, {0.50196f, 0.0f, 0.50196f}},        // purple
      {{349.5238f, 0.75294f, 0.0f}, {1.0f, 0.75294f, 0.79608f}}};    // pink

  for (auto& color_pair : colors_tests) {
    auto [input_h, input_w, input_b] = color_pair.input;
    auto [expected_r, expected_g, expected_b] = color_pair.expected;
    auto [fR, fG, fB] = HWBToSRGB(input_h, input_w, input_b);
    EXPECT_NEAR(fR, expected_r, 0.01f)
        << input_h << ' ' << input_w << ' ' << input_b << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << fR << ' '
        << fG << ' ' << fB;
    EXPECT_NEAR(fG, expected_g, 0.01f)
        << input_h << ' ' << input_w << ' ' << input_b << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << fR << ' '
        << fG << ' ' << fB;
    EXPECT_NEAR(fB, expected_b, 0.01f)
        << input_h << ' ' << input_w << ' ' << input_b << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << fR << ' '
        << fG << ' ' << fB;
  }
}

}  // namespace gfx
