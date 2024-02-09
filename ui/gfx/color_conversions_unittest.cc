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

TEST(ColorConversions, OklabToXYZD65) {
  // Color conversions obtained from
  // https://colorjs.io/apps/convert/?color=lime&precision=4
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},  // black
      {{1.00f, 0.0, 0.0f},
       {0.9504559270516717f, 1.0f, 1.0890577507598784f}},  // white
      {{0.8664396115356694f, -0.23388757418790818f, 0.17949847989672985f},
       {0.357584339383878f, 0.715168678767756f, 0.11919477979462598f}},  // lime
      {{0.4209136612058102f, 0.16470430417002319f, -0.10147178154592906f},
       {0.1279775574172914f, 0.06148383144929487f,
        0.20935510595451154f}},  // purple
      {{0.4806125447400232f, 0.1440294785250731f, 0.0688902950420287f},
       {0.167625056565021f, 0.09823806119130823f,
        0.03204123425728893f}},  // brown
      {{0.5197518277948419f, -0.14030232755310995f, 0.10767589774360209f},
       {0.07718833433230218f, 0.15437666866460437f,
        0.025729444777434055f}}};  // green

  for (auto& color_pair : colors_tests) {
    auto [input_l, input_a, input_b] = color_pair.input;
    auto [expected_x, expected_y, expected_z] = color_pair.expected;
    auto [output_x, output_y, output_z] =
        OklabToXYZD65(input_l, input_a, input_b);
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

TEST(ColorConversions, XYZD65ToOklab) {
  // Color conversions obtained from
  // https://colorjs.io/apps/convert/?color=lime&precision=4
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},  // black
      {{0.9504559270516717f, 1.0f, 1.0890577507598784f},
       {1.00f, 0.0, 0.0f}},  // white
      {{0.357584339383878f, 0.715168678767756f, 0.11919477979462598f},
       {0.8664396115356694f, -0.23388757418790818f,
        0.17949847989672985f}},  // lime
      {{0.1279775574172914f, 0.06148383144929487f, 0.20935510595451154f},
       {0.4209136612058102f, 0.16470430417002319f,
        -0.10147178154592906f}},  // purple
      {{0.167625056565021f, 0.09823806119130823f, 0.03204123425728893f},
       {0.4806125447400232f, 0.1440294785250731f,
        0.0688902950420287f}},  // brown
      {{0.07718833433230218f, 0.15437666866460437f, 0.025729444777434055f},
       {0.5197518277948419f, -0.14030232755310995f,
        0.10767589774360209f}}};  // green

  for (auto& color_pair : colors_tests) {
    auto [input_l, input_a, input_b] = color_pair.input;
    auto [expected_x, expected_y, expected_z] = color_pair.expected;
    auto [output_x, output_y, output_z] =
        XYZD65ToOklab(input_l, input_a, input_b);
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

TEST(ColorConversions, XYZD50ToD65) {
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
        XYZD50ToD65(input_x, input_y, input_z);
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

TEST(ColorConversions, XYZD65ToD50) {
  // Color conversions obtained from
  // https://www.nixsensor.com/free-color-converter/
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},                      // black
      {{0.95392f, 1.00594f, 1.439698f}, {0.95047f, 1.0f, 1.0888f}},  // white
      {{0.389938f, 0.20384f, 0.025982f}, {0.412, 0.213f, 0.019f}},
      {{0.33307f, 0.714494f, 0.1480589f}, {0.358f, 0.715f, 0.119f}},
      {{0.23041847f, 0.087602f, 1.264587f}, {0.18f, 0.072f, 0.95f}},
      {{0.252396f, 0.113222f, 0.73899f}, {0.23f, 0.107f, 0.555f}},
      {{0.112348f, 0.089496f, 0.115299f}, {0.114f, 0.09f, 0.087f}}};

  for (auto& color_pair : colors_tests) {
    auto [input_x, input_y, input_z] = color_pair.input;
    auto [expected_x, expected_y, expected_z] = color_pair.expected;
    auto [output_x, output_y, output_z] =
        XYZD65ToD50(input_x, input_y, input_z);
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

TEST(ColorConversions, XYZD50TosRGBLinear) {
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
        XYZD50TosRGBLinear(input_x, input_y, input_z);
    auto [x, y, z] = XYZD50ToD65(input_x, input_y, input_z);
    auto [expected_r, expected_g, expected_b] = XYZD65TosRGBLinear(x, y, z);
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

TEST(ColorConversions, LchToSkColor4f) {
  // Color conversions obtained from
  // https://colorjs.io/apps/convert/?color=purple&precision=4
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},  // black
      {{87.81853633115202f, 113.33150206540324f, 134.38385832883824f},
       {0.0f, 1.0f, 0.0f}},  // lime
      {{29.6915239933531f, 66.82572352143814f, 327.1054738802461f},
       {0.5019607843137255f, 0.0f, 0.5019607843137255f}},  // purple
      {{38.14895894517021f, 59.598372928277406f, 32.286662896162966f},
       {0.6470588235294118f, 0.16470588235294117f,
        0.16470588235294117f}},  // brown
      {{46.27770902748027f, 67.9842594463414f, 134.3838583288382f},
       {0.0f, 0.5019607843137255f, 0.0f}}};  // green

  for (auto& color_pair : colors_tests) {
    auto [input_l, input_c, input_h] = color_pair.input;
    auto [expected_r, expected_g, expected_b] = color_pair.expected;
    SkColor4f color = LchToSkColor4f(input_l, input_c, input_h, 1.0f);
    EXPECT_NEAR(color.fR, expected_r, 0.01f)
        << input_l << ' ' << input_c << ' ' << input_h << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
    EXPECT_NEAR(color.fG, expected_g, 0.01f)
        << input_l << ' ' << input_c << ' ' << input_h << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
    EXPECT_NEAR(color.fB, expected_b, 0.01f)
        << input_l << ' ' << input_c << ' ' << input_h << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
  }
}

TEST(ColorConversions, OklchToSkColor4f) {
  // Color conversions obtained from
  // https://colorjs.io/apps/convert/?color=purple&precision=4
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},  // black
      {{0.8664396115356694f, 0.2948272403370167f, 142.49533888780996f},
       {0.0f, 1.0f, 0.0f}},  // lime
      {{0.4209136612058102f, 0.19345291484554133f, 328.36341792345144f},
       {0.5019607843137255f, 0.0f, 0.5019607843137255f}},  // purple
      {{0.4806125447400232f, 0.1596570181206647f, 25.562112067668068f},
       {0.6470588235294118f, 0.16470588235294117f,
        0.16470588235294117f}},  // brown
      {{0.5197518277948419f, 0.17685825418032036f, 142.4953388878099f},
       {0.0f, 0.5019607843137255f, 0.0f}}};  // green

  for (auto& color_pair : colors_tests) {
    auto [input_l, input_c, input_h] = color_pair.input;
    auto [expected_r, expected_g, expected_b] = color_pair.expected;
    SkColor4f color = OklchToSkColor4f(input_l, input_c, input_h, 1.0f);
    EXPECT_NEAR(color.fR, expected_r, 0.01f)
        << input_l << ' ' << input_c << ' ' << input_h << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
    EXPECT_NEAR(color.fG, expected_g, 0.01f)
        << input_l << ' ' << input_c << ' ' << input_h << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
    EXPECT_NEAR(color.fB, expected_b, 0.01f)
        << input_l << ' ' << input_c << ' ' << input_h << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
  }
}

TEST(ColorConversions, SRGBLinearToXYZD50) {
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
      {{0.37626212299090644f, 0.02315336617811041f, 0.02315336617811041f},
       {0.1763053229982614f, 0.10171766135467991f,
        0.024020600356509242f}},  // brown
      {{1.0f, 0.5271151257058131f, 0.5972017883637634f},
       {0.7245316165924385f, 0.6365774485679174f,
        0.4915583325045292f}}};  // pink

  for (auto& color_pair : colors_tests) {
    auto [input_r, input_g, input_b] = color_pair.input;
    auto [expected_x, expected_y, expected_z] = color_pair.expected;
    auto [output_x, output_y, output_z] =
        SRGBLinearToXYZD50(input_r, input_g, input_b);
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

TEST(ColorConversions, XYZD50ToSkColor4f) {
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
    SkColor4f color = XYZD50ToSkColor4f(input_x, input_y, input_z, 1.0f);
    EXPECT_NEAR(color.fR, expected_r, 0.01f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
    EXPECT_NEAR(color.fG, expected_g, 0.01f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
    EXPECT_NEAR(color.fB, expected_b, 0.01f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
  }
}

TEST(ColorConversions, XYZD65ToSkColor4f) {
  // Color conversions obtained from
  // https://www.nixsensor.com/free-color-converter/
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},  // black
      {{0.9504559270516717f, 1.f, 1.0890577507598784f},
       {1.0f, 1.0f, 1.0f}},  // white
      {{0.357584339383878f, 0.715168678767756f, 0.11919477979462598f},
       {0.0f, 1.0f, 0.0f}},  // lime
      {{0.167625056565021f, 0.09823806119130823f, 0.032041234257288932f},
       {0.6470588235294118f, 0.16470588235294117f,
        0.16470588235294117f}},  // brown
      {{0.7086623628695997f, 0.6327286137205872f, 0.6498196912712672f},
       {1.0f, 0.7529411764705882f, 0.796078431372549f}},  // pink
      {{1.0f, 1.0f, 1.0f}, {1.085f, 0.9769f, 0.9587f}}};

  for (auto& color_pair : colors_tests) {
    auto [input_x, input_y, input_z] = color_pair.input;
    auto [expected_r, expected_g, expected_b] = color_pair.expected;
    SkColor4f color = XYZD65ToSkColor4f(input_x, input_y, input_z, 1.0f);
    EXPECT_NEAR(color.fR, expected_r, 0.01f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
    EXPECT_NEAR(color.fG, expected_g, 0.01f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
    EXPECT_NEAR(color.fB, expected_b, 0.01f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
  }
}

TEST(ColorConversions, LabToSkColor4f) {
  // Color conversions obtained from
  // https://www.nixsensor.com/free-color-converter/
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},              // black
      {{100.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},            // white
      {{46.2775f, -47.521f, 48.5837f}, {0.0f, 0.5f, 0.0f}},  // green
      {{50.0f, 50.0f, 0.0f}, {0.756208f, 0.304487f, 0.475634f}},
      {{70.0f, -45.0f, 0.0f}, {0.10751f, 0.75558f, 0.66398f}},
      {{70.0f, 0.0f, 70.0f}, {0.766254f, 0.663607f, 0.055775f}},
      {{55.0f, 0.0f, -60.0f}, {0.128128f, 0.53105f, 0.927645f}},
      {{100.115f, 9.06448f, 5.80177f}, {1.085f, 0.9769f, 0.9587f}}};

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

TEST(ColorConversions, SRGBLinearToSkColor4f) {
  // Color conversions obtained from
  // https://www.nixsensor.com/free-color-converter/
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},  // black
      {{1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}},  // white
      {{0.f, 0.21586050011389923f, 0.f},
       {0.f, 0.5019607843137255f, 0.f}},  // green
      {{0.21586050011389923f, 0.f, 0.21586050011389923f},
       {0.5019607843137255f, 0.f, 0.5019607843137255f}},  // purple
      {{1.f, 0.5271151257058131f, 0.5972017883637634f},
       {1.f, 0.7529411764705882f, 0.796078431372549f}},  // pink
      {{0.37626212299090644f, 0.02315336617811041f, 0.02315336617811041f},
       {0.6470588235294118f, 0.16470588235294117f,
        0.16470588235294117f}}};  // brown

  for (auto& color_pair : colors_tests) {
    auto [input_r, input_g, input_b] = color_pair.input;
    auto [expected_r, expected_g, expected_b] = color_pair.expected;
    SkColor4f color = SRGBLinearToSkColor4f(input_r, input_g, input_b, 1.0f);
    EXPECT_NEAR(color.fR, expected_r, 0.01f)
        << input_r << ' ' << input_g << ' ' << input_b << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
    EXPECT_NEAR(color.fG, expected_g, 0.01f)
        << input_r << ' ' << input_g << ' ' << input_b << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
    EXPECT_NEAR(color.fB, expected_b, 0.01f)
        << input_r << ' ' << input_g << ' ' << input_b << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
  }
}

TEST(ColorConversions, DisplayP3ToXYZD50) {
  // Color conversions obtained from
  // https://colorjs.io/apps/convert/?color=purple&precision=4
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},  // black
      {{0.9999999999999999f, 0.9999999999999997f, 0.9999999999999999f},
       {0.9642956660812443f, 1.0000000361162846f,
        0.8251045485672053f}},  // white
      {{0.45840159019103005f, 0.9852645833250543f, 0.29829470783345835f},
       {0.3851514688337912f, 0.7168870538238823f,
        0.09708128566574631f}},  // lime
      {{0.5957181607237907f, 0.2055939145569215f, 0.18695695018247227f},
       {0.1763053229982614f, 0.10171766135467991f,
        0.024020600356509242f}},  // brown
      {{0.4584004101072638f, 0.07977226603250179f, 0.4847907338567859f},
       {0.1250143560558979f, 0.0611129099463755f,
        0.15715146562446167f}},  // purple
      {{0.962148711796773f, 0.7628803605364196f, 0.7971503318758075f},
       {0.7245316165924385f, 0.6365774485679174f,
        0.4915583325045292f}}};  // pink

  for (auto& color_pair : colors_tests) {
    auto [input_r, input_g, input_b] = color_pair.input;
    auto [expected_x, expected_y, expected_z] = color_pair.expected;
    auto [output_x, output_y, output_z] =
        DisplayP3ToXYZD50(input_r, input_g, input_b);
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

TEST(ColorConversions, XYZD50ToDisplayP3) {
  // Color conversions obtained from
  // https://colorjs.io/apps/convert/?color=purple&precision=4
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},  // black
      {{0.9642956660812443f, 1.0000000361162846f, 0.8251045485672053f},
       {0.9999999999999999f, 0.9999999999999997f,
        0.9999999999999999f}},  // white
      {{0.3851514688337912f, 0.7168870538238823f, 0.09708128566574631f},
       {0.45840159019103005f, 0.9852645833250543f,
        0.29829470783345835f}},  // lime
      {{0.1763053229982614f, 0.10171766135467991f, 0.024020600356509242f},
       {0.5957181607237907f, 0.2055939145569215f,
        0.18695695018247227f}},  // brown
      {{0.1250143560558979f, 0.0611129099463755f, 0.15715146562446167f},
       {0.4584004101072638f, 0.07977226603250179f,
        0.4847907338567859f}},  // purple
      {{0.7245316165924385f, 0.6365774485679174f, 0.4915583325045292f},
       {0.962148711796773f, 0.7628803605364196f,
        0.7971503318758075f}}};  // pink

  for (auto& color_pair : colors_tests) {
    auto [input_x, input_y, input_z] = color_pair.input;
    auto [expected_r, expected_g, expected_b] = color_pair.expected;
    auto [output_r, output_g, output_b] =
        XYZD50ToDisplayP3(input_x, input_y, input_z);
    EXPECT_NEAR(output_r, expected_r, 0.001f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << output_r
        << ' ' << output_g << ' ' << output_b;
    EXPECT_NEAR(output_g, expected_g, 0.001f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << output_r
        << ' ' << output_g << ' ' << output_b;
    EXPECT_NEAR(output_b, expected_b, 0.001f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << output_r
        << ' ' << output_g << ' ' << output_b;
  }
}

TEST(ColorConversions, DisplayP3ToSkColor4f) {
  // Color conversions obtained from
  // https://colorjs.io/apps/convert/?color=purple&precision=4
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},  // black
      {{0.9999999999999999f, 0.9999999999999997f, 0.9999999999999999f},
       {1.0f, 1.0f, 1.0f}},  // white
      {{0.45840159019103005f, 0.9852645833250543f, 0.29829470783345835f},
       {0.0f, 1.0f, 0.0f}},  // lime
      {{0.5957181607237907f, 0.2055939145569215f, 0.18695695018247227f},
       {0.6470588235294118f, 0.16470588235294117f,
        0.16470588235294117f}},  // brown
      {{0.4584004101072638f, 0.07977226603250179f, 0.4847907338567859f},
       {0.5019607843137255f, 0.0f, 0.5019607843137255f}},  // purple
      {{0.962148711796773f, 0.7628803605364196f, 0.7971503318758075f},
       {1.0f, 0.7529411764705882f, 0.796078431372549f}}};  // pink

  for (auto& color_pair : colors_tests) {
    auto [input_r, input_g, input_b] = color_pair.input;
    auto [expected_r, expected_g, expected_b] = color_pair.expected;
    SkColor4f color = DisplayP3ToSkColor4f(input_r, input_g, input_b, 1.0f);
    EXPECT_NEAR(color.fR, expected_r, 0.01f)
        << input_r << ' ' << input_g << ' ' << input_b << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
    EXPECT_NEAR(color.fG, expected_g, 0.01f)
        << input_r << ' ' << input_g << ' ' << input_b << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
    EXPECT_NEAR(color.fB, expected_b, 0.01f)
        << input_r << ' ' << input_g << ' ' << input_b << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
  }
}

TEST(ColorConversions, ProPhotoToXYZD50) {
  // Color conversions obtained from
  // https://colorjs.io/apps/convert/?color=pink&precision=4
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},  // black
      {{0.9999999886663737f, 1.0000000327777285f, 0.9999999636791804f},
       {0.9642956660812443f, 1.0000000361162846f,
        0.8251045485672053f}},  // white
      {{0.5402807890930262f, 0.9275948938161531f, 0.30456598218387576f},
       {0.3851514688337912f, 0.7168870538238823f,
        0.09708128566574631f}},  // lime
      {{0.4202512875251534f, 0.20537448341387265f, 0.14018716364460992f},
       {0.1763053229982614f, 0.10171766135467991f,
        0.024020600356509242f}},  // brown
      {{0.3415199027593793f, 0.13530888280806527f, 0.3980101298732242f},
       {0.1250143560558979f, 0.0611129099463755f,
        0.15715146562446167f}},  // purple
      {{0.8755612852965058f, 0.7357597566543541f, 0.7499575746802042f},
       {0.7245316165924385f, 0.6365774485679174f,
        0.4915583325045292f}}};  // pink

  for (auto& color_pair : colors_tests) {
    auto [input_r, input_g, input_b] = color_pair.input;
    auto [expected_x, expected_y, expected_z] = color_pair.expected;
    auto [output_x, output_y, output_z] =
        ProPhotoToXYZD50(input_r, input_g, input_b);
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

TEST(ColorConversions, XYZD50ToProPhoto) {
  // Color conversions obtained from
  // https://colorjs.io/apps/convert/?color=pink&precision=4
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},  // black
      {{0.9642956660812443f, 1.0000000361162846f, 0.8251045485672053f},
       {0.9999999886663737f, 1.0000000327777285f,
        0.9999999636791804f}},  // white
      {{0.3851514688337912f, 0.7168870538238823f, 0.09708128566574631f},
       {0.5402807890930262f, 0.9275948938161531f,
        0.30456598218387576f}},  // lime
      {{0.1763053229982614f, 0.10171766135467991f, 0.024020600356509242f},
       {0.4202512875251534f, 0.20537448341387265f,
        0.14018716364460992f}},  // brown
      {{0.1250143560558979f, 0.0611129099463755f, 0.15715146562446167f},
       {0.3415199027593793f, 0.13530888280806527f,
        0.3980101298732242f}},  // purple
      {{0.7245316165924385f, 0.6365774485679174f, 0.4915583325045292f},
       {0.8755612852965058f, 0.7357597566543541f,
        0.7499575746802042f}}};  // pink

  for (auto& color_pair : colors_tests) {
    auto [input_x, input_y, input_z] = color_pair.input;
    auto [expected_r, expected_g, expected_b] = color_pair.expected;
    auto [output_r, output_g, output_b] =
        XYZD50ToProPhoto(input_x, input_y, input_z);
    EXPECT_NEAR(output_r, expected_r, 0.001f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << output_r
        << ' ' << output_g << ' ' << output_b;
    EXPECT_NEAR(output_g, expected_g, 0.001f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << output_r
        << ' ' << output_g << ' ' << output_b;
    EXPECT_NEAR(output_b, expected_b, 0.001f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << output_r
        << ' ' << output_g << ' ' << output_b;
  }
}

TEST(ColorConversions, ProPhotoToSkColor4f) {
  // Color conversions obtained from
  // https://colorjs.io/apps/convert/?color=pink&precision=4
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},  // black
      {{0.9999999886663737f, 1.0000000327777285f, 0.9999999636791804f},
       {1.0f, 1.0f, 1.0f}},  // white
      {{0.5402807890930262f, 0.9275948938161531f, 0.30456598218387576f},
       {0.0f, 1.0f, 0.0f}},  // lime
      {{0.4202512875251534f, 0.20537448341387265f, 0.14018716364460992f},
       {0.6470588235294118f, 0.16470588235294117f,
        0.16470588235294117f}},  // brown
      {{0.3415199027593793f, 0.13530888280806527f, 0.3980101298732242f},
       {0.5019607843137255f, 0.0f, 0.5019607843137255f}},  // purple
      {{0.8755612852965058f, 0.7357597566543541f, 0.7499575746802042f},
       {1.0f, 0.7529411764705882f, 0.796078431372549f}}};  // pink

  for (auto& color_pair : colors_tests) {
    auto [input_r, input_g, input_b] = color_pair.input;
    auto [expected_r, expected_g, expected_b] = color_pair.expected;
    SkColor4f color = ProPhotoToSkColor4f(input_r, input_g, input_b, 1.0f);
    EXPECT_NEAR(color.fR, expected_r, 0.01f)
        << input_r << ' ' << input_g << ' ' << input_b << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
    EXPECT_NEAR(color.fG, expected_g, 0.01f)
        << input_r << ' ' << input_g << ' ' << input_b << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
    EXPECT_NEAR(color.fB, expected_b, 0.01f)
        << input_r << ' ' << input_g << ' ' << input_b << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
  }
}

TEST(ColorConversions, AdobeRGBToXYZD50) {
  // Color conversions obtained from
  // https://colorjs.io/apps/convert/?color=purple&precision=4
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},  // black
      {{1.0000000000000002f, 0.9999999999999999f, 1.f},
       {0.9642956660812443f, 1.0000000361162846f,
        0.8251045485672053f}},  // white
      {{0.564972265988564f, 0.9999999999999999f, 0.23442379872902916f},
       {0.3851514688337912f, 0.7168870538238823f,
        0.09708128566574631f}},  // lime
      {{0.5565979160264471f, 0.18045907254050694f, 0.18045907254050705f},
       {0.1763053229982614f, 0.10171766135467991f,
        0.024020600356509242f}},  // brown
      {{0.4275929819700999f, 0.0f, 0.4885886519419426f},
       {0.1250143560558979f, 0.0611129099463755f,
        0.15715146562446167f}},  // purple
      {{0.9363244100721754f, 0.7473920857106169f, 0.7893042668092753f},
       {0.7245316165924385f, 0.6365774485679174f,
        0.4915583325045292f}}};  // pink

  for (auto& color_pair : colors_tests) {
    auto [input_r, input_g, input_b] = color_pair.input;
    auto [expected_x, expected_y, expected_z] = color_pair.expected;
    auto [output_x, output_y, output_z] =
        AdobeRGBToXYZD50(input_r, input_g, input_b);
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

TEST(ColorConversions, XYZD50ToAdobeRGB) {
  // Color conversions obtained from
  // https://colorjs.io/apps/convert/?color=purple&precision=4
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},  // black
      {{0.9642956660812443f, 1.0000000361162846f, 0.8251045485672053f},
       {1.0000000000000002f, 0.9999999999999999f, 1.f}},  // white
      {{0.3851514688337912f, 0.7168870538238823f, 0.09708128566574631f},
       {0.564972265988564f, 0.9999999999999999f,
        0.23442379872902916f}},  // lime
      {{0.1763053229982614f, 0.10171766135467991f, 0.024020600356509242f},
       {0.5565979160264471f, 0.18045907254050694f,
        0.18045907254050705f}},  // brown
      {{0.1250143560558979f, 0.0611129099463755f, 0.15715146562446167f},
       {0.4275929819700999f, 0.0f, 0.4885886519419426f}},  // purple
      {{0.7245316165924385f, 0.6365774485679174f, 0.4915583325045292f},
       {0.9363244100721754f, 0.7473920857106169f,
        0.7893042668092753f}}};  // pink

  for (auto& color_pair : colors_tests) {
    auto [input_x, input_y, input_z] = color_pair.input;
    auto [expected_r, expected_g, expected_b] = color_pair.expected;
    auto [output_r, output_g, output_b] =
        XYZD50ToAdobeRGB(input_x, input_y, input_z);
    EXPECT_NEAR(output_r, expected_r, 0.01f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << output_r
        << ' ' << output_g << ' ' << output_b;
    EXPECT_NEAR(output_g, expected_g, 0.01f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << output_r
        << ' ' << output_g << ' ' << output_b;
    EXPECT_NEAR(output_b, expected_b, 0.01f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << output_r
        << ' ' << output_g << ' ' << output_b;
  }
}

TEST(ColorConversions, AdobeRGBToSkColor4f) {
  // Color conversions obtained from
  // https://colorjs.io/apps/convert/?color=purple&precision=4
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},  // black
      {{1.0000000000000002f, 0.9999999999999999f, 1.f},
       {1.0f, 1.0f, 1.0f}},  // white
      {{0.564972265988564f, 0.9999999999999999f, 0.23442379872902916f},
       {0.0f, 1.0f, 0.0f}},  // lime
      {{0.5565979160264471f, 0.18045907254050694f, 0.18045907254050705f},
       {0.6470588235294118f, 0.16470588235294117f,
        0.16470588235294117f}},  // brown
      {{0.4275929819700999f, 0.0f, 0.4885886519419426f},
       {0.5019607843137255f, 0.0f, 0.5019607843137255f}},  // purple
      {{0.9363244100721754f, 0.7473920857106169f, 0.7893042668092753f},
       {1.0f, 0.7529411764705882f, 0.796078431372549f}}};  // pink

  for (auto& color_pair : colors_tests) {
    auto [input_r, input_g, input_b] = color_pair.input;
    auto [expected_r, expected_g, expected_b] = color_pair.expected;
    SkColor4f color = AdobeRGBToSkColor4f(input_r, input_g, input_b, 1.0f);
    EXPECT_NEAR(color.fR, expected_r, 0.01f)
        << input_r << ' ' << input_g << ' ' << input_b << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
    EXPECT_NEAR(color.fG, expected_g, 0.01f)
        << input_r << ' ' << input_g << ' ' << input_b << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
    EXPECT_NEAR(color.fB, expected_b, 0.01f)
        << input_r << ' ' << input_g << ' ' << input_b << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
  }
}

TEST(ColorConversions, Rec2020ToXYZD50) {
  // Color conversions obtained from
  // https://colorjs.io/apps/convert/?color=purple&precision=4
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},  // black
      {{1.0000000000000002f, 1.f, 1.f},
       {0.9642956660812443f, 1.0000000361162846f,
        0.8251045485672053f}},  // white
      {{0.5675424725933591f, 0.959278677099374f, 0.2689692617052188f},
       {0.3851514688337912f, 0.7168870538238823f,
        0.09708128566574631f}},  // lime
      {{0.4841434514625542f, 0.17985588424119636f, 0.12395667053434403f},
       {0.1763053229982614f, 0.10171766135467991f,
        0.024020600356509242f}},  // brown
      {{0.36142160262090384f, 0.0781562275109019f, 0.429742223818931f},
       {0.1250143560558979f, 0.0611129099463755f,
        0.15715146562446167f}},  // purple
      {{0.9098509851821579f, 0.747938726996672f, 0.7726929727190115f},
       {0.7245316165924385f, 0.6365774485679174f,
        0.4915583325045292f}}};  // pink

  for (auto& color_pair : colors_tests) {
    auto [input_r, input_g, input_b] = color_pair.input;
    auto [expected_x, expected_y, expected_z] = color_pair.expected;
    auto [output_x, output_y, output_z] =
        Rec2020ToXYZD50(input_r, input_g, input_b);
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

TEST(ColorConversions, XYZD50ToRec2020) {
  // Color conversions obtained from
  // https://colorjs.io/apps/convert/?color=purple&precision=4
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},  // black
      {{0.9642956660812443f, 1.0000000361162846f, 0.8251045485672053f},
       {1.0000000000000002f, 1.f, 1.f}},  // white
      {{0.3851514688337912f, 0.7168870538238823f, 0.09708128566574631f},
       {0.5675424725933591f, 0.959278677099374f, 0.2689692617052188f}},  // lime
      {{0.1763053229982614f, 0.10171766135467991f, 0.024020600356509242f},
       {0.4841434514625542f, 0.17985588424119636f,
        0.12395667053434403f}},  // brown
      {{0.1250143560558979f, 0.0611129099463755f, 0.15715146562446167f},
       {0.36142160262090384f, 0.0781562275109019f,
        0.429742223818931f}},  // purple
      {{0.7245316165924385f, 0.6365774485679174f, 0.4915583325045292f},
       {0.9098509851821579f, 0.747938726996672f,
        0.7726929727190115f}}};  // pink

  for (auto& color_pair : colors_tests) {
    auto [input_x, input_y, input_z] = color_pair.input;
    auto [expected_r, expected_g, expected_b] = color_pair.expected;
    auto [output_r, output_g, output_b] =
        XYZD50ToRec2020(input_x, input_y, input_z);
    EXPECT_NEAR(output_r, expected_r, 0.001f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << output_r
        << ' ' << output_g << ' ' << output_b;
    EXPECT_NEAR(output_g, expected_g, 0.001f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << output_r
        << ' ' << output_g << ' ' << output_b;
    EXPECT_NEAR(output_b, expected_b, 0.001f)
        << input_x << ' ' << input_y << ' ' << input_z << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << output_r
        << ' ' << output_g << ' ' << output_b;
  }
}

TEST(ColorConversions, Rec2020ToSkColor4f) {
  // Color conversions obtained from
  // https://colorjs.io/apps/convert/?color=purple&precision=4
  ColorTest colors_tests[] = {
      {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},               // black
      {{1.0000000000000002f, 1.f, 1.f}, {1.0f, 1.0f, 1.0f}},  // white
      {{0.5675424725933591f, 0.959278677099374f, 0.2689692617052188f},
       {0.0f, 1.0f, 0.0f}},  // lime
      {{0.4841434514625542f, 0.17985588424119636f, 0.12395667053434403f},
       {0.6470588235294118f, 0.16470588235294117f,
        0.16470588235294117f}},  // brown
      {{0.36142160262090384f, 0.0781562275109019f, 0.429742223818931f},
       {0.5019607843137255f, 0.0f, 0.5019607843137255f}},  // purple
      {{0.9098509851821579f, 0.747938726996672f, 0.7726929727190115f},
       {1.0f, 0.7529411764705882f, 0.796078431372549f}}};  // pink

  for (auto& color_pair : colors_tests) {
    auto [input_r, input_g, input_b] = color_pair.input;
    auto [expected_r, expected_g, expected_b] = color_pair.expected;
    SkColor4f color = Rec2020ToSkColor4f(input_r, input_g, input_b, 1.0f);
    EXPECT_NEAR(color.fR, expected_r, 0.01f)
        << input_r << ' ' << input_g << ' ' << input_b << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
    EXPECT_NEAR(color.fG, expected_g, 0.01f)
        << input_r << ' ' << input_g << ' ' << input_b << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
    EXPECT_NEAR(color.fB, expected_b, 0.01f)
        << input_r << ' ' << input_g << ' ' << input_b << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
  }
}

TEST(ColorConversions, HSLToSkColor4f) {
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
    SkColor4f color = HSLToSkColor4f(input_h, input_s, input_l, 1.0f);
    EXPECT_NEAR(color.fR, expected_r, 0.01f)
        << input_h << ' ' << input_s << ' ' << input_l << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
    EXPECT_NEAR(color.fG, expected_g, 0.01f)
        << input_h << ' ' << input_s << ' ' << input_l << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
    EXPECT_NEAR(color.fB, expected_b, 0.01f)
        << input_h << ' ' << input_s << ' ' << input_l << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
  }
}

TEST(ColorConversions, HWBToSkColor4f) {
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
    SkColor4f color = HWBToSkColor4f(input_h, input_w, input_b, 1.0f);
    EXPECT_NEAR(color.fR, expected_r, 0.01f)
        << input_h << ' ' << input_w << ' ' << input_b << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
    EXPECT_NEAR(color.fG, expected_g, 0.01f)
        << input_h << ' ' << input_w << ' ' << input_b << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
    EXPECT_NEAR(color.fB, expected_b, 0.01f)
        << input_h << ' ' << input_w << ' ' << input_b << " to " << expected_r
        << ' ' << expected_g << ' ' << expected_b << " produced " << color.fR
        << ' ' << color.fG << ' ' << color.fB;
  }
}

}  // namespace gfx
