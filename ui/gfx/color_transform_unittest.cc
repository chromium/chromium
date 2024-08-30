// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <tuple>
#include <vector>

#include "base/logging.h"
#include "base/test/scoped_feature_list.h"
#include "skia/ext/skcolorspace_trfn.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/effects/SkRuntimeEffect.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_transform.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/skia_color_space_util.h"
#include "ui/gfx/test/icc_profiles.h"

namespace gfx {

// Allowed math error.
constexpr float kMathEpsilon = 0.001f;
constexpr float kMathLargeEpsilon = 0.025f;

// Internal functions, exposted for testing.
GFX_EXPORT Transform GetTransferMatrix(ColorSpace::MatrixID id);

ColorSpace::PrimaryID all_primaries[] = {
    ColorSpace::PrimaryID::BT709,        ColorSpace::PrimaryID::BT470M,
    ColorSpace::PrimaryID::BT470BG,      ColorSpace::PrimaryID::SMPTE170M,
    ColorSpace::PrimaryID::SMPTE240M,    ColorSpace::PrimaryID::FILM,
    ColorSpace::PrimaryID::BT2020,       ColorSpace::PrimaryID::SMPTEST428_1,
    ColorSpace::PrimaryID::SMPTEST431_2, ColorSpace::PrimaryID::P3,
};

ColorSpace::TransferID simple_transfers[] = {
    ColorSpace::TransferID::BT709,      ColorSpace::TransferID::GAMMA22,
    ColorSpace::TransferID::GAMMA28,    ColorSpace::TransferID::SMPTE170M,
    ColorSpace::TransferID::SMPTE240M,  ColorSpace::TransferID::SMPTEST428_1,
    ColorSpace::TransferID::LINEAR,     ColorSpace::TransferID::LOG,
    ColorSpace::TransferID::LOG_SQRT,   ColorSpace::TransferID::IEC61966_2_4,
    ColorSpace::TransferID::BT1361_ECG, ColorSpace::TransferID::SRGB,
    ColorSpace::TransferID::BT2020_10,  ColorSpace::TransferID::BT2020_12,
    ColorSpace::TransferID::SRGB_HDR,
};

ColorSpace::TransferID extended_transfers[] = {
    ColorSpace::TransferID::LINEAR_HDR,
    ColorSpace::TransferID::SRGB_HDR,
};

ColorSpace::MatrixID all_matrices[] = {
    ColorSpace::MatrixID::RGB,       ColorSpace::MatrixID::BT709,
    ColorSpace::MatrixID::FCC,       ColorSpace::MatrixID::BT470BG,
    ColorSpace::MatrixID::SMPTE170M, ColorSpace::MatrixID::SMPTE240M,
    ColorSpace::MatrixID::YCOCG,     ColorSpace::MatrixID::BT2020_NCL,
    ColorSpace::MatrixID::YDZDX,
};

ColorSpace::RangeID all_ranges[] = {ColorSpace::RangeID::FULL,
                                    ColorSpace::RangeID::LIMITED,
                                    ColorSpace::RangeID::DERIVED};

bool optimizations[] = {true, false};

TEST(SimpleColorSpace, BT709toSRGB) {
  ColorSpace bt709 = ColorSpace::CreateREC709();
  ColorSpace sRGB = ColorSpace::CreateSRGB();
  std::unique_ptr<ColorTransform> t(
      ColorTransform::NewColorTransform(bt709, sRGB));

  ColorTransform::TriStim tmp(16.0f / 255.0f, 0.5f, 0.5f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 0.0f, kMathEpsilon);
  EXPECT_NEAR(tmp.y(), 0.0f, kMathEpsilon);
  EXPECT_NEAR(tmp.z(), 0.0f, kMathEpsilon);

  tmp = ColorTransform::TriStim(235.0f / 255.0f, 0.5f, 0.5f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 1.0f, kMathEpsilon);
  EXPECT_NEAR(tmp.y(), 1.0f, kMathEpsilon);
  EXPECT_NEAR(tmp.z(), 1.0f, kMathEpsilon);

  // Test a blue color
  tmp = ColorTransform::TriStim(128.0f / 255.0f, 240.0f / 255.0f, 0.5f);
  t->Transform(&tmp, 1);
  EXPECT_GT(tmp.z(), tmp.x());
  EXPECT_GT(tmp.z(), tmp.y());
}

TEST(SimpleColorSpace, YCOCGLimitedToSRGB) {
  ColorSpace ycocg(ColorSpace::PrimaryID::BT709, ColorSpace::TransferID::SRGB,
                   ColorSpace::MatrixID::YCOCG, ColorSpace::RangeID::LIMITED);
  ColorSpace sRGB = ColorSpace::CreateSRGB();
  std::unique_ptr<ColorTransform> t(
      ColorTransform::NewColorTransform(ycocg, sRGB));

  ColorTransform::TriStim tmp(16.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 0.0f, kMathEpsilon);
  EXPECT_NEAR(tmp.y(), 0.0f, kMathEpsilon);
  EXPECT_NEAR(tmp.z(), 0.0f, kMathEpsilon);

  tmp = ColorTransform::TriStim(235.0f / 255.0f, 128.0f / 255.0f,
                                128.0f / 255.0f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 1.0f, kMathEpsilon);
  EXPECT_NEAR(tmp.y(), 1.0f, kMathEpsilon);
  EXPECT_NEAR(tmp.z(), 1.0f, kMathEpsilon);

  // Test a blue color
  // Use the equations for MatrixCoefficients 8 and VideoFullRangeFlag 0 in
  // ITU-T H.273:
  // Equations 11-13: E'_R = 0.0, E'_G = 0.0, E'_B = 1.0
  // Equations 20-22: R = 16, G = 16, B = 219 + 16 = 235
  // Equations 44-46:
  //   Y = Round(0.5 * 16 + 0.25 * (16 + 235)) = Round(70.75) = 71
  //   Cb = Round(0.5 * 16 - 0.25 * (16 + 235)) + 128 = Round(-54.75) + 128 = 73
  //   Cr = Round(0.5 * (16 - 235)) + 128 = Round(-109.5) + 128 = 18
  // In this test we omit the Round() calls to avoid rounding errors.
  //   Y = 0.5 * 16 + 0.25 * (16 + 235) = 70.75
  //   Cb = 0.5 * 16 - 0.25 * (16 + 235) + 128 = -54.75 + 128 = 73.25
  //   Cr = 0.5 * (16 - 235) + 128 = -109.5 + 128 = 18.5
  tmp =
      ColorTransform::TriStim(70.75f / 255.0f, 73.25f / 255.0f, 18.5f / 255.0f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 0.0f, kMathEpsilon);
  EXPECT_NEAR(tmp.y(), 0.0f, kMathEpsilon);
  EXPECT_NEAR(tmp.z(), 1.0f, kMathEpsilon);
}

TEST(SimpleColorSpace, TransferFnCancel) {
  ColorSpace::PrimaryID primary = ColorSpace::PrimaryID::BT709;
  ColorSpace::MatrixID matrix = ColorSpace::MatrixID::RGB;
  ColorSpace::RangeID range = ColorSpace::RangeID::FULL;

  // BT709 has a gamma of 2.2222 (with some adjustments)
  ColorSpace bt709(primary, ColorSpace::TransferID::BT709, matrix, range);

  // IEC61966_2_1 has the sRGB gamma of 2.4 (with some adjustments)
  ColorSpace srgb(primary, ColorSpace::TransferID::SRGB, matrix, range);

  // gamma28 is a simple exponential
  ColorSpace gamma28(primary, ColorSpace::TransferID::GAMMA28, matrix, range);

  // gamma24 is a simple exponential
  ColorSpace gamma24(primary, ColorSpace::TransferID::GAMMA24, matrix, range);

  // BT709 source is common for video and sRGB destination is common for
  // monitors. The two transfer functions are very close, and should cancel
  // out (so the transfer between them should be the identity). This particular
  // case is important for power reasons.
  std::unique_ptr<ColorTransform> bt709_to_srgb(
      ColorTransform::NewColorTransform(bt709, srgb));
  EXPECT_EQ(bt709_to_srgb->NumberOfStepsForTesting(), 0u);

  // Gamma 2.8 isn't even close to BT709 and won't cancel out (so we will have
  // two steps in the transform -- to-linear and from-linear).
  std::unique_ptr<ColorTransform> bt709_to_gamma28(
      ColorTransform::NewColorTransform(bt709, gamma28));
  EXPECT_EQ(bt709_to_gamma28->NumberOfStepsForTesting(), 2u);

  // Gamma 2.4 is closer to BT709, but not close enough to actually cancel out.
  std::unique_ptr<ColorTransform> bt709_to_gamma24(
      ColorTransform::NewColorTransform(bt709, gamma24));
  EXPECT_EQ(bt709_to_gamma24->NumberOfStepsForTesting(), 2u);

  // Rec 601 YUV to RGB conversion should have a single step.
  gfx::ColorSpace rec601 = gfx::ColorSpace::CreateREC601();
  std::unique_ptr<ColorTransform> rec601_yuv_to_rgb(
      ColorTransform::NewColorTransform(rec601, rec601.GetAsFullRangeRGB()));
  EXPECT_EQ(rec601_yuv_to_rgb->NumberOfStepsForTesting(), 1u);
}

TEST(SimpleColorSpace, SRGBFromICCAndNotICC) {
  float kPixelEpsilon = kMathEpsilon;
  ColorTransform::TriStim value_fromicc;
  ColorTransform::TriStim value_default;

  ICCProfile srgb_icc_profile = ICCProfileForTestingSRGB();
  ColorSpace srgb_fromicc = srgb_icc_profile.GetColorSpace();
  ColorSpace srgb_default = gfx::ColorSpace::CreateSRGB();
  ColorSpace xyzd50 = gfx::ColorSpace::CreateXYZD50();

  value_fromicc = value_default = ColorTransform::TriStim(0.1f, 0.5f, 0.9f);

  std::unique_ptr<ColorTransform> toxyzd50_fromicc(
      ColorTransform::NewColorTransform(srgb_fromicc, xyzd50));
  // This will be converted to a transfer function and then linear transform.
  EXPECT_EQ(toxyzd50_fromicc->NumberOfStepsForTesting(), 2u);
  toxyzd50_fromicc->Transform(&value_fromicc, 1);

  std::unique_ptr<ColorTransform> toxyzd50_default(
      ColorTransform::NewColorTransform(srgb_default, xyzd50));
  // This will have a transfer function and then linear transform.
  EXPECT_EQ(toxyzd50_default->NumberOfStepsForTesting(), 2u);
  toxyzd50_default->Transform(&value_default, 1);

  EXPECT_NEAR(value_fromicc.x(), value_default.x(), kPixelEpsilon);
  EXPECT_NEAR(value_fromicc.y(), value_default.y(), kPixelEpsilon);
  EXPECT_NEAR(value_fromicc.z(), value_default.z(), kPixelEpsilon);

  value_fromicc = value_default = ColorTransform::TriStim(0.1f, 0.5f, 0.9f);

  std::unique_ptr<ColorTransform> fromxyzd50_fromicc(
      ColorTransform::NewColorTransform(xyzd50, srgb_fromicc));
  fromxyzd50_fromicc->Transform(&value_fromicc, 1);

  std::unique_ptr<ColorTransform> fromxyzd50_default(
      ColorTransform::NewColorTransform(xyzd50, srgb_default));
  fromxyzd50_default->Transform(&value_default, 1);

  EXPECT_NEAR(value_fromicc.x(), value_default.x(), kPixelEpsilon);
  EXPECT_NEAR(value_fromicc.y(), value_default.y(), kPixelEpsilon);
  EXPECT_NEAR(value_fromicc.z(), value_default.z(), kPixelEpsilon);
}

TEST(SimpleColorSpace, BT709toSRGBICC) {
  ICCProfile srgb_icc = ICCProfileForTestingSRGB();
  ColorSpace bt709 = ColorSpace::CreateREC709();
  ColorSpace sRGB = srgb_icc.GetColorSpace();
  std::unique_ptr<ColorTransform> t(
      ColorTransform::NewColorTransform(bt709, sRGB));

  ColorTransform::TriStim tmp(16.0f / 255.0f, 0.5f, 0.5f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 0.0f, kMathEpsilon);
  EXPECT_NEAR(tmp.y(), 0.0f, kMathEpsilon);
  EXPECT_NEAR(tmp.z(), 0.0f, kMathEpsilon);

  tmp = ColorTransform::TriStim(235.0f / 255.0f, 0.5f, 0.5f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 1.0f, kMathEpsilon);
  EXPECT_NEAR(tmp.y(), 1.0f, kMathEpsilon);
  EXPECT_NEAR(tmp.z(), 1.0f, kMathEpsilon);

  // Test a blue color
  tmp = ColorTransform::TriStim(128.0f / 255.0f, 240.0f / 255.0f, 0.5f);
  t->Transform(&tmp, 1);
  EXPECT_GT(tmp.z(), tmp.x());
  EXPECT_GT(tmp.z(), tmp.y());
}

TEST(SimpleColorSpace, ICCProfileOnlyXYZ) {
  const float kPixelEpsilon = 2.5f / 255.f;
  ICCProfile icc_profile = ICCProfileForTestingNoAnalyticTrFn();
  ColorSpace icc_space = icc_profile.GetColorSpace();
  ColorSpace xyzd50 = ColorSpace::CreateXYZD50();

  ColorTransform::TriStim input_value(127.f / 255, 187.f / 255, 157.f / 255);
  ColorTransform::TriStim transformed_value = input_value;
  ColorTransform::TriStim expected_transformed_value(
      0.34090986847877502f, 0.42633286118507385f, 0.3408740758895874f);

  // Two steps should be needed, transfer fn and matrix.
  std::unique_ptr<ColorTransform> icc_to_xyzd50(
      ColorTransform::NewColorTransform(icc_space, xyzd50));
  EXPECT_EQ(icc_to_xyzd50->NumberOfStepsForTesting(), 2u);
  icc_to_xyzd50->Transform(&transformed_value, 1);
  EXPECT_NEAR(transformed_value.x(), expected_transformed_value.x(),
              kPixelEpsilon);
  EXPECT_NEAR(transformed_value.y(), expected_transformed_value.y(),
              kPixelEpsilon);
  EXPECT_NEAR(transformed_value.z(), expected_transformed_value.z(),
              kPixelEpsilon);

  // Two steps should be needed, matrix and transfer fn.
  std::unique_ptr<ColorTransform> xyzd50_to_icc(
      ColorTransform::NewColorTransform(xyzd50, icc_space));
  EXPECT_EQ(xyzd50_to_icc->NumberOfStepsForTesting(), 2u);
  xyzd50_to_icc->Transform(&transformed_value, 1);
  EXPECT_NEAR(input_value.x(), transformed_value.x(), kPixelEpsilon);
  EXPECT_NEAR(input_value.y(), transformed_value.y(), kPixelEpsilon);
  EXPECT_NEAR(input_value.z(), transformed_value.z(), kPixelEpsilon);
}

TEST(SimpleColorSpace, ICCProfileOnlyColorSpin) {
  const float kPixelEpsilon = 3.0f / 255.f;
  ICCProfile icc_profile = ICCProfileForTestingNoAnalyticTrFn();
  ColorSpace icc_space = icc_profile.GetColorSpace();
  ColorSpace colorspin = ICCProfileForTestingColorSpin().GetColorSpace();

  ColorTransform::TriStim input_value(0.25f, 0.5f, 0.75f);
  ColorTransform::TriStim transformed_value = input_value;
  ColorTransform::TriStim expected_transformed_value(
      0.49694931507110596f, 0.74937951564788818f, 0.31359460949897766f);

  // Three steps will be needed.
  std::unique_ptr<ColorTransform> icc_to_colorspin(
      ColorTransform::NewColorTransform(icc_space, colorspin));
  EXPECT_EQ(icc_to_colorspin->NumberOfStepsForTesting(), 3u);
  icc_to_colorspin->Transform(&transformed_value, 1);
  EXPECT_NEAR(transformed_value.x(), expected_transformed_value.x(),
              kPixelEpsilon);
  EXPECT_NEAR(transformed_value.y(), expected_transformed_value.y(),
              kPixelEpsilon);
  EXPECT_NEAR(transformed_value.z(), expected_transformed_value.z(),
              kPixelEpsilon);

  transformed_value = expected_transformed_value;
  std::unique_ptr<ColorTransform> colorspin_to_icc(
      ColorTransform::NewColorTransform(colorspin, icc_space));
  EXPECT_EQ(colorspin_to_icc->NumberOfStepsForTesting(), 3u);
  transformed_value = expected_transformed_value;
  colorspin_to_icc->Transform(&transformed_value, 1);
  EXPECT_NEAR(input_value.x(), transformed_value.x(), kPixelEpsilon);
  EXPECT_NEAR(input_value.y(), transformed_value.y(), kPixelEpsilon);
  EXPECT_NEAR(input_value.z(), transformed_value.z(), kPixelEpsilon);
}

TEST(SimpleColorSpace, GetColorSpace) {
  const float kPixelEpsilon = 1.5f / 255.f;
  ICCProfile srgb_icc = ICCProfileForTestingSRGB();
  ColorSpace sRGB = srgb_icc.GetColorSpace();
  ColorSpace sRGB2 = sRGB;

  std::unique_ptr<ColorTransform> t(
      ColorTransform::NewColorTransform(sRGB, sRGB2));

  ColorTransform::TriStim tmp(1.0f, 1.0f, 1.0f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 1.0f, kPixelEpsilon);
  EXPECT_NEAR(tmp.y(), 1.0f, kPixelEpsilon);
  EXPECT_NEAR(tmp.z(), 1.0f, kPixelEpsilon);

  tmp = ColorTransform::TriStim(1.0f, 0.0f, 0.0f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 1.0f, kPixelEpsilon);
  EXPECT_NEAR(tmp.y(), 0.0f, kPixelEpsilon);
  EXPECT_NEAR(tmp.z(), 0.0f, kPixelEpsilon);

  tmp = ColorTransform::TriStim(0.0f, 1.0f, 0.0f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 0.0f, kPixelEpsilon);
  EXPECT_NEAR(tmp.y(), 1.0f, kPixelEpsilon);
  EXPECT_NEAR(tmp.z(), 0.0f, kPixelEpsilon);

  tmp = ColorTransform::TriStim(0.0f, 0.0f, 1.0f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 0.0f, kPixelEpsilon);
  EXPECT_NEAR(tmp.y(), 0.0f, kPixelEpsilon);
  EXPECT_NEAR(tmp.z(), 1.0f, kPixelEpsilon);
}

TEST(SimpleColorSpace, Scale) {
  const float kPixelEpsilon = 1.5f / 255.f;
  ColorSpace srgb = ColorSpace::CreateSRGB();
  ColorSpace srgb_scaled = srgb.GetScaledColorSpace(2.0f);
  std::unique_ptr<ColorTransform> t(
      ColorTransform::NewColorTransform(srgb, srgb_scaled));

  ColorTransform::TriStim tmp(1.0f, 1.0f, 1.0f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 0.735356983052449f, kPixelEpsilon);
  EXPECT_NEAR(tmp.y(), 0.735356983052449f, kPixelEpsilon);
  EXPECT_NEAR(tmp.z(), 0.735356983052449f, kPixelEpsilon);
}

TEST(SimpleColorSpace, ToUndefined) {
  ColorSpace null;
  ColorSpace nonnull = gfx::ColorSpace::CreateSRGB();
  // Video should have 1 step: YUV to RGB.
  // Anything else should have 0 steps.
  ColorSpace video = gfx::ColorSpace::CreateREC709();
  std::unique_ptr<ColorTransform> video_to_null(
      ColorTransform::NewColorTransform(video, null));
  EXPECT_EQ(video_to_null->NumberOfStepsForTesting(), 1u);
  // Without optimization, video should have 2 steps: limited range to full
  // range, and YUV to RGB.
  ColorTransform::Options options;
  options.disable_optimizations = true;
  std::unique_ptr<ColorTransform> video_to_null_no_opt(
      ColorTransform::NewColorTransform(video, null, options));
  EXPECT_EQ(video_to_null_no_opt->NumberOfStepsForTesting(), 2u);

  // Test with an ICC profile that can't be represented as matrix+transfer.
  ColorSpace luttrcicc = ICCProfileForTestingNoAnalyticTrFn().GetColorSpace();
  std::unique_ptr<ColorTransform> luttrcicc_to_null(
      ColorTransform::NewColorTransform(luttrcicc, null));
  EXPECT_EQ(luttrcicc_to_null->NumberOfStepsForTesting(), 0u);
  std::unique_ptr<ColorTransform> luttrcicc_to_nonnull(
      ColorTransform::NewColorTransform(luttrcicc, nonnull));
  EXPECT_GT(luttrcicc_to_nonnull->NumberOfStepsForTesting(), 0u);

  // Test with an ICC profile that can.
  ColorSpace adobeicc = ICCProfileForTestingAdobeRGB().GetColorSpace();
  std::unique_ptr<ColorTransform> adobeicc_to_null(
      ColorTransform::NewColorTransform(adobeicc, null));
  EXPECT_EQ(adobeicc_to_null->NumberOfStepsForTesting(), 0u);
  std::unique_ptr<ColorTransform> adobeicc_to_nonnull(
      ColorTransform::NewColorTransform(adobeicc, nonnull));
  EXPECT_GT(adobeicc_to_nonnull->NumberOfStepsForTesting(), 0u);

  // And with something analytic.
  ColorSpace xyzd50 = gfx::ColorSpace::CreateXYZD50();
  std::unique_ptr<ColorTransform> xyzd50_to_null(
      ColorTransform::NewColorTransform(xyzd50, null));
  EXPECT_EQ(xyzd50_to_null->NumberOfStepsForTesting(), 0u);
  std::unique_ptr<ColorTransform> xyzd50_to_nonnull(
      ColorTransform::NewColorTransform(xyzd50, nonnull));
  EXPECT_GT(xyzd50_to_nonnull->NumberOfStepsForTesting(), 0u);
}

TEST(SimpleColorSpace, DefaultToSRGB) {
  // The default value should do no transformation, regardless of destination.
  ColorSpace unknown;
  std::unique_ptr<ColorTransform> t1(
      ColorTransform::NewColorTransform(unknown, ColorSpace::CreateSRGB()));
  EXPECT_EQ(t1->NumberOfStepsForTesting(), 0u);
  std::unique_ptr<ColorTransform> t2(
      ColorTransform::NewColorTransform(unknown, ColorSpace::CreateXYZD50()));
  EXPECT_EQ(t2->NumberOfStepsForTesting(), 0u);
}

// Checks that the generated SkSL fragment shaders can be parsed by
// SkSL::Compiler.
TEST(SimpleColorSpace, CanParseSkShaderSource) {
  std::vector<ColorSpace> common_color_spaces = {
      ColorSpace::CreateSRGB(),         ColorSpace::CreateDisplayP3D65(),
      ColorSpace::CreateExtendedSRGB(), ColorSpace::CreateSRGBLinear(),
      ColorSpace::CreateJpeg(),         ColorSpace::CreateREC601(),
      ColorSpace::CreateREC709()};
  for (const auto& src : common_color_spaces) {
    for (const auto& dst : common_color_spaces) {
      auto transform = ColorTransform::NewColorTransform(src, dst);
      EXPECT_NE(transform->GetSkRuntimeEffect(), nullptr);
    }
  }
}

class TransferTest : public testing::TestWithParam<ColorSpace::TransferID> {};

TEST_P(TransferTest, BasicTest) {
  gfx::ColorSpace space_with_transfer(ColorSpace::PrimaryID::BT709, GetParam(),
                                      ColorSpace::MatrixID::RGB,
                                      ColorSpace::RangeID::FULL);
  gfx::ColorSpace space_linear(
      ColorSpace::PrimaryID::BT709, ColorSpace::TransferID::LINEAR,
      ColorSpace::MatrixID::RGB, ColorSpace::RangeID::FULL);

  std::unique_ptr<ColorTransform> to_linear(
      ColorTransform::NewColorTransform(space_with_transfer, space_linear));

  std::unique_ptr<ColorTransform> from_linear(
      ColorTransform::NewColorTransform(space_linear, space_with_transfer));

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
    EXPECT_NEAR(x, tristim.x(), kMathEpsilon);
  }
}

INSTANTIATE_TEST_SUITE_P(ColorSpace,
                         TransferTest,
                         testing::ValuesIn(simple_transfers));

class ExtendedTransferTest
    : public testing::TestWithParam<ColorSpace::TransferID> {};

TEST_P(ExtendedTransferTest, extendedTest) {
  gfx::ColorSpace space_with_transfer(ColorSpace::PrimaryID::BT709, GetParam(),
                                      ColorSpace::MatrixID::RGB,
                                      ColorSpace::RangeID::FULL);
  gfx::ColorSpace space_linear(
      ColorSpace::PrimaryID::BT709, ColorSpace::TransferID::LINEAR,
      ColorSpace::MatrixID::RGB, ColorSpace::RangeID::FULL);

  std::unique_ptr<ColorTransform> to_linear(
      ColorTransform::NewColorTransform(space_with_transfer, space_linear));

  std::unique_ptr<ColorTransform> from_linear(
      ColorTransform::NewColorTransform(space_linear, space_with_transfer));

  for (float x = -2.0f; x <= 2.0f; x += 1.0f / 32.0f) {
    ColorTransform::TriStim tristim(x, x, x);
    to_linear->Transform(&tristim, 1);
    from_linear->Transform(&tristim, 1);
    EXPECT_NEAR(x, tristim.x(), kMathEpsilon);
  }
}

INSTANTIATE_TEST_SUITE_P(ColorSpace,
                         ExtendedTransferTest,
                         testing::ValuesIn(extended_transfers));

typedef std::tuple<ColorSpace::PrimaryID,
                   ColorSpace::TransferID,
                   ColorSpace::MatrixID,
                   ColorSpace::RangeID,
                   bool>
    ColorSpaceTestData;

class ColorSpaceTestBase : public testing::TestWithParam<ColorSpaceTestData> {
 public:
  ColorSpaceTestBase()
      : color_space_(std::get<0>(GetParam()),
                     std::get<1>(GetParam()),
                     std::get<2>(GetParam()),
                     std::get<3>(GetParam())) {
    options_.disable_optimizations = std::get<4>(GetParam());
  }

 protected:
  ColorSpace color_space_;
  ColorTransform::Options options_;
};

TEST_P(ColorSpaceTestBase, testNullTransform) {
  std::unique_ptr<ColorTransform> t(
      ColorTransform::NewColorTransform(color_space_, color_space_, options_));
  ColorTransform::TriStim tristim(0.4f, 0.5f, 0.6f);
  t->Transform(&tristim, 1);
  EXPECT_NEAR(tristim.x(), 0.4f, kMathEpsilon);
  EXPECT_NEAR(tristim.y(), 0.5f, kMathEpsilon);
  EXPECT_NEAR(tristim.z(), 0.6f, kMathEpsilon);
}

TEST_P(ColorSpaceTestBase, toXYZandBack) {
  std::unique_ptr<ColorTransform> t1(ColorTransform::NewColorTransform(
      color_space_, ColorSpace::CreateXYZD50(), options_));
  std::unique_ptr<ColorTransform> t2(ColorTransform::NewColorTransform(
      ColorSpace::CreateXYZD50(), color_space_, options_));
  ColorTransform::TriStim tristim(0.4f, 0.5f, 0.6f);
  t1->Transform(&tristim, 1);
  t2->Transform(&tristim, 1);
  EXPECT_NEAR(tristim.x(), 0.4f, kMathEpsilon);
  EXPECT_NEAR(tristim.y(), 0.5f, kMathEpsilon);
  EXPECT_NEAR(tristim.z(), 0.6f, kMathEpsilon);
}

INSTANTIATE_TEST_SUITE_P(
    A,
    ColorSpaceTestBase,
    testing::Combine(testing::ValuesIn(all_primaries),
                     testing::ValuesIn(simple_transfers),
                     testing::Values(ColorSpace::MatrixID::BT709),
                     testing::Values(ColorSpace::RangeID::LIMITED),
                     testing::ValuesIn(optimizations)));

INSTANTIATE_TEST_SUITE_P(
    B,
    ColorSpaceTestBase,
    testing::Combine(testing::Values(ColorSpace::PrimaryID::BT709),
                     testing::ValuesIn(simple_transfers),
                     testing::ValuesIn(all_matrices),
                     testing::ValuesIn(all_ranges),
                     testing::ValuesIn(optimizations)));

INSTANTIATE_TEST_SUITE_P(
    C,
    ColorSpaceTestBase,
    testing::Combine(testing::ValuesIn(all_primaries),
                     testing::Values(ColorSpace::TransferID::BT709),
                     testing::ValuesIn(all_matrices),
                     testing::ValuesIn(all_ranges),
                     testing::ValuesIn(optimizations)));

TEST(ColorSpaceTest, ExtendedSRGBScale) {
  ColorSpace space_unscaled = ColorSpace::CreateSRGB();
  float scale = 3.14;
  skcms_TransferFunction scaled_trfn =
      skia::ScaleTransferFunction(*skcms_sRGB_TransferFunction(), scale);
  ColorSpace space_scaled(ColorSpace::PrimaryID::BT709,
                          ColorSpace::TransferID::CUSTOM_HDR,
                          ColorSpace::MatrixID::RGB, ColorSpace::RangeID::FULL,
                          nullptr, &scaled_trfn);
  ColorSpace space_target(ColorSpace::PrimaryID::BT709,
                          ColorSpace::TransferID::LINEAR,
                          ColorSpace::MatrixID::RGB, ColorSpace::RangeID::FULL);

  std::unique_ptr<ColorTransform> xform_scaled(
      ColorTransform::NewColorTransform(space_scaled, space_target));
  std::unique_ptr<ColorTransform> xform_unscaled(
      ColorTransform::NewColorTransform(space_unscaled, space_target));

  // Make sure that we're testing something in the linear (0.001) and nonlinear
  // (the rest) segments of the function.
  ColorTransform::TriStim val_scaled(0.001, 0.5, 0.7);
  ColorTransform::TriStim val_unscaled = val_scaled;

  xform_scaled->Transform(&val_scaled, 1);
  xform_unscaled->Transform(&val_unscaled, 1);

  EXPECT_NEAR(val_scaled.x() / val_unscaled.x(), scale, kMathEpsilon);
  EXPECT_NEAR(val_scaled.y() / val_unscaled.y(), scale, kMathEpsilon);
  EXPECT_NEAR(val_scaled.z() / val_unscaled.z(), scale, kMathEpsilon);
}

TEST(ColorSpaceTest, ScrgbLinear80Nits) {
  ColorSpace dst(ColorSpace::PrimaryID::BT2020,
                 ColorSpace::TransferID::SCRGB_LINEAR_80_NITS);

  // PQ's 80 nits maps to 80 nits.
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures({}, {kHlgPqSdrRelative});

    ColorSpace src_pq = ColorSpace::CreateHDR10();

    ColorTransform::Options options;
    ColorTransform::RuntimeOptions runtime_options;

    std::unique_ptr<ColorTransform> xform(
        ColorTransform::NewColorTransform(src_pq, dst, options));

    constexpr float kPq80Nits = 0.4858567653886785f;
    ColorTransform::TriStim val(kPq80Nits, kPq80Nits, kPq80Nits);
    xform->Transform(&val, 1, runtime_options);
    EXPECT_NEAR(val.x(), 1.f, kMathEpsilon);
  }

  // SDR white is scaled by 80 nits.
  {
    constexpr float kSdrWhite = 300.f;

    ColorSpace src_srgb = ColorSpace::CreateSRGB();

    ColorTransform::Options options;
    ColorTransform::RuntimeOptions runtime_options;
    runtime_options.dst_sdr_max_luminance_nits = kSdrWhite;

    std::unique_ptr<ColorTransform> xform(
        ColorTransform::NewColorTransform(src_srgb, dst, options));

    ColorTransform::TriStim val(1.f, 1.f, 1.f);
    xform->Transform(&val, 1, runtime_options);
    EXPECT_NEAR(val.x(), kSdrWhite / 80.f, kMathEpsilon);
  }

  // PQ's maximum maps to the maximum value when tonemapped.
  {
    constexpr float kSdrWhite = 150.f;
    constexpr float kDstMaxLumRel = 2.f;

    ColorSpace src_pq = ColorSpace::CreateHDR10();

    ColorTransform::Options options;
    ColorTransform::RuntimeOptions runtime_options;
    options.tone_map_pq_and_hlg_to_dst = true;
    runtime_options.dst_sdr_max_luminance_nits = kSdrWhite;
    runtime_options.dst_max_luminance_relative = kDstMaxLumRel;
    runtime_options.src_hdr_metadata =
        HDRMetadata(HdrMetadataCta861_3(10000.f, 100.f));

    std::unique_ptr<ColorTransform> xform(
        ColorTransform::NewColorTransform(src_pq, dst, options));

    ColorTransform::TriStim val(1.f, 1.f, 1.f);
    xform->Transform(&val, 1, runtime_options);
    EXPECT_NEAR(val.x(), kDstMaxLumRel * kSdrWhite / 80.f, kMathEpsilon);
  }

  // HLG's maximum value will be 12 times 203 nits.
  // TODO(crbug.com/40267141): This is not an appropriate value. This
  // path is to be deleted.
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures({}, {kHlgPqUnifiedTonemap, kHlgPqSdrRelative});

    constexpr float kSdrWhite = 300.f;

    ColorSpace src_hlg(ColorSpace::PrimaryID::BT2020,
                       ColorSpace::TransferID::HLG);

    ColorTransform::Options options;
    ColorTransform::RuntimeOptions runtime_options;
    runtime_options.dst_sdr_max_luminance_nits = kSdrWhite;

    std::unique_ptr<ColorTransform> xform(
        ColorTransform::NewColorTransform(src_hlg, dst, options));

    ColorTransform::TriStim val(1.f, 1.f, 1.f);
    xform->Transform(&val, 1, runtime_options);
    EXPECT_NEAR(val.x(), 12.f * ColorSpace::kDefaultSDRWhiteLevel / 80.f,
                kMathEpsilon);
  }

  // HLG's maximum maps to the maximum value when tonemapped.
  // TODO(crbug.com/40267141): This path is to be deleted.
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures({}, {kHlgPqUnifiedTonemap, kHlgPqSdrRelative});

    constexpr float kSdrWhite = 200.f;
    constexpr float kDstMaxLumRel = 2.f;

    ColorSpace src_hlg(ColorSpace::PrimaryID::BT2020,
                       ColorSpace::TransferID::HLG);

    ColorTransform::Options options;
    ColorTransform::RuntimeOptions runtime_options;
    options.tone_map_pq_and_hlg_to_dst = true;
    runtime_options.dst_sdr_max_luminance_nits = kSdrWhite;
    runtime_options.dst_max_luminance_relative = kDstMaxLumRel;

    std::unique_ptr<ColorTransform> xform(
        ColorTransform::NewColorTransform(src_hlg, dst, options));

    {
      ColorTransform::TriStim val(1.f, 1.f, 1.f);
      xform->Transform(&val, 1, runtime_options);
      EXPECT_NEAR(val.x(), kDstMaxLumRel * kSdrWhite / 80.f, kMathLargeEpsilon);
    }

    // Test a non-maximum value which is affected by the OOTF curve.
    {
      ColorTransform::TriStim val(0.5f, 0.5f, 0.5f);
      xform->Transform(&val, 1, runtime_options);
      EXPECT_NEAR(val.x(), 0.38373923301696777f, kMathLargeEpsilon);
    }
  }
}

TEST(ColorSpaceTest, HLGTonemap) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({kHlgPqUnifiedTonemap}, {kHlgPqSdrRelative});

  ColorSpace dst(ColorSpace::PrimaryID::BT2020,
                 ColorSpace::TransferID::SCRGB_LINEAR_80_NITS);
  ColorSpace src_hlg(ColorSpace::PrimaryID::BT2020,
                     ColorSpace::TransferID::HLG);
  ColorTransform::Options options;
  options.tone_map_pq_and_hlg_to_dst = true;

  std::unique_ptr<ColorTransform> xform(
      ColorTransform::NewColorTransform(src_hlg, dst, options));

  // If the headroom is low enough that HLG will exceed it, then we will map to
  // the headroom.
  {
    ColorTransform::RuntimeOptions runtime_options;
    constexpr float kSdrWhite = 100.f;
    constexpr float kDstMaxLumRel = 2.f;
    runtime_options.dst_sdr_max_luminance_nits = kSdrWhite;
    runtime_options.dst_max_luminance_relative = kDstMaxLumRel;

    ColorTransform::TriStim val(1.f, 1.f, 1.f);
    xform->Transform(&val, 1, runtime_options);
    EXPECT_NEAR(val.x(), kDstMaxLumRel * kSdrWhite / 80.f, kMathLargeEpsilon);
  }

  // We will max out at the reference maximum if it is below the headroom.
  {
    ColorTransform::RuntimeOptions runtime_options;
    constexpr float kSdrWhite = 250.f;
    constexpr float kDstMaxLumRel = 6.f;
    runtime_options.dst_sdr_max_luminance_nits = kSdrWhite;
    runtime_options.dst_max_luminance_relative = kDstMaxLumRel;

    ColorTransform::TriStim val(1.f, 1.f, 1.f);
    xform->Transform(&val, 1, runtime_options);
    EXPECT_NEAR(val.x(), 1000.f / 80.f, kMathLargeEpsilon);
  }
}

TEST(ColorSpaceTest, HLGNoTonemap) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({kHlgPqUnifiedTonemap}, {kHlgPqSdrRelative});

  ColorSpace dst(ColorSpace::PrimaryID::BT2020,
                 ColorSpace::TransferID::SCRGB_LINEAR_80_NITS);
  ColorSpace src_hlg(ColorSpace::PrimaryID::BT2020,
                     ColorSpace::TransferID::HLG);
  ColorTransform::Options options;
  options.tone_map_pq_and_hlg_to_dst = false;

  std::unique_ptr<ColorTransform> xform(
      ColorTransform::NewColorTransform(src_hlg, dst, options));

  ColorTransform::RuntimeOptions runtime_options;
  constexpr float kSdrWhite = 100.f;
  constexpr float kDstMaxLumRel = 2.f;
  runtime_options.dst_sdr_max_luminance_nits = kSdrWhite;
  runtime_options.dst_max_luminance_relative = kDstMaxLumRel;

  // HLG 75% will match 203 nits.
  {
    ColorTransform::TriStim val(0.75f, 0.75f, 0.75f);
    xform->Transform(&val, 1, runtime_options);
    EXPECT_NEAR(val.x(), 203.f / 80.f, kMathLargeEpsilon);
  }

  // HLG 100% will match 1000 nits.
  {
    ColorTransform::TriStim val(1.f, 1.f, 1.f);
    xform->Transform(&val, 1, runtime_options);
    EXPECT_NEAR(val.x(), 1000.f / 80.f, kMathLargeEpsilon);
  }
}

TEST(ColorSpaceTest, HLGTonemapSdrRelative) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({kHlgPqUnifiedTonemap, kHlgPqSdrRelative}, {});

  ColorSpace dst(ColorSpace::PrimaryID::BT2020, ColorSpace::TransferID::LINEAR);
  ColorSpace src_hlg(ColorSpace::PrimaryID::BT2020,
                     ColorSpace::TransferID::HLG);
  ColorTransform::Options options;
  options.tone_map_pq_and_hlg_to_dst = true;

  std::unique_ptr<ColorTransform> xform(
      ColorTransform::NewColorTransform(src_hlg, dst, options));

  // If the headroom is low enough that HLG will exceed it, then we will map to
  // the headroom.
  {
    ColorTransform::RuntimeOptions runtime_options;
    constexpr float kSdrWhite = 100.f;
    constexpr float kDstMaxLumRel = 2.f;
    runtime_options.dst_sdr_max_luminance_nits = kSdrWhite;
    runtime_options.dst_max_luminance_relative = kDstMaxLumRel;

    ColorTransform::TriStim val(1.f, 1.f, 1.f);
    xform->Transform(&val, 1, runtime_options);
    EXPECT_NEAR(val.x(), kDstMaxLumRel, kMathLargeEpsilon);
  }

  // We will max out at the reference maximum if it is below the headroom.
  {
    ColorTransform::RuntimeOptions runtime_options;
    constexpr float kSdrWhite = 250.f;
    constexpr float kDstMaxLumRel = 6.f;
    runtime_options.dst_sdr_max_luminance_nits = kSdrWhite;
    runtime_options.dst_max_luminance_relative = kDstMaxLumRel;

    ColorTransform::TriStim val(1.f, 1.f, 1.f);
    xform->Transform(&val, 1, runtime_options);
    EXPECT_NEAR(val.x(), 1000.f / ColorSpace::kDefaultSDRWhiteLevel,
                kMathLargeEpsilon);
  }
}

TEST(ColorSpaceTest, HLGNoTonemapSdrRelative) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({kHlgPqUnifiedTonemap, kHlgPqSdrRelative}, {});

  ColorSpace dst(ColorSpace::PrimaryID::BT2020, ColorSpace::TransferID::LINEAR);
  ColorSpace src_hlg(ColorSpace::PrimaryID::BT2020,
                     ColorSpace::TransferID::HLG);
  ColorTransform::Options options;
  options.tone_map_pq_and_hlg_to_dst = false;

  std::unique_ptr<ColorTransform> xform(
      ColorTransform::NewColorTransform(src_hlg, dst, options));

  ColorTransform::RuntimeOptions runtime_options;
  constexpr float kSdrWhite = 100.f;
  constexpr float kDstMaxLumRel = 2.f;
  runtime_options.dst_sdr_max_luminance_nits = kSdrWhite;
  runtime_options.dst_max_luminance_relative = kDstMaxLumRel;

  // HLG 75% will match 203 nits.
  {
    ColorTransform::TriStim val(0.75f, 0.75f, 0.75f);
    xform->Transform(&val, 1, runtime_options);
    EXPECT_NEAR(val.x(), 1.f, kMathLargeEpsilon);
  }

  // HLG 100% will match 1000 nits.
  {
    ColorTransform::TriStim val(1.f, 1.f, 1.f);
    xform->Transform(&val, 1, runtime_options);
    EXPECT_NEAR(val.x(), 1000.f / ColorSpace::kDefaultSDRWhiteLevel,
                kMathLargeEpsilon);
  }
}

TEST(ColorSpaceTest, PQTonemapSdrRelative) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({kHlgPqUnifiedTonemap, kHlgPqSdrRelative}, {});

  ColorSpace dst(ColorSpace::PrimaryID::BT2020, ColorSpace::TransferID::LINEAR);
  ColorSpace src_hlg(ColorSpace::PrimaryID::BT2020, ColorSpace::TransferID::PQ);
  ColorTransform::Options options;
  options.tone_map_pq_and_hlg_to_dst = true;

  std::unique_ptr<ColorTransform> xform(
      ColorTransform::NewColorTransform(src_hlg, dst, options));

  constexpr float kPQ1000Nits = 0.751827096247041f;

  // If the headroom is low enough that the maximum PQ value will exceed it,
  // then we will map to the headroom.
  {
    ColorTransform::RuntimeOptions runtime_options;
    constexpr float kSdrWhite = 100.f;
    constexpr float kDstMaxLumRel = 2.f;
    runtime_options.dst_sdr_max_luminance_nits = kSdrWhite;
    runtime_options.dst_max_luminance_relative = kDstMaxLumRel;
    runtime_options.src_hdr_metadata =
        HDRMetadata(HdrMetadataCta861_3(10000.f, 100.f));

    ColorTransform::TriStim val(1.f, 1.f, 1.f);
    xform->Transform(&val, 1, runtime_options);
    EXPECT_NEAR(val.x(), kDstMaxLumRel, kMathLargeEpsilon);
  }

  // Ensure that the maximum value specified in metadata is mapped to the
  // headroom.
  {
    ColorTransform::RuntimeOptions runtime_options;
    constexpr float kSdrWhite = 100.f;
    constexpr float kDstMaxLumRel = 2.f;
    runtime_options.dst_sdr_max_luminance_nits = kSdrWhite;
    runtime_options.dst_max_luminance_relative = kDstMaxLumRel;

    ColorTransform::TriStim val(kPQ1000Nits, kPQ1000Nits, kPQ1000Nits);
    xform->Transform(&val, 1, runtime_options);
    EXPECT_NEAR(val.x(), 2.f, kMathLargeEpsilon);
  }

  // If we do not reach the headroom, then no tonemapping is applied.
  {
    ColorTransform::RuntimeOptions runtime_options;
    constexpr float kSdrWhite = 90.f;
    constexpr float kDstMaxLumRel = 51.f;
    runtime_options.dst_sdr_max_luminance_nits = kSdrWhite;
    runtime_options.dst_max_luminance_relative = kDstMaxLumRel;

    ColorTransform::TriStim val(1.f, 1.f, 1.f);
    xform->Transform(&val, 1, runtime_options);
    EXPECT_NEAR(val.x(), 10000.f / ColorSpace::kDefaultSDRWhiteLevel,
                kMathLargeEpsilon);
  }

  // If we do not reach the headroom (because of metadata), then no tonemapping
  // is applied.
  {
    ColorTransform::RuntimeOptions runtime_options;
    constexpr float kSdrWhite = 100.f;
    constexpr float kDstMaxLumRel = 6.f;
    runtime_options.dst_sdr_max_luminance_nits = kSdrWhite;
    runtime_options.dst_max_luminance_relative = kDstMaxLumRel;
    runtime_options.src_hdr_metadata =
        HDRMetadata(HdrMetadataCta861_3(1000.f, 100.f));

    ColorTransform::TriStim val(kPQ1000Nits, kPQ1000Nits, kPQ1000Nits);
    xform->Transform(&val, 1, runtime_options);
    EXPECT_NEAR(val.x(), 1000.f / ColorSpace::kDefaultSDRWhiteLevel,
                kMathLargeEpsilon);
  }
}

TEST(ColorSpaceTest, PQSDRWhiteLevel) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({}, {kHlgPqSdrRelative});

  // The PQ function maps |pq_encoded_nits| to |nits|. We mangle it a bit with
  // the SDR white level.
  float pq_encoded_nits[] = {
      0.485857f,
      0.508078f,
      0.579133f,
  };
  float nits[] = {80.f, 100.f, 200.f};

  for (size_t i = 0; i < 3; ++i) {
    // We'll set the SDR white level to the values in |nits| and also the
    // default.
    const ColorSpace hdr10 = ColorSpace::CreateHDR10();
    ColorTransform::Options options;
    ColorTransform::RuntimeOptions runtime_options;
    runtime_options.dst_sdr_max_luminance_nits = nits[i];

    // Transform to the same color space, but with the LINEAR_HDR transfer
    // function.
    ColorSpace target(ColorSpace::PrimaryID::BT2020,
                      ColorSpace::TransferID::LINEAR_HDR,
                      ColorSpace::MatrixID::RGB, ColorSpace::RangeID::FULL);
    std::unique_ptr<ColorTransform> xform(
        ColorTransform::NewColorTransform(hdr10, target, options));

    // Do the transform to the values in |pq_encoded_nits|.
    ColorTransform::TriStim val(pq_encoded_nits[0], pq_encoded_nits[1],
                                pq_encoded_nits[2]);
    xform->Transform(&val, 1, runtime_options);

    // The white level should be mapped to 1.
    switch (i) {
      case 0:
        EXPECT_NEAR(val.x(), 1.f, kMathEpsilon);
        break;
      case 1:
        EXPECT_NEAR(val.y(), 1.f, kMathEpsilon);
        break;
      case 2:
        EXPECT_NEAR(val.z(), 1.f, kMathEpsilon);
        break;
      case 3:
        // Check that the default white level is 100 nits.
        EXPECT_NEAR(val.y(), 1.f, kMathEpsilon);
        break;
    }

    // The nit ratios should be preserved by the transform.
    EXPECT_NEAR(val.y() / val.x(), nits[1] / nits[0], kMathEpsilon);
    EXPECT_NEAR(val.z() / val.x(), nits[2] / nits[0], kMathEpsilon);

    // Test the inverse transform.
    std::unique_ptr<ColorTransform> xform_inv(
        ColorTransform::NewColorTransform(target, hdr10, options));
    xform_inv->Transform(&val, 1, runtime_options);
    EXPECT_NEAR(val.x(), pq_encoded_nits[0], kMathEpsilon);
    EXPECT_NEAR(val.y(), pq_encoded_nits[1], kMathEpsilon);
    EXPECT_NEAR(val.z(), pq_encoded_nits[2], kMathEpsilon);
  }
}

TEST(ColorSpaceTest, HLGSDRWhiteLevel) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({}, {kHlgPqUnifiedTonemap, kHlgPqSdrRelative});

  // These values are (1.0f * nits[i] / kDefaultSDRWhiteLevel) converted to
  // LINEAR_HDR via the HLG transfer function.
  constexpr float hlg_encoded_nits[] = {
      0.447214f,  // 0.5 * sqrt(1.0 * 80 / 100)
      0.5f,       // 0.5 * sqrt(1.0 * 100 / 100)
      0.65641f,   // 0.17883277 * ln(1.0 * 200 / 100 - 0.28466892) + 0.55991073
  };
  constexpr float nits[] = {203.f / 2, 203.f, 203.f * 2};

  for (size_t i = 0; i < 3; ++i) {
    // We'll set the SDR white level to the values in |nits| and also the
    // default.
    const ColorSpace hlg = ColorSpace::CreateHLG();
    ColorTransform::Options options;
    ColorTransform::RuntimeOptions runtime_options;
    runtime_options.dst_sdr_max_luminance_nits = nits[i];

    // Transform to the same color space, but with the LINEAR_HDR transfer
    // function.
    ColorSpace target(ColorSpace::PrimaryID::BT2020,
                      ColorSpace::TransferID::LINEAR_HDR,
                      ColorSpace::MatrixID::RGB, ColorSpace::RangeID::FULL);
    std::unique_ptr<ColorTransform> xform(
        ColorTransform::NewColorTransform(hlg, target, options));

    // Do the transform to the values in |hlg_encoded_nits|.
    ColorTransform::TriStim val(hlg_encoded_nits[0], hlg_encoded_nits[1],
                                hlg_encoded_nits[2]);
    xform->Transform(&val, 1, runtime_options);

    // Each |hlg_encoded_nits| value should map back to 1.0f after conversion
    // via a ColorSpace with the right SDR white level.
    switch (i) {
      case 0:
        EXPECT_NEAR(val.x(), 1.6f, kMathEpsilon);
        break;
      case 1:
        EXPECT_NEAR(val.y(), 1.f, kMathEpsilon);
        break;
      case 2:
        EXPECT_NEAR(val.z(), 1.f, kMathEpsilon);
        break;
      case 3:
        // Check that the default white level is 100 nits.
        EXPECT_NEAR(val.y(), 1.f, kMathEpsilon);
        break;
    }

    // Test the inverse transform.
    std::unique_ptr<ColorTransform> xform_inv(
        ColorTransform::NewColorTransform(target, hlg, options));
    xform_inv->Transform(&val, 1, runtime_options);
    EXPECT_NEAR(val.x(), hlg_encoded_nits[0], kMathEpsilon);
    EXPECT_NEAR(val.y(), hlg_encoded_nits[1], kMathEpsilon);
    EXPECT_NEAR(val.z(), hlg_encoded_nits[2], kMathEpsilon);
  }
}

TEST(ColorSpaceTest, PiecewiseHDR) {
  // The sRGB function evaluated at a couple of test points.
  const float srgb_x0 = 0.01;
  const float srgb_y0 = 0.00077399380805;
  const float srgb_x1 = 0.5;
  const float srgb_y1 = 0.2140411174732872;

  // Parameters for CreatePiecewiseHDR to test.
  const std::vector<float> test_sdr_joints = {
      0.25f,
      0.5f,
      0.75f,
  };
  const std::vector<float> test_hdr_levels = {
      1.5f,
      2.0f,
      5.0f,
  };

  // Go through all combinations.
  for (float sdr_joint : test_sdr_joints) {
    for (float hdr_level : test_hdr_levels) {
      ColorSpace hdr = ColorSpace::CreatePiecewiseHDR(
          ColorSpace::PrimaryID::BT709, sdr_joint, hdr_level);
      ColorSpace linear(ColorSpace::PrimaryID::BT709,
                        ColorSpace::TransferID::LINEAR_HDR);
      std::unique_ptr<ColorTransform> xform_to(
          ColorTransform::NewColorTransform(hdr, linear));
      std::unique_ptr<ColorTransform> xform_from(
          ColorTransform::NewColorTransform(linear, hdr));

      // We're going to to test both sides of the joint points. Use this
      // epsilon, which is much smaller than kMathEpsilon, to make that
      // adjustment.
      const float kSideEpsilon = kMathEpsilon / 100;

      const size_t kTestPointCount = 8;
      const float test_x[kTestPointCount] = {
          // Test the linear segment of the sRGB function.
          srgb_x0 * sdr_joint,
          // Test the exponential segment of the sRGB function.
          srgb_x1 * sdr_joint,
          // Test epsilon before the HDR joint
          sdr_joint - kSideEpsilon,
          // Test the HDR joint
          sdr_joint,
          // Test epsilon after the HDR joint
          sdr_joint + kSideEpsilon,
          // Test the middle of the linear HDR segment
          sdr_joint + 0.5f * (1.f - sdr_joint),
          // Test just before the end of the linear HDR segment.
          1.f - kSideEpsilon,
          // Test the endpoint of the linear HDR segment.
          1.f,
      };
      const float test_y[kTestPointCount] = {
          srgb_y0,
          srgb_y1,
          1.f - kSideEpsilon,
          1.f,
          1.f + kSideEpsilon,
          0.5f * (1.f + hdr_level),
          hdr_level - kSideEpsilon,
          hdr_level,
      };
      for (size_t i = 0; i < kTestPointCount; ++i) {
        ColorTransform::TriStim val;
        val.set_x(test_x[i]);
        xform_to->Transform(&val, 1);
        EXPECT_NEAR(val.x(), test_y[i], kMathEpsilon)
            << " test_x[i] is " << test_x[i];

        val.set_x(test_y[i]);
        xform_from->Transform(&val, 1);
        EXPECT_NEAR(val.x(), test_x[i], kMathEpsilon)
            << " test_y[i] is " << test_y[i];
      }
    }
  }
}

}  // namespace gfx
