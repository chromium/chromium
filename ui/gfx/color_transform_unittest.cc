// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>
#include <vector>

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/src/sksl/SkSLCompiler.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_transform.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/test/icc_profiles.h"
#include "ui/gfx/transform.h"

namespace gfx {

// Allowed error in most test.
const float kEpsilon = 1.5f / 255.f;

// Internal functions, exposted for testing.
GFX_EXPORT Transform GetTransferMatrix(ColorSpace::MatrixID id);

ColorSpace::PrimaryID all_primaries[] = {
    ColorSpace::PrimaryID::BT709,        ColorSpace::PrimaryID::BT470M,
    ColorSpace::PrimaryID::BT470BG,      ColorSpace::PrimaryID::SMPTE170M,
    ColorSpace::PrimaryID::SMPTE240M,    ColorSpace::PrimaryID::FILM,
    ColorSpace::PrimaryID::BT2020,       ColorSpace::PrimaryID::SMPTEST428_1,
    ColorSpace::PrimaryID::SMPTEST431_2, ColorSpace::PrimaryID::SMPTEST432_1,
};

ColorSpace::TransferID simple_transfers[] = {
    ColorSpace::TransferID::BT709,
    ColorSpace::TransferID::GAMMA22,
    ColorSpace::TransferID::GAMMA28,
    ColorSpace::TransferID::SMPTE170M,
    ColorSpace::TransferID::SMPTE240M,
    ColorSpace::TransferID::LINEAR,
    ColorSpace::TransferID::LOG,
    ColorSpace::TransferID::LOG_SQRT,
    ColorSpace::TransferID::IEC61966_2_4,
    ColorSpace::TransferID::BT1361_ECG,
    ColorSpace::TransferID::IEC61966_2_1,
    ColorSpace::TransferID::BT2020_10,
    ColorSpace::TransferID::BT2020_12,
    ColorSpace::TransferID::SMPTEST2084,
    ColorSpace::TransferID::ARIB_STD_B67,
    ColorSpace::TransferID::IEC61966_2_1_HDR,
};

// This one is weird as the non-linear numbers are not between 0 and 1.
ColorSpace::TransferID noninvertible_transfers[] = {
    ColorSpace::TransferID::SMPTEST428_1,
    ColorSpace::TransferID::SMPTEST2084_NON_HDR,
};

ColorSpace::TransferID extended_transfers[] = {
    ColorSpace::TransferID::LINEAR_HDR,
    ColorSpace::TransferID::IEC61966_2_1_HDR,
};

ColorSpace::MatrixID all_matrices[] = {
    ColorSpace::MatrixID::RGB, ColorSpace::MatrixID::BT709,
    ColorSpace::MatrixID::FCC, ColorSpace::MatrixID::BT470BG,
    ColorSpace::MatrixID::SMPTE170M, ColorSpace::MatrixID::SMPTE240M,

    // YCOCG produces lots of negative values which isn't compatible with many
    // transfer functions.
    // TODO(hubbe): Test this separately.
    // ColorSpace::MatrixID::YCOCG,
    ColorSpace::MatrixID::BT2020_NCL, ColorSpace::MatrixID::BT2020_CL,
    ColorSpace::MatrixID::YDZDX,
};

ColorSpace::RangeID all_ranges[] = {ColorSpace::RangeID::FULL,
                                    ColorSpace::RangeID::LIMITED,
                                    ColorSpace::RangeID::DERIVED};

ColorTransform::Intent intents[] = {ColorTransform::Intent::INTENT_ABSOLUTE,
                                    ColorTransform::Intent::TEST_NO_OPT};

TEST(SimpleColorSpace, BT709toSRGB) {
  ColorSpace bt709 = ColorSpace::CreateREC709();
  ColorSpace sRGB = ColorSpace::CreateSRGB();
  std::unique_ptr<ColorTransform> t(ColorTransform::NewColorTransform(
      bt709, sRGB, ColorTransform::Intent::INTENT_ABSOLUTE));

  ColorTransform::TriStim tmp(16.0f / 255.0f, 0.5f, 0.5f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 0.0f, 0.001f);
  EXPECT_NEAR(tmp.y(), 0.0f, 0.001f);
  EXPECT_NEAR(tmp.z(), 0.0f, 0.001f);

  tmp = ColorTransform::TriStim(235.0f / 255.0f, 0.5f, 0.5f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 1.0f, 0.001f);
  EXPECT_NEAR(tmp.y(), 1.0f, 0.001f);
  EXPECT_NEAR(tmp.z(), 1.0f, 0.001f);

  // Test a blue color
  tmp = ColorTransform::TriStim(128.0f / 255.0f, 240.0f / 255.0f, 0.5f);
  t->Transform(&tmp, 1);
  EXPECT_GT(tmp.z(), tmp.x());
  EXPECT_GT(tmp.z(), tmp.y());
}

TEST(SimpleColorSpace, BT2020CLtoBT2020RGB) {
  ColorSpace bt2020cl(
      ColorSpace::PrimaryID::BT2020, ColorSpace::TransferID::BT2020_10,
      ColorSpace::MatrixID::BT2020_CL, ColorSpace::RangeID::LIMITED);
  ColorSpace bt2020rgb(ColorSpace::PrimaryID::BT2020,
                       ColorSpace::TransferID::BT2020_10,
                       ColorSpace::MatrixID::RGB, ColorSpace::RangeID::FULL);
  std::unique_ptr<ColorTransform> t(ColorTransform::NewColorTransform(
      bt2020cl, bt2020rgb, ColorTransform::Intent::INTENT_ABSOLUTE));

  ColorTransform::TriStim tmp(16.0f / 255.0f, 0.5f, 0.5f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 0.0f, 0.001f);
  EXPECT_NEAR(tmp.y(), 0.0f, 0.001f);
  EXPECT_NEAR(tmp.z(), 0.0f, 0.001f);

  tmp = ColorTransform::TriStim(235.0f / 255.0f, 0.5f, 0.5f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 1.0f, 0.001f);
  EXPECT_NEAR(tmp.y(), 1.0f, 0.001f);
  EXPECT_NEAR(tmp.z(), 1.0f, 0.001f);

  // Test a blue color
  tmp = ColorTransform::TriStim(128.0f / 255.0f, 240.0f / 255.0f, 0.5f);
  t->Transform(&tmp, 1);
  EXPECT_GT(tmp.z(), tmp.x());
  EXPECT_GT(tmp.z(), tmp.y());
}

TEST(SimpleColorSpace, TransferFnCancel) {
  ColorSpace::PrimaryID primary = ColorSpace::PrimaryID::BT709;
  ColorSpace::MatrixID matrix = ColorSpace::MatrixID::RGB;
  ColorSpace::RangeID range = ColorSpace::RangeID::FULL;

  // BT709 has a gamma of 2.2222 (with some adjustments)
  ColorSpace bt709(primary, ColorSpace::TransferID::BT709, matrix, range);

  // IEC61966_2_1 has the sRGB gamma of 2.4 (with some adjustments)
  ColorSpace srgb(primary, ColorSpace::TransferID::IEC61966_2_1, matrix, range);

  // gamma28 is a simple exponential
  ColorSpace gamma28(primary, ColorSpace::TransferID::GAMMA28, matrix, range);

  // gamma24 is a simple exponential
  ColorSpace gamma24(primary, ColorSpace::TransferID::GAMMA24, matrix, range);

  // BT709 source is common for video and sRGB destination is common for
  // monitors. The two transfer functions are very close, and should cancel
  // out (so the transfer between them should be the identity). This particular
  // case is important for power reasons.
  std::unique_ptr<ColorTransform> bt709_to_srgb(
      ColorTransform::NewColorTransform(
          bt709, srgb, ColorTransform::Intent::INTENT_PERCEPTUAL));
  EXPECT_EQ(bt709_to_srgb->NumberOfStepsForTesting(), 0u);

  // Gamma 2.8 isn't even close to BT709 and won't cancel out (so we will have
  // two steps in the transform -- to-linear and from-linear).
  std::unique_ptr<ColorTransform> bt709_to_gamma28(
      ColorTransform::NewColorTransform(
          bt709, gamma28, ColorTransform::Intent::INTENT_PERCEPTUAL));
  EXPECT_EQ(bt709_to_gamma28->NumberOfStepsForTesting(), 2u);

  // Gamma 2.4 is closer to BT709, but not close enough to actually cancel out.
  std::unique_ptr<ColorTransform> bt709_to_gamma24(
      ColorTransform::NewColorTransform(
          bt709, gamma24, ColorTransform::Intent::INTENT_PERCEPTUAL));
  EXPECT_EQ(bt709_to_gamma24->NumberOfStepsForTesting(), 2u);

  // Rec 601 YUV to RGB conversion should have a single step.
  gfx::ColorSpace rec601 = gfx::ColorSpace::CreateREC601();
  std::unique_ptr<ColorTransform> rec601_yuv_to_rgb(
      ColorTransform::NewColorTransform(
          rec601, rec601.GetAsFullRangeRGB(),
          ColorTransform::Intent::INTENT_PERCEPTUAL));
  EXPECT_EQ(rec601_yuv_to_rgb->NumberOfStepsForTesting(), 1u);
}

TEST(SimpleColorSpace, SRGBFromICCAndNotICC) {
  float kEpsilon = 0.001f;
  ColorTransform::TriStim value_fromicc;
  ColorTransform::TriStim value_default;

  ICCProfile srgb_icc_profile = ICCProfileForTestingSRGB();
  ColorSpace srgb_fromicc = srgb_icc_profile.GetColorSpace();
  ColorSpace srgb_default = gfx::ColorSpace::CreateSRGB();
  ColorSpace xyzd50 = gfx::ColorSpace::CreateXYZD50();

  value_fromicc = value_default = ColorTransform::TriStim(0.1f, 0.5f, 0.9f);

  std::unique_ptr<ColorTransform> toxyzd50_fromicc(
      ColorTransform::NewColorTransform(
          srgb_fromicc, xyzd50, ColorTransform::Intent::INTENT_ABSOLUTE));
  // This will be converted to a transfer function and then linear transform.
  EXPECT_EQ(toxyzd50_fromicc->NumberOfStepsForTesting(), 2u);
  toxyzd50_fromicc->Transform(&value_fromicc, 1);

  std::unique_ptr<ColorTransform> toxyzd50_default(
      ColorTransform::NewColorTransform(
          srgb_default, xyzd50, ColorTransform::Intent::INTENT_ABSOLUTE));
  // This will have a transfer function and then linear transform.
  EXPECT_EQ(toxyzd50_default->NumberOfStepsForTesting(), 2u);
  toxyzd50_default->Transform(&value_default, 1);

  EXPECT_NEAR(value_fromicc.x(), value_default.x(), kEpsilon);
  EXPECT_NEAR(value_fromicc.y(), value_default.y(), kEpsilon);
  EXPECT_NEAR(value_fromicc.z(), value_default.z(), kEpsilon);

  value_fromicc = value_default = ColorTransform::TriStim(0.1f, 0.5f, 0.9f);

  std::unique_ptr<ColorTransform> fromxyzd50_fromicc(
      ColorTransform::NewColorTransform(
          xyzd50, srgb_fromicc, ColorTransform::Intent::INTENT_ABSOLUTE));
  fromxyzd50_fromicc->Transform(&value_fromicc, 1);

  std::unique_ptr<ColorTransform> fromxyzd50_default(
      ColorTransform::NewColorTransform(
          xyzd50, srgb_default, ColorTransform::Intent::INTENT_ABSOLUTE));
  fromxyzd50_default->Transform(&value_default, 1);

  EXPECT_NEAR(value_fromicc.x(), value_default.x(), kEpsilon);
  EXPECT_NEAR(value_fromicc.y(), value_default.y(), kEpsilon);
  EXPECT_NEAR(value_fromicc.z(), value_default.z(), kEpsilon);
}

TEST(SimpleColorSpace, BT709toSRGBICC) {
  ICCProfile srgb_icc = ICCProfileForTestingSRGB();
  ColorSpace bt709 = ColorSpace::CreateREC709();
  ColorSpace sRGB = srgb_icc.GetColorSpace();
  std::unique_ptr<ColorTransform> t(ColorTransform::NewColorTransform(
      bt709, sRGB, ColorTransform::Intent::INTENT_ABSOLUTE));

  ColorTransform::TriStim tmp(16.0f / 255.0f, 0.5f, 0.5f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 0.0f, 0.001f);
  EXPECT_NEAR(tmp.y(), 0.0f, 0.001f);
  EXPECT_NEAR(tmp.z(), 0.0f, 0.001f);

  tmp = ColorTransform::TriStim(235.0f / 255.0f, 0.5f, 0.5f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 1.0f, 0.001f);
  EXPECT_NEAR(tmp.y(), 1.0f, 0.001f);
  EXPECT_NEAR(tmp.z(), 1.0f, 0.001f);

  // Test a blue color
  tmp = ColorTransform::TriStim(128.0f / 255.0f, 240.0f / 255.0f, 0.5f);
  t->Transform(&tmp, 1);
  EXPECT_GT(tmp.z(), tmp.x());
  EXPECT_GT(tmp.z(), tmp.y());
}

TEST(SimpleColorSpace, ICCProfileOnlyXYZ) {
  const float kEpsilon = 2.5f / 255.f;
  ICCProfile icc_profile = ICCProfileForTestingNoAnalyticTrFn();
  ColorSpace icc_space = icc_profile.GetColorSpace();
  ColorSpace xyzd50 = ColorSpace::CreateXYZD50();

  ColorTransform::TriStim input_value(127.f / 255, 187.f / 255, 157.f / 255);
  ColorTransform::TriStim transformed_value = input_value;
  ColorTransform::TriStim expected_transformed_value(
      0.34090986847877502f, 0.42633286118507385f, 0.3408740758895874f);

  // Two steps should be needed, transfer fn and matrix.
  std::unique_ptr<ColorTransform> icc_to_xyzd50(
      ColorTransform::NewColorTransform(
          icc_space, xyzd50, ColorTransform::Intent::INTENT_ABSOLUTE));
  EXPECT_EQ(icc_to_xyzd50->NumberOfStepsForTesting(), 2u);
  icc_to_xyzd50->Transform(&transformed_value, 1);
  EXPECT_NEAR(transformed_value.x(), expected_transformed_value.x(), kEpsilon);
  EXPECT_NEAR(transformed_value.y(), expected_transformed_value.y(), kEpsilon);
  EXPECT_NEAR(transformed_value.z(), expected_transformed_value.z(), kEpsilon);

  // Two steps should be needed, matrix and transfer fn.
  std::unique_ptr<ColorTransform> xyzd50_to_icc(
      ColorTransform::NewColorTransform(
          xyzd50, icc_space, ColorTransform::Intent::INTENT_ABSOLUTE));
  EXPECT_EQ(xyzd50_to_icc->NumberOfStepsForTesting(), 2u);
  xyzd50_to_icc->Transform(&transformed_value, 1);
  EXPECT_NEAR(input_value.x(), transformed_value.x(), kEpsilon);
  EXPECT_NEAR(input_value.y(), transformed_value.y(), kEpsilon);
  EXPECT_NEAR(input_value.z(), transformed_value.z(), kEpsilon);
}

TEST(SimpleColorSpace, ICCProfileOnlyColorSpin) {
  const float kEpsilon = 3.0f / 255.f;
  ICCProfile icc_profile = ICCProfileForTestingNoAnalyticTrFn();
  ColorSpace icc_space = icc_profile.GetColorSpace();
  ColorSpace colorspin = ICCProfileForTestingColorSpin().GetColorSpace();

  ColorTransform::TriStim input_value(0.25f, 0.5f, 0.75f);
  ColorTransform::TriStim transformed_value = input_value;
  ColorTransform::TriStim expected_transformed_value(
      0.49694931507110596f, 0.74937951564788818f, 0.31359460949897766f);

  // Three steps will be needed.
  std::unique_ptr<ColorTransform> icc_to_colorspin(
      ColorTransform::NewColorTransform(
          icc_space, colorspin, ColorTransform::Intent::INTENT_PERCEPTUAL));
  EXPECT_EQ(icc_to_colorspin->NumberOfStepsForTesting(), 3u);
  icc_to_colorspin->Transform(&transformed_value, 1);
  EXPECT_NEAR(transformed_value.x(), expected_transformed_value.x(), kEpsilon);
  EXPECT_NEAR(transformed_value.y(), expected_transformed_value.y(), kEpsilon);
  EXPECT_NEAR(transformed_value.z(), expected_transformed_value.z(), kEpsilon);

  transformed_value = expected_transformed_value;
  std::unique_ptr<ColorTransform> colorspin_to_icc(
      ColorTransform::NewColorTransform(
          colorspin, icc_space, ColorTransform::Intent::INTENT_PERCEPTUAL));
  EXPECT_EQ(colorspin_to_icc->NumberOfStepsForTesting(), 3u);
  transformed_value = expected_transformed_value;
  colorspin_to_icc->Transform(&transformed_value, 1);
  EXPECT_NEAR(input_value.x(), transformed_value.x(), kEpsilon);
  EXPECT_NEAR(input_value.y(), transformed_value.y(), kEpsilon);
  EXPECT_NEAR(input_value.z(), transformed_value.z(), kEpsilon);
}

TEST(SimpleColorSpace, GetColorSpace) {
  ICCProfile srgb_icc = ICCProfileForTestingSRGB();
  ColorSpace sRGB = srgb_icc.GetColorSpace();
  ColorSpace sRGB2 = sRGB;

  std::unique_ptr<ColorTransform> t(ColorTransform::NewColorTransform(
      sRGB, sRGB2, ColorTransform::Intent::INTENT_ABSOLUTE));

  ColorTransform::TriStim tmp(1.0f, 1.0f, 1.0f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 1.0f, kEpsilon);
  EXPECT_NEAR(tmp.y(), 1.0f, kEpsilon);
  EXPECT_NEAR(tmp.z(), 1.0f, kEpsilon);

  tmp = ColorTransform::TriStim(1.0f, 0.0f, 0.0f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 1.0f, kEpsilon);
  EXPECT_NEAR(tmp.y(), 0.0f, kEpsilon);
  EXPECT_NEAR(tmp.z(), 0.0f, kEpsilon);

  tmp = ColorTransform::TriStim(0.0f, 1.0f, 0.0f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 0.0f, kEpsilon);
  EXPECT_NEAR(tmp.y(), 1.0f, kEpsilon);
  EXPECT_NEAR(tmp.z(), 0.0f, kEpsilon);

  tmp = ColorTransform::TriStim(0.0f, 0.0f, 1.0f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 0.0f, kEpsilon);
  EXPECT_NEAR(tmp.y(), 0.0f, kEpsilon);
  EXPECT_NEAR(tmp.z(), 1.0f, kEpsilon);
}

TEST(SimpleColorSpace, Scale) {
  ColorSpace srgb = ColorSpace::CreateSRGB();
  ColorSpace srgb_scaled = srgb.GetScaledColorSpace(2.0f);
  std::unique_ptr<ColorTransform> t(ColorTransform::NewColorTransform(
      srgb, srgb_scaled, ColorTransform::Intent::INTENT_PERCEPTUAL));

  ColorTransform::TriStim tmp(1.0f, 1.0f, 1.0f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 0.735356983052449f, kEpsilon);
  EXPECT_NEAR(tmp.y(), 0.735356983052449f, kEpsilon);
  EXPECT_NEAR(tmp.z(), 0.735356983052449f, kEpsilon);
}

TEST(SimpleColorSpace, ToUndefined) {
  ColorSpace null;
  ColorSpace nonnull = gfx::ColorSpace::CreateSRGB();
  // Video should have 1 step: YUV to RGB.
  // Anything else should have 0 steps.
  ColorSpace video = gfx::ColorSpace::CreateREC709();
  std::unique_ptr<ColorTransform> video_to_null(
      ColorTransform::NewColorTransform(
          video, null, ColorTransform::Intent::INTENT_PERCEPTUAL));
  EXPECT_EQ(video_to_null->NumberOfStepsForTesting(), 1u);

  // Test with an ICC profile that can't be represented as matrix+transfer.
  ColorSpace luttrcicc = ICCProfileForTestingNoAnalyticTrFn().GetColorSpace();
  std::unique_ptr<ColorTransform> luttrcicc_to_null(
      ColorTransform::NewColorTransform(
          luttrcicc, null, ColorTransform::Intent::INTENT_PERCEPTUAL));
  EXPECT_EQ(luttrcicc_to_null->NumberOfStepsForTesting(), 0u);
  std::unique_ptr<ColorTransform> luttrcicc_to_nonnull(
      ColorTransform::NewColorTransform(
          luttrcicc, nonnull, ColorTransform::Intent::INTENT_PERCEPTUAL));
  EXPECT_GT(luttrcicc_to_nonnull->NumberOfStepsForTesting(), 0u);

  // Test with an ICC profile that can.
  ColorSpace adobeicc = ICCProfileForTestingAdobeRGB().GetColorSpace();
  std::unique_ptr<ColorTransform> adobeicc_to_null(
      ColorTransform::NewColorTransform(
          adobeicc, null, ColorTransform::Intent::INTENT_PERCEPTUAL));
  EXPECT_EQ(adobeicc_to_null->NumberOfStepsForTesting(), 0u);
  std::unique_ptr<ColorTransform> adobeicc_to_nonnull(
      ColorTransform::NewColorTransform(
          adobeicc, nonnull, ColorTransform::Intent::INTENT_PERCEPTUAL));
  EXPECT_GT(adobeicc_to_nonnull->NumberOfStepsForTesting(), 0u);

  // And with something analytic.
  ColorSpace srgb = gfx::ColorSpace::CreateXYZD50();
  std::unique_ptr<ColorTransform> srgb_to_null(
      ColorTransform::NewColorTransform(
          srgb, null, ColorTransform::Intent::INTENT_PERCEPTUAL));
  EXPECT_EQ(srgb_to_null->NumberOfStepsForTesting(), 0u);
  std::unique_ptr<ColorTransform> srgb_to_nonnull(
      ColorTransform::NewColorTransform(
          srgb, nonnull, ColorTransform::Intent::INTENT_PERCEPTUAL));
  EXPECT_GT(srgb_to_nonnull->NumberOfStepsForTesting(), 0u);
}

TEST(SimpleColorSpace, DefaultToSRGB) {
  // The default value should do no transformation, regardless of destination.
  ColorSpace unknown;
  std::unique_ptr<ColorTransform> t1(ColorTransform::NewColorTransform(
      unknown, ColorSpace::CreateSRGB(),
      ColorTransform::Intent::INTENT_PERCEPTUAL));
  EXPECT_EQ(t1->NumberOfStepsForTesting(), 0u);
  std::unique_ptr<ColorTransform> t2(ColorTransform::NewColorTransform(
      unknown, ColorSpace::CreateXYZD50(),
      ColorTransform::Intent::INTENT_PERCEPTUAL));
  EXPECT_EQ(t2->NumberOfStepsForTesting(), 0u);
}

// This tests to make sure that we don't emit "pow" parts of a
// transfer function unless necessary.
TEST(SimpleColorSpace, ShaderSourceTrFnOptimizations) {
  skcms_Matrix3x3 primaries;
  gfx::ColorSpace::CreateSRGB().GetPrimaryMatrix(&primaries);

  skcms_TransferFunction fn_no_pow = {
      1.f, 2.f, 0.f, 1.f, 0.f, 0.f, 0.f,
  };
  skcms_TransferFunction fn_yes_pow = {
      2.f, 2.f, 0.f, 1.f, 0.f, 0.f, 0.f,
  };
  gfx::ColorSpace src;
  gfx::ColorSpace dst = gfx::ColorSpace::CreateXYZD50();
  std::string shader_string;

  src = gfx::ColorSpace::CreateCustom(primaries, fn_no_pow);
  shader_string = ColorTransform::NewColorTransform(
                      src, dst, ColorTransform::Intent::INTENT_PERCEPTUAL)
                      ->GetShaderSource();
  EXPECT_EQ(shader_string.find("pow("), std::string::npos);

  src = gfx::ColorSpace::CreateCustom(primaries, fn_yes_pow);
  shader_string = ColorTransform::NewColorTransform(
                      src, dst, ColorTransform::Intent::INTENT_PERCEPTUAL)
                      ->GetShaderSource();
  EXPECT_NE(shader_string.find("pow("), std::string::npos);
}

// Note: This is not actually "testing" anything -- the goal of this test is to
// to make reviewing shader code simpler by giving an example of the resulting
// shader source. This should be updated whenever shader generation is updated.
// This test produces slightly different results on Android.
TEST(SimpleColorSpace, SampleShaderSource) {
  ColorSpace bt709 = ColorSpace::CreateREC709();
  ColorSpace output(ColorSpace::PrimaryID::BT2020,
                    ColorSpace::TransferID::GAMMA28);
  std::string source =
      ColorTransform::NewColorTransform(
          bt709, output, ColorTransform::Intent::INTENT_PERCEPTUAL)
          ->GetShaderSource();
  std::string expected =
      "float TransferFn1(float v) {\n"
      "  if (v < 4.04499359e-02)\n"
      "    v = 7.73993805e-02 * v;\n"
      "  else\n"
      "    v = pow(9.47867334e-01 * v + 5.21326549e-02, 2.40000010e+00);\n"
      "  return v;\n"
      "}\n"
      "float TransferFn3(float v) {\n"
      "  if (v < 0.00000000e+00)\n"
      "    v = 0.00000000e+00 * v;\n"
      "  else\n"
      "    v = pow(v, 3.57142866e-01);\n"
      "  return v;\n"
      "}\n"
      "vec3 DoColorConversion(vec3 color) {\n"
      "  color = mat3(1.16438353e+00, 1.16438353e+00, 1.16438353e+00,\n"
      "               -2.28029018e-09, -2.13248596e-01, 2.11240172e+00,\n"
      "               1.79274118e+00, -5.32909274e-01, -5.96049432e-10) "
      "* color;\n"
      "  color += vec3(-9.69429970e-01, 3.00019622e-01, -1.12926030e+00);\n"
      "  color.r = TransferFn1(color.r);\n"
      "  color.g = TransferFn1(color.g);\n"
      "  color.b = TransferFn1(color.b);\n"
      "  color = mat3(6.27404153e-01, 6.90974146e-02, 1.63914431e-02,\n"
      "               3.29283088e-01, 9.19540644e-01, 8.80132765e-02,\n"
      "               4.33131084e-02, 1.13623096e-02, 8.95595253e-01) "
      "* color;\n"
      "  color.r = TransferFn3(color.r);\n"
      "  color.g = TransferFn3(color.g);\n"
      "  color.b = TransferFn3(color.b);\n"
      "  return color;\n"
      "}\n";
  EXPECT_EQ(source, expected);
}

// Checks that the generated SkSL fragment shaders can be parsed by
// SkSL::Compiler.
TEST(SimpleColorSpace, CanParseSkShaderSource) {
  std::vector<ColorSpace> common_color_spaces = {
      ColorSpace::CreateSRGB(),         ColorSpace::CreateDisplayP3D65(),
      ColorSpace::CreateExtendedSRGB(), ColorSpace::CreateSCRGBLinear(),
      ColorSpace::CreateJpeg(),         ColorSpace::CreateREC601(),
      ColorSpace::CreateREC709()};
  for (const auto& src : common_color_spaces) {
    for (const auto& dst : common_color_spaces) {
      auto transform = ColorTransform::NewColorTransform(
          src, dst, ColorTransform::Intent::INTENT_PERCEPTUAL);
      if (!transform->CanGetShaderSource())
        continue;

      std::string source = "void main(inout half4 color) {" +
                           transform->GetSkShaderSource() + "}";
      SkSL::Program::Settings settings;
      SkSL::Compiler compiler;
      auto program = compiler.convertProgram(
          SkSL::Program::kPipelineStage_Kind,
          SkSL::String(source.c_str(), source.length()), settings);
      EXPECT_NE(nullptr, program.get());
      EXPECT_EQ(0, compiler.errorCount()) << compiler.errorText();
    }
  }
}

class TransferTest : public testing::TestWithParam<ColorSpace::TransferID> {};

TEST_P(TransferTest, basicTest) {
  gfx::ColorSpace space_with_transfer(ColorSpace::PrimaryID::BT709, GetParam(),
                                      ColorSpace::MatrixID::RGB,
                                      ColorSpace::RangeID::FULL);
  gfx::ColorSpace space_linear(
      ColorSpace::PrimaryID::BT709, ColorSpace::TransferID::LINEAR,
      ColorSpace::MatrixID::RGB, ColorSpace::RangeID::FULL);

  std::unique_ptr<ColorTransform> to_linear(ColorTransform::NewColorTransform(
      space_with_transfer, space_linear,
      ColorTransform::Intent::INTENT_ABSOLUTE));

  std::unique_ptr<ColorTransform> from_linear(ColorTransform::NewColorTransform(
      space_linear, space_with_transfer,
      ColorTransform::Intent::INTENT_ABSOLUTE));

  // The transforms will have 1 or 0 steps (0 for linear).
  size_t expected_steps = 1u;
  if (GetParam() == ColorSpace::TransferID::LINEAR)
    expected_steps = 0u;
  EXPECT_EQ(to_linear->NumberOfStepsForTesting(), expected_steps);
  EXPECT_EQ(from_linear->NumberOfStepsForTesting(), expected_steps);

  for (float x = 0.0f; x <= 1.0f; x += 1.0f / 128.0f) {
    ColorTransform::TriStim tristim(x, x, x);
    to_linear->Transform(&tristim, 1);
    from_linear->Transform(&tristim, 1);
    EXPECT_NEAR(x, tristim.x(), 0.001f);
  }
}

INSTANTIATE_TEST_SUITE_P(ColorSpace,
                         TransferTest,
                         testing::ValuesIn(simple_transfers));

class NonInvertibleTransferTest
    : public testing::TestWithParam<ColorSpace::TransferID> {};

TEST_P(NonInvertibleTransferTest, basicTest) {
  gfx::ColorSpace space_with_transfer(ColorSpace::PrimaryID::BT709, GetParam(),
                                      ColorSpace::MatrixID::RGB,
                                      ColorSpace::RangeID::FULL);
  gfx::ColorSpace space_linear(
      ColorSpace::PrimaryID::BT709, ColorSpace::TransferID::LINEAR,
      ColorSpace::MatrixID::RGB, ColorSpace::RangeID::FULL);

  std::unique_ptr<ColorTransform> to_linear(ColorTransform::NewColorTransform(
      space_with_transfer, space_linear,
      ColorTransform::Intent::INTENT_ABSOLUTE));

  std::unique_ptr<ColorTransform> from_linear(ColorTransform::NewColorTransform(
      space_linear, space_with_transfer,
      ColorTransform::Intent::INTENT_ABSOLUTE));

  // These transforms should not crash when created or applied.
  float x = 0.5;
  ColorTransform::TriStim tristim(x, x, x);
  to_linear->Transform(&tristim, 1);
  from_linear->Transform(&tristim, 1);
}

INSTANTIATE_TEST_SUITE_P(ColorSpace,
                         NonInvertibleTransferTest,
                         testing::ValuesIn(noninvertible_transfers));

class ExtendedTransferTest
    : public testing::TestWithParam<ColorSpace::TransferID> {};

TEST_P(ExtendedTransferTest, extendedTest) {
  gfx::ColorSpace space_with_transfer(ColorSpace::PrimaryID::BT709, GetParam(),
                                      ColorSpace::MatrixID::RGB,
                                      ColorSpace::RangeID::FULL);
  gfx::ColorSpace space_linear(
      ColorSpace::PrimaryID::BT709, ColorSpace::TransferID::LINEAR,
      ColorSpace::MatrixID::RGB, ColorSpace::RangeID::FULL);

  std::unique_ptr<ColorTransform> to_linear(ColorTransform::NewColorTransform(
      space_with_transfer, space_linear,
      ColorTransform::Intent::INTENT_ABSOLUTE));

  std::unique_ptr<ColorTransform> from_linear(ColorTransform::NewColorTransform(
      space_linear, space_with_transfer,
      ColorTransform::Intent::INTENT_ABSOLUTE));

  for (float x = -2.0f; x <= 2.0f; x += 1.0f / 32.0f) {
    ColorTransform::TriStim tristim(x, x, x);
    to_linear->Transform(&tristim, 1);
    from_linear->Transform(&tristim, 1);
    EXPECT_NEAR(x, tristim.x(), 0.001f);
  }
}

INSTANTIATE_TEST_SUITE_P(ColorSpace,
                         ExtendedTransferTest,
                         testing::ValuesIn(extended_transfers));

typedef std::tuple<ColorSpace::PrimaryID,
                   ColorSpace::TransferID,
                   ColorSpace::MatrixID,
                   ColorSpace::RangeID,
                   ColorTransform::Intent>
    ColorSpaceTestData;

class ColorSpaceTest : public testing::TestWithParam<ColorSpaceTestData> {
 public:
  ColorSpaceTest()
      : color_space_(std::get<0>(GetParam()),
                     std::get<1>(GetParam()),
                     std::get<2>(GetParam()),
                     std::get<3>(GetParam())),
        intent_(std::get<4>(GetParam())) {}

 protected:
  ColorSpace color_space_;
  ColorTransform::Intent intent_;
};

TEST_P(ColorSpaceTest, testNullTransform) {
  std::unique_ptr<ColorTransform> t(
      ColorTransform::NewColorTransform(color_space_, color_space_, intent_));
  ColorTransform::TriStim tristim(0.4f, 0.5f, 0.6f);
  t->Transform(&tristim, 1);
  EXPECT_NEAR(tristim.x(), 0.4f, 0.001f);
  EXPECT_NEAR(tristim.y(), 0.5f, 0.001f);
  EXPECT_NEAR(tristim.z(), 0.6f, 0.001f);
}

TEST_P(ColorSpaceTest, toXYZandBack) {
  std::unique_ptr<ColorTransform> t1(ColorTransform::NewColorTransform(
      color_space_, ColorSpace::CreateXYZD50(), intent_));
  std::unique_ptr<ColorTransform> t2(ColorTransform::NewColorTransform(
      ColorSpace::CreateXYZD50(), color_space_, intent_));
  ColorTransform::TriStim tristim(0.4f, 0.5f, 0.6f);
  t1->Transform(&tristim, 1);
  t2->Transform(&tristim, 1);
  EXPECT_NEAR(tristim.x(), 0.4f, 0.001f);
  EXPECT_NEAR(tristim.y(), 0.5f, 0.001f);
  EXPECT_NEAR(tristim.z(), 0.6f, 0.001f);
}

INSTANTIATE_TEST_SUITE_P(
    A,
    ColorSpaceTest,
    testing::Combine(testing::ValuesIn(all_primaries),
                     testing::ValuesIn(simple_transfers),
                     testing::Values(ColorSpace::MatrixID::BT709),
                     testing::Values(ColorSpace::RangeID::LIMITED),
                     testing::ValuesIn(intents)));

INSTANTIATE_TEST_SUITE_P(
    B,
    ColorSpaceTest,
    testing::Combine(testing::Values(ColorSpace::PrimaryID::BT709),
                     testing::ValuesIn(simple_transfers),
                     testing::ValuesIn(all_matrices),
                     testing::ValuesIn(all_ranges),
                     testing::ValuesIn(intents)));

INSTANTIATE_TEST_SUITE_P(
    C,
    ColorSpaceTest,
    testing::Combine(testing::ValuesIn(all_primaries),
                     testing::Values(ColorSpace::TransferID::BT709),
                     testing::ValuesIn(all_matrices),
                     testing::ValuesIn(all_ranges),
                     testing::ValuesIn(intents)));
}  // namespace gfx
