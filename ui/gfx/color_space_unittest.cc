// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cmath>
#include <tuple>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/skia_color_space_util.h"

namespace gfx {
namespace {

const float kEpsilon = 1.0e-3f;

// Returns the L-infty difference of u and v.
float Diff(const SkVector4& u, const SkVector4& v) {
  float result = 0;
  for (size_t i = 0; i < 4; ++i)
    result = std::max(result, std::abs(u.fData[i] - v.fData[i]));
  return result;
}

TEST(ColorSpace, RGBToYUV) {
  const size_t kNumTestRGBs = 3;
  SkVector4 test_rgbs[kNumTestRGBs] = {
      SkVector4(1.f, 0.f, 0.f, 1.f),
      SkVector4(0.f, 1.f, 0.f, 1.f),
      SkVector4(0.f, 0.f, 1.f, 1.f),
  };

  const size_t kNumColorSpaces = 4;
  gfx::ColorSpace color_spaces[kNumColorSpaces] = {
      gfx::ColorSpace::CreateREC601(), gfx::ColorSpace::CreateREC709(),
      gfx::ColorSpace::CreateJpeg(), gfx::ColorSpace::CreateXYZD50(),
  };

  SkVector4 expected_yuvs[kNumColorSpaces][kNumTestRGBs] = {
      // REC601
      {
          SkVector4(0.3195f, 0.3518f, 0.9392f, 1.0000f),
          SkVector4(0.5669f, 0.2090f, 0.1322f, 1.0000f),
          SkVector4(0.1607f, 0.9392f, 0.4286f, 1.0000f),
      },
      // REC709
      {
          SkVector4(0.2453f, 0.3994f, 0.9392f, 1.0000f),
          SkVector4(0.6770f, 0.1614f, 0.1011f, 1.0000f),
          SkVector4(0.1248f, 0.9392f, 0.4597f, 1.0000f),
      },
      // Jpeg
      {
          SkVector4(0.2990f, 0.3313f, 1.0000f, 1.0000f),
          SkVector4(0.5870f, 0.1687f, 0.0813f, 1.0000f),
          SkVector4(0.1140f, 1.0000f, 0.4187f, 1.0000f),
      },
      // XYZD50
      {
          SkVector4(1.0000f, 0.0000f, 0.0000f, 1.0000f),
          SkVector4(0.0000f, 1.0000f, 0.0000f, 1.0000f),
          SkVector4(0.0000f, 0.0000f, 1.0000f, 1.0000f),
      },
  };

  for (size_t i = 0; i < kNumColorSpaces; ++i) {
    SkMatrix44 transfer;
    color_spaces[i].GetTransferMatrix(/*bit_depth=*/8, &transfer);

    SkMatrix44 range_adjust;
    color_spaces[i].GetRangeAdjustMatrix(&range_adjust);

    SkMatrix44 range_adjust_inv;
    range_adjust.invert(&range_adjust_inv);

    for (size_t j = 0; j < kNumTestRGBs; ++j) {
      SkVector4 yuv = range_adjust_inv * transfer * test_rgbs[j];
      EXPECT_LT(Diff(yuv, expected_yuvs[i][j]), kEpsilon);
    }
  }
}

TEST(ColorSpace, RangeAdjust) {
  const size_t kNumTestYUVs = 2;
  SkVector4 test_yuvs[kNumTestYUVs] = {
      SkVector4(1.f, 1.f, 1.f, 1.f),
      SkVector4(0.f, 0.f, 0.f, 1.f),
  };

  const size_t kNumBitDepths = 3;
  int bit_depths[kNumBitDepths] = {8, 10, 12};

  const size_t kNumColorSpaces = 3;
  ColorSpace color_spaces[kNumColorSpaces] = {
      ColorSpace::CreateREC601(),
      ColorSpace::CreateJpeg(),
      ColorSpace(ColorSpace::PrimaryID::INVALID,
                 ColorSpace::TransferID::INVALID, ColorSpace::MatrixID::YCOCG,
                 ColorSpace::RangeID::LIMITED),
  };

  SkVector4 expected_yuvs[kNumColorSpaces][kNumBitDepths][kNumTestYUVs] = {
      // REC601
      {
          // 8bpc
          {
              SkVector4(235.f / 255.f, 239.5f / 255.f, 239.5f / 255.f, 1.0000f),
              SkVector4(16.f / 255.f, 15.5f / 255.f, 15.5f / 255.f, 1.0000f),
          },
          // 10bpc
          {
              SkVector4(940.f / 1023.f, 959.5f / 1023.f, 959.5f / 1023.f,
                        1.0000f),
              SkVector4(64.f / 1023.f, 63.5f / 1023.f, 63.5f / 1023.f, 1.0000f),
          },
          // 12bpc
          {
              SkVector4(3760.f / 4095.f, 3839.5f / 4095.f, 3839.5f / 4095.f,
                        1.0000f),
              SkVector4(256.f / 4095.f, 255.5f / 4095.f, 255.5f / 4095.f,
                        1.0000f),
          },
      },
      // Jpeg
      {
          // 8bpc
          {
              SkVector4(1.0000f, 1.0000f, 1.0000f, 1.0000f),
              SkVector4(0.0000f, 0.0000f, 0.0000f, 1.0000f),
          },
          // 10bpc
          {
              SkVector4(1.0000f, 1.0000f, 1.0000f, 1.0000f),
              SkVector4(0.0000f, 0.0000f, 0.0000f, 1.0000f),
          },
          // 12bpc
          {
              SkVector4(1.0000f, 1.0000f, 1.0000f, 1.0000f),
              SkVector4(0.0000f, 0.0000f, 0.0000f, 1.0000f),
          },
      },
      // YCoCg
      {
          // 8bpc
          {
              SkVector4(235.f / 255.f, 235.f / 255.f, 235.f / 255.f, 1.0000f),
              SkVector4(16.f / 255.f, 16.f / 255.f, 16.f / 255.f, 1.0000f),
          },
          // 10bpc
          {
              SkVector4(940.f / 1023.f, 940.f / 1023.f, 940.f / 1023.f,
                        1.0000f),
              SkVector4(64.f / 1023.f, 64.f / 1023.f, 64.f / 1023.f, 1.0000f),
          },
          // 12bpc
          {
              SkVector4(3760.f / 4095.f, 3760.f / 4095.f, 3760.f / 4095.f,
                        1.0000f),
              SkVector4(256.f / 4095.f, 256.f / 4095.f, 256.f / 4095.f,
                        1.0000f),
          },
      },
  };

  for (size_t i = 0; i < kNumColorSpaces; ++i) {
    for (size_t j = 0; j < kNumBitDepths; ++j) {
      SkMatrix44 range_adjust;
      color_spaces[i].GetRangeAdjustMatrix(bit_depths[j], &range_adjust);

      SkMatrix44 range_adjust_inv;
      range_adjust.invert(&range_adjust_inv);

      for (size_t k = 0; k < kNumTestYUVs; ++k) {
        SkVector4 yuv = range_adjust_inv * test_yuvs[k];
        EXPECT_LT(Diff(yuv, expected_yuvs[i][j][k]), kEpsilon);
      }
    }
  }
}

TEST(ColorSpace, Blending) {
  ColorSpace display_color_space;

  // A linear transfer function being used for HDR should be blended using an
  // sRGB-like transfer function.
  display_color_space = ColorSpace::CreateSCRGBLinear();
  EXPECT_FALSE(display_color_space.IsSuitableForBlending());

  // If not used for HDR, a linear transfer function should be left unchanged.
  display_color_space = ColorSpace::CreateXYZD50();
  EXPECT_TRUE(display_color_space.IsSuitableForBlending());
}

TEST(ColorSpace, ConversionToAndFromSkColorSpace) {
  const size_t kNumTests = 5;
  skcms_Matrix3x3 primary_matrix = {{
      {0.205276f, 0.625671f, 0.060867f},
      {0.149185f, 0.063217f, 0.744553f},
      {0.609741f, 0.311111f, 0.019470f},
  }};
  skcms_TransferFunction transfer_fn = {2.1f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};

  ColorSpace color_spaces[kNumTests] = {
      ColorSpace(ColorSpace::PrimaryID::BT709,
                 ColorSpace::TransferID::IEC61966_2_1),
      ColorSpace(ColorSpace::PrimaryID::ADOBE_RGB,
                 ColorSpace::TransferID::IEC61966_2_1),
      ColorSpace(ColorSpace::PrimaryID::SMPTEST432_1,
                 ColorSpace::TransferID::LINEAR),
      ColorSpace(ColorSpace::PrimaryID::BT2020,
                 ColorSpace::TransferID::IEC61966_2_1),
      ColorSpace::CreateCustom(primary_matrix, transfer_fn),
  };
  sk_sp<SkColorSpace> sk_color_spaces[kNumTests] = {
      SkColorSpace::MakeSRGB(),
      SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB, SkNamedGamut::kAdobeRGB),
      SkColorSpace::MakeRGB(SkNamedTransferFn::kLinear,
                            SkNamedGamut::kDisplayP3),
      SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB, SkNamedGamut::kRec2020),
      SkColorSpace::MakeRGB(transfer_fn, primary_matrix),
  };

  // Test that converting from ColorSpace to SkColorSpace is producing an
  // equivalent representation.
  for (size_t i = 0; i < kNumTests; ++i) {
    EXPECT_TRUE(SkColorSpace::Equals(color_spaces[i].ToSkColorSpace().get(),
                                     sk_color_spaces[i].get()))
        << " on iteration i = " << i;
  }

  // Invariant test: Test that converting a SkColorSpace to a ColorSpace is
  // producing an equivalent representation; and then converting the converted
  // ColorSpace back to SkColorSpace is also producing an equivalent
  // representation.
  for (size_t i = 0; i < kNumTests; ++i) {
    const ColorSpace from_sk_color_space(*sk_color_spaces[i]);
    EXPECT_EQ(color_spaces[i], from_sk_color_space);
    EXPECT_TRUE(SkColorSpace::Equals(
        sk_color_spaces[i].get(), from_sk_color_space.ToSkColorSpace().get()));
  }
}

TEST(ColorSpace, PQToSkColorSpace) {
  ColorSpace color_space;
  ColorSpace roundtrip_color_space;
  float roundtrip_sdr_white_level;
  const float kEpsilon = 1.e-5f;

  // We expect that when a white point is specified, the conversion from
  // ColorSpace -> SkColorSpace -> ColorSpace be the identity. Because of
  // rounding error, this will not quite be the case.
  color_space = ColorSpace::CreateHDR10(50.f);
  roundtrip_color_space = ColorSpace(*color_space.ToSkColorSpace());
  EXPECT_TRUE(
      roundtrip_color_space.GetSDRWhiteLevel(&roundtrip_sdr_white_level));
  EXPECT_NEAR(50.f, roundtrip_sdr_white_level, kEpsilon);
  EXPECT_EQ(ColorSpace::TransferID::SMPTEST2084,
            roundtrip_color_space.GetTransferID());

  // When no white level is specified, we should get an SkColorSpace that
  // specifies the default white level. Of note is that in the roundtrip, the
  // value of kDefaultSDRWhiteLevel gets baked in.
  color_space = ColorSpace::CreateHDR10();
  roundtrip_color_space = ColorSpace(*color_space.ToSkColorSpace());
  EXPECT_TRUE(
      roundtrip_color_space.GetSDRWhiteLevel(&roundtrip_sdr_white_level));
  EXPECT_NEAR(ColorSpace::kDefaultSDRWhiteLevel, roundtrip_sdr_white_level,
              kEpsilon);
}

TEST(ColorSpace, HLGToSkColorSpace) {
  ColorSpace color_space;
  ColorSpace roundtrip_color_space;
  float roundtrip_sdr_white_level;

  // We expect that when a white point is specified, the conversion from
  // ColorSpace -> SkColorSpace -> ColorSpace be the identity. Because of
  // rounding error, this will not quite be the case.
  constexpr float kSDRWhiteLevel = 50.0f;
  color_space = ColorSpace::CreateHLG().GetWithSDRWhiteLevel(kSDRWhiteLevel);
  roundtrip_color_space = ColorSpace(*color_space.ToSkColorSpace());
  EXPECT_TRUE(
      roundtrip_color_space.GetSDRWhiteLevel(&roundtrip_sdr_white_level));
  EXPECT_FLOAT_EQ(kSDRWhiteLevel, roundtrip_sdr_white_level);
  EXPECT_EQ(ColorSpace::TransferID::ARIB_STD_B67,
            roundtrip_color_space.GetTransferID());

  // When no white level is specified, we should get an SkColorSpace that
  // specifies the default white level. Of note is that in the roundtrip, the
  // value of kDefaultSDRWhiteLevel gets baked in.
  color_space = ColorSpace::CreateHLG();
  roundtrip_color_space = ColorSpace(*color_space.ToSkColorSpace());
  EXPECT_TRUE(
      roundtrip_color_space.GetSDRWhiteLevel(&roundtrip_sdr_white_level));
  EXPECT_NEAR(ColorSpace::kDefaultSDRWhiteLevel, roundtrip_sdr_white_level,
              kEpsilon);
}

TEST(ColorSpace, MixedInvalid) {
  ColorSpace color_space;
  color_space = color_space.GetWithMatrixAndRange(ColorSpace::MatrixID::INVALID,
                                                  ColorSpace::RangeID::INVALID);
  EXPECT_TRUE(!color_space.IsValid());
  color_space = color_space.GetWithMatrixAndRange(
      ColorSpace::MatrixID::SMPTE170M, ColorSpace::RangeID::LIMITED);
  EXPECT_TRUE(!color_space.IsValid());
}

TEST(ColorSpace, MixedSRGBWithRec601) {
  const ColorSpace expected_color_space = ColorSpace(
      ColorSpace::PrimaryID::BT709, ColorSpace::TransferID::IEC61966_2_1,
      ColorSpace::MatrixID::SMPTE170M, ColorSpace::RangeID::LIMITED);
  ColorSpace color_space = ColorSpace::CreateSRGB();
  color_space = color_space.GetWithMatrixAndRange(
      ColorSpace::MatrixID::SMPTE170M, ColorSpace::RangeID::LIMITED);
  EXPECT_TRUE(expected_color_space.IsValid());
  EXPECT_EQ(color_space, expected_color_space);
}

TEST(ColorSpace, MixedHDR10WithRec709) {
  const ColorSpace expected_color_space = ColorSpace(
      ColorSpace::PrimaryID::BT2020, ColorSpace::TransferID::SMPTEST2084,
      ColorSpace::MatrixID::BT709, ColorSpace::RangeID::LIMITED);
  ColorSpace color_space = ColorSpace::CreateHDR10();
  color_space = color_space.GetWithMatrixAndRange(ColorSpace::MatrixID::BT709,
                                                  ColorSpace::RangeID::LIMITED);
  EXPECT_TRUE(expected_color_space.IsValid());
  EXPECT_EQ(color_space, expected_color_space);
}

TEST(ColorSpace, GetsPrimariesTransferMatrixAndRange) {
  ColorSpace color_space(
      ColorSpace::PrimaryID::BT709, ColorSpace::TransferID::BT709,
      ColorSpace::MatrixID::BT709, ColorSpace::RangeID::LIMITED);
  EXPECT_EQ(color_space.GetPrimaryID(), ColorSpace::PrimaryID::BT709);
  EXPECT_EQ(color_space.GetTransferID(), ColorSpace::TransferID::BT709);
  EXPECT_EQ(color_space.GetMatrixID(), ColorSpace::MatrixID::BT709);
  EXPECT_EQ(color_space.GetRangeID(), ColorSpace::RangeID::LIMITED);
}

TEST(ColorSpace, PQWhiteLevel) {
  constexpr float kCustomWhiteLevel = 200.f;

  ColorSpace color_space = ColorSpace::CreateHDR10(kCustomWhiteLevel);
  EXPECT_EQ(color_space.GetTransferID(), ColorSpace::TransferID::SMPTEST2084);
  float sdr_white_level;
  EXPECT_TRUE(color_space.GetSDRWhiteLevel(&sdr_white_level));
  EXPECT_EQ(sdr_white_level, kCustomWhiteLevel);

  color_space = ColorSpace::CreateHDR10();
  EXPECT_EQ(color_space.GetTransferID(), ColorSpace::TransferID::SMPTEST2084);
  EXPECT_TRUE(color_space.GetSDRWhiteLevel(&sdr_white_level));
  EXPECT_EQ(sdr_white_level, ColorSpace::kDefaultSDRWhiteLevel);

  color_space = color_space.GetWithSDRWhiteLevel(kCustomWhiteLevel);
  EXPECT_EQ(color_space.GetTransferID(), ColorSpace::TransferID::SMPTEST2084);
  EXPECT_TRUE(color_space.GetSDRWhiteLevel(&sdr_white_level));
  EXPECT_EQ(sdr_white_level, kCustomWhiteLevel);

  constexpr float kCustomWhiteLevel2 = kCustomWhiteLevel * 2;
  color_space = color_space.GetWithSDRWhiteLevel(kCustomWhiteLevel2);
  EXPECT_EQ(color_space.GetTransferID(), ColorSpace::TransferID::SMPTEST2084);
  EXPECT_TRUE(color_space.GetSDRWhiteLevel(&sdr_white_level));
  EXPECT_EQ(sdr_white_level, kCustomWhiteLevel2);
}

TEST(ColorSpace, LinearHDRWhiteLevel) {
  constexpr float kCustomWhiteLevel = 200.f;
  constexpr float kCustomSlope =
      ColorSpace::kDefaultScrgbLinearSdrWhiteLevel / kCustomWhiteLevel;

  ColorSpace color_space = ColorSpace::CreateSCRGBLinear(kCustomWhiteLevel);
  skcms_TransferFunction fn;
  EXPECT_EQ(color_space.GetTransferID(), ColorSpace::TransferID::CUSTOM_HDR);
  EXPECT_TRUE(color_space.GetTransferFunction(&fn));
  EXPECT_EQ(std::make_tuple(fn.g, fn.a, fn.b, fn.c, fn.d, fn.e, fn.f),
            std::make_tuple(1.f, kCustomSlope, 0.f, 0.f, 0.f, 0.f, 0.f));

  color_space = ColorSpace::CreateSCRGBLinear();
  EXPECT_EQ(color_space.GetTransferID(), ColorSpace::TransferID::LINEAR_HDR);
  EXPECT_TRUE(color_space.GetTransferFunction(&fn));
  EXPECT_EQ(std::make_tuple(fn.g, fn.a, fn.b, fn.c, fn.d, fn.e, fn.f),
            std::make_tuple(1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f));

  color_space = color_space.GetWithSDRWhiteLevel(kCustomWhiteLevel);
  EXPECT_EQ(color_space.GetTransferID(), ColorSpace::TransferID::CUSTOM_HDR);
  EXPECT_TRUE(color_space.GetTransferFunction(&fn));
  EXPECT_EQ(std::make_tuple(fn.g, fn.a, fn.b, fn.c, fn.d, fn.e, fn.f),
            std::make_tuple(1.f, kCustomSlope, 0.f, 0.f, 0.f, 0.f, 0.f));
}

TEST(ColorSpace, ExpectationsMatchSRGB) {
  ColorSpace::PrimaryID primary_ids[] = {
      ColorSpace::PrimaryID::BT709,
      ColorSpace::PrimaryID::BT470M,
      ColorSpace::PrimaryID::BT470BG,
      ColorSpace::PrimaryID::SMPTE170M,
      ColorSpace::PrimaryID::SMPTE240M,
      ColorSpace::PrimaryID::FILM,
      ColorSpace::PrimaryID::BT2020,
      ColorSpace::PrimaryID::SMPTEST428_1,
      ColorSpace::PrimaryID::SMPTEST431_2,
      ColorSpace::PrimaryID::SMPTEST432_1,
      ColorSpace::PrimaryID::XYZ_D50,
      ColorSpace::PrimaryID::ADOBE_RGB,
      ColorSpace::PrimaryID::APPLE_GENERIC_RGB,
      ColorSpace::PrimaryID::WIDE_GAMUT_COLOR_SPIN,
  };

  // Create a custom color space with the sRGB primary matrix.
  ColorSpace srgb = ColorSpace::CreateSRGB();
  skcms_Matrix3x3 to_XYZD50;
  srgb.GetPrimaryMatrix(&to_XYZD50);
  ColorSpace custom_srgb =
      ColorSpace::CreateCustom(to_XYZD50, ColorSpace::TransferID::IEC61966_2_1);

  for (auto id : primary_ids) {
    ColorSpace color_space(id, ColorSpace::TransferID::IEC61966_2_1);
    // The precomputed results for Contains(sRGB) should match the calculation
    // performed on a custom color space with sRGB primaries.
    EXPECT_EQ(color_space.Contains(srgb), color_space.Contains(custom_srgb));
  }
}

}  // namespace
}  // namespace gfx
