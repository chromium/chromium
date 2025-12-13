// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_space.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <tuple>

#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/skia_color_space_util.h"

namespace gfx {
namespace {

// Returns the L-infty difference of u and v.
float Diff(const SkV4& u, const SkV4& v) {
  return std::max({std::abs(u.x - v.x), std::abs(u.y - v.y),
                   std::abs(u.z - v.z), std::abs(u.w - v.w)});
}

TEST(ColorSpace, RGBToYUV) {
  const float kEpsilon = 1.0e-3f;
  const size_t kNumTestRGBs = 3;
  std::array<SkV4, kNumTestRGBs> test_rgbs = {{
      {1.f, 0.f, 0.f, 1.f},
      {0.f, 1.f, 0.f, 1.f},
      {0.f, 0.f, 1.f, 1.f},
  }};

  const size_t kNumColorSpaces = 4;
  std::array<gfx::ColorSpace, kNumColorSpaces> color_spaces = {
      gfx::ColorSpace::CreateREC601(),
      gfx::ColorSpace::CreateREC709(),
      gfx::ColorSpace::CreateJpeg(),
      gfx::ColorSpace::CreateXYZD50(),
  };

  SkV4 expected_yuvs[kNumColorSpaces][kNumTestRGBs] = {
      // REC601
      {
          {0.3195f, 0.3518f, 0.9392f, 1.0000f},
          {0.5669f, 0.2090f, 0.1322f, 1.0000f},
          {0.1607f, 0.9392f, 0.4286f, 1.0000f},
      },
      // REC709
      {
          {0.2453f, 0.3994f, 0.9392f, 1.0000f},
          {0.6770f, 0.1614f, 0.1011f, 1.0000f},
          {0.1248f, 0.9392f, 0.4597f, 1.0000f},
      },
      // Jpeg
      {
          {0.2990f, 0.3313f, 1.0000f, 1.0000f},
          {0.5870f, 0.1687f, 0.0813f, 1.0000f},
          {0.1140f, 1.0000f, 0.4187f, 1.0000f},
      },
      // XYZD50
      {
          {1.0000f, 0.0000f, 0.0000f, 1.0000f},
          {0.0000f, 1.0000f, 0.0000f, 1.0000f},
          {0.0000f, 0.0000f, 1.0000f, 1.0000f},
      },
  };

  for (size_t i = 0; i < kNumColorSpaces; ++i) {
    SkM44 transfer = color_spaces[i].GetTransferMatrix(/*bit_depth=*/8);

    SkM44 range_adjust = color_spaces[i].GetRangeAdjustMatrix(/*bit_depth=*/8);

    SkM44 range_adjust_inv;
    EXPECT_TRUE(range_adjust.invert(&range_adjust_inv));

    for (size_t j = 0; j < kNumTestRGBs; ++j) {
      SkV4 yuv = range_adjust_inv * transfer * test_rgbs[j];
      UNSAFE_TODO(EXPECT_LT(Diff(yuv, expected_yuvs[i][j]), kEpsilon));
    }
  }
}

TEST(ColorSpace, RangeAdjust) {
  const float kEpsilon = 1.0e-3f;
  const size_t kNumTestYUVs = 2;
  std::array<SkV4, kNumTestYUVs> test_yuvs = {{
      {1.f, 1.f, 1.f, 1.f},
      {0.f, 0.f, 0.f, 1.f},
  }};

  const size_t kNumBitDepths = 3;
  std::array<int, kNumBitDepths> bit_depths = {8, 10, 12};

  const size_t kNumColorSpaces = 3;
  std::array<ColorSpace, kNumColorSpaces> color_spaces = {
      ColorSpace::CreateREC601(),
      ColorSpace::CreateJpeg(),
      ColorSpace(ColorSpace::PrimaryID::INVALID,
                 ColorSpace::TransferID::INVALID, ColorSpace::MatrixID::YCOCG,
                 ColorSpace::RangeID::LIMITED),
  };

  SkV4 expected_yuvs[kNumColorSpaces][kNumBitDepths][kNumTestYUVs] = {
      // REC601
      {
          // 8bpc
          {
              {235.f / 255.f, 239.5f / 255.f, 239.5f / 255.f, 1.0000f},
              {16.f / 255.f, 15.5f / 255.f, 15.5f / 255.f, 1.0000f},
          },
          // 10bpc
          {
              {940.f / 1023.f, 959.5f / 1023.f, 959.5f / 1023.f, 1.0000f},
              {64.f / 1023.f, 63.5f / 1023.f, 63.5f / 1023.f, 1.0000f},
          },
          // 12bpc
          {
              {3760.f / 4095.f, 3839.5f / 4095.f, 3839.5f / 4095.f, 1.0000f},
              {256.f / 4095.f, 255.5f / 4095.f, 255.5f / 4095.f, 1.0000f},
          },
      },
      // Jpeg
      {
          // 8bpc
          {
              {1.0000f, 1.0000f, 1.0000f, 1.0000f},
              {0.0000f, 0.0000f, 0.0000f, 1.0000f},
          },
          // 10bpc
          {
              {1.0000f, 1.0000f, 1.0000f, 1.0000f},
              {0.0000f, 0.0000f, 0.0000f, 1.0000f},
          },
          // 12bpc
          {
              {1.0000f, 1.0000f, 1.0000f, 1.0000f},
              {0.0000f, 0.0000f, 0.0000f, 1.0000f},
          },
      },
      // YCoCg
      {
          // 8bpc
          {
              {235.f / 255.f, 235.f / 255.f, 235.f / 255.f, 1.0000f},
              {16.f / 255.f, 16.f / 255.f, 16.f / 255.f, 1.0000f},
          },
          // 10bpc
          {
              {940.f / 1023.f, 940.f / 1023.f, 940.f / 1023.f, 1.0000f},
              {64.f / 1023.f, 64.f / 1023.f, 64.f / 1023.f, 1.0000f},
          },
          // 12bpc
          {
              {3760.f / 4095.f, 3760.f / 4095.f, 3760.f / 4095.f, 1.0000f},
              {256.f / 4095.f, 256.f / 4095.f, 256.f / 4095.f, 1.0000f},
          },
      },
  };

  for (size_t i = 0; i < kNumColorSpaces; ++i) {
    for (size_t j = 0; j < kNumBitDepths; ++j) {
      SkM44 range_adjust = color_spaces[i].GetRangeAdjustMatrix(bit_depths[j]);

      SkM44 range_adjust_inv;
      EXPECT_TRUE(range_adjust.invert(&range_adjust_inv));

      for (size_t k = 0; k < kNumTestYUVs; ++k) {
        SkV4 yuv = range_adjust_inv * test_yuvs[k];
        UNSAFE_TODO(EXPECT_LT(Diff(yuv, expected_yuvs[i][j][k]), kEpsilon));
      }
    }
  }
}

TEST(ColorSpace, Blending) {
  ColorSpace display_color_space;

  // A linear transfer function being used for HDR should be blended using an
  // sRGB-like transfer function.
  display_color_space = ColorSpace::CreateSRGBLinear();
  EXPECT_FALSE(display_color_space.IsSuitableForBlending());

  // If not used for HDR, a linear transfer function should be left unchanged.
  display_color_space = ColorSpace::CreateXYZD50();
  EXPECT_TRUE(display_color_space.IsSuitableForBlending());
}

TEST(ColorSpace, ConversionToAndFromSkColorSpace) {
  skcms_Matrix3x3 primary_matrix = {{
      {0.205276f, 0.625671f, 0.060867f},
      {0.149185f, 0.063217f, 0.744553f},
      {0.609741f, 0.311111f, 0.019470f},
  }};
  skcms_TransferFunction transfer_fn = {2.1f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};

  auto color_spaces = std::to_array<ColorSpace>({
      ColorSpace(ColorSpace::PrimaryID::BT709, ColorSpace::TransferID::SRGB),
      ColorSpace(ColorSpace::PrimaryID::ADOBE_RGB,
                 ColorSpace::TransferID::SRGB),
      ColorSpace(ColorSpace::PrimaryID::P3, ColorSpace::TransferID::LINEAR),
      ColorSpace(ColorSpace::PrimaryID::BT2020, ColorSpace::TransferID::SRGB),
      ColorSpace::CreateCustom(primary_matrix, transfer_fn),
      // HDR
      ColorSpace::CreateSRGBLinear(),
  });
  auto sk_color_spaces = std::to_array<sk_sp<SkColorSpace>>({
      SkColorSpace::MakeSRGB(),
      SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB, SkNamedGamut::kAdobeRGB),
      SkColorSpace::MakeRGB(SkNamedTransferFn::kLinear,
                            SkNamedGamut::kDisplayP3),
      SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB, SkNamedGamut::kRec2020),
      SkColorSpace::MakeRGB(transfer_fn, primary_matrix),
      // HDR
      SkColorSpace::MakeSRGBLinear(),
  });

  static_assert(std::size(color_spaces) == std::size(sk_color_spaces), "");

  // Test that converting from ColorSpace to SkColorSpace is producing an
  // equivalent representation.
  for (size_t i = 0; i < std::size(color_spaces); ++i) {
    EXPECT_TRUE(SkColorSpace::Equals(color_spaces[i].ToSkColorSpace().get(),
                                     sk_color_spaces[i].get()))
        << " on iteration i = " << i;
  }

  // Invariant test: Test that converting a SkColorSpace to a ColorSpace is
  // producing an equivalent representation; and then converting the converted
  // ColorSpace back to SkColorSpace is also producing an equivalent
  // representation.
  for (size_t i = 0; i < std::size(color_spaces); ++i) {
    const ColorSpace from_sk_color_space(*sk_color_spaces[i],
                                         color_spaces[i].IsHDR());
    EXPECT_EQ(color_spaces[i], from_sk_color_space);
    EXPECT_TRUE(SkColorSpace::Equals(
        sk_color_spaces[i].get(), from_sk_color_space.ToSkColorSpace().get()));
  }
}

TEST(ColorSpace, PQAndHLGToSkColorSpace) {
  const auto hlg = ColorSpace::CreateHLG();
  const auto pq = ColorSpace::CreateHDR10();

  constexpr size_t kNumCases = 3;
  std::array<float, kNumCases> pq_nits = {
      100,
      203,
      500,
  };

  for (size_t i = 0; i < kNumCases; ++i) {
    const float sdr_white_level = pq_nits[i];
    sk_sp<SkColorSpace> sk_hlg = hlg.ToSkColorSpace(sdr_white_level);
    sk_sp<SkColorSpace> sk_pq = pq.ToSkColorSpace(sdr_white_level);

    // The SDR white level parameter should get put into a parameter for the PQ
    // and HLG transfer function.
    skcms_TransferFunction pq_fn = {0};
    sk_pq->transferFn(&pq_fn);
    EXPECT_EQ(pq_fn.a, sdr_white_level);

    skcms_TransferFunction hlg_fn = {0};
    sk_hlg->transferFn(&hlg_fn);
    EXPECT_EQ(hlg_fn.a, sdr_white_level);
  }
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
  const ColorSpace expected_color_space =
      ColorSpace(ColorSpace::PrimaryID::BT709, ColorSpace::TransferID::SRGB,
                 ColorSpace::MatrixID::SMPTE170M, ColorSpace::RangeID::LIMITED);
  ColorSpace color_space = ColorSpace::CreateSRGB();
  color_space = color_space.GetWithMatrixAndRange(
      ColorSpace::MatrixID::SMPTE170M, ColorSpace::RangeID::LIMITED);
  EXPECT_TRUE(expected_color_space.IsValid());
  EXPECT_EQ(color_space, expected_color_space);
}

TEST(ColorSpace, MixedHDR10WithRec709) {
  const ColorSpace expected_color_space =
      ColorSpace(ColorSpace::PrimaryID::BT2020, ColorSpace::TransferID::PQ,
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
      ColorSpace::PrimaryID::P3,
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
      ColorSpace::CreateCustom(to_XYZD50, ColorSpace::TransferID::SRGB);

  for (auto id : primary_ids) {
    ColorSpace color_space(id, ColorSpace::TransferID::SRGB);
    // The precomputed results for Contains(sRGB) should match the calculation
    // performed on a custom color space with sRGB primaries.
    EXPECT_EQ(color_space.Contains(srgb), color_space.Contains(custom_srgb));
  }
}

TEST(ColorSpaceUtil, SkcmsMatrixConvert) {
  skcms_Matrix3x3 in_m33 = SkNamedGamut::kSRGB;
  SkM44 m44 = SkM44FromSkcmsMatrix3x3(in_m33);
  skcms_Matrix3x3 out_m33 = SkcmsMatrix3x3FromSkM44(m44);
  UNSAFE_TODO(EXPECT_EQ(memcmp(&in_m33, &out_m33, sizeof(in_m33)), 0));
}

TEST(ColorSpace, AsHDR) {
  ColorSpace cs;
  skcms_TransferFunction fn;
  constexpr float kEpsilon = 0.00001f;

  cs = ColorSpace(ColorSpace::PrimaryID::P3, ColorSpace::TransferID::SRGB);
  cs = cs.GetAsHDR();
  EXPECT_EQ(cs.GetTransferID(), ColorSpace::TransferID::SRGB_HDR);

  cs = ColorSpace(ColorSpace::PrimaryID::P3, ColorSpace::TransferID::LINEAR);
  cs = cs.GetAsHDR();
  EXPECT_EQ(cs.GetTransferID(), ColorSpace::TransferID::LINEAR_HDR);

  cs = cs.GetWithTransferFunction(ColorSpace::TransferID::GAMMA22);
  EXPECT_FALSE(cs.IsHDR());
  cs = cs.GetAsHDR();
  EXPECT_EQ(cs.GetTransferID(), ColorSpace::TransferID::CUSTOM_HDR);
  EXPECT_TRUE(cs.GetTransferFunction(&fn));
  EXPECT_NEAR(fn.g, 2.2, kEpsilon);

  fn.a = 0.5;
  fn.g = 2.5;
  cs = cs.GetWithTransferFunction(fn, /*is_hdr=*/true);
  EXPECT_EQ(cs.GetTransferID(), ColorSpace::TransferID::CUSTOM_HDR);
  EXPECT_TRUE(cs.GetTransferFunction(&fn));
  EXPECT_NEAR(fn.a, 0.5, kEpsilon);
  EXPECT_NEAR(fn.g, 2.5, kEpsilon);
}

}  // namespace
}  // namespace gfx
