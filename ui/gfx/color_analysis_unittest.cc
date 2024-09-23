// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_analysis.h"

#include <stddef.h>
#include <stdint.h>

#include <exception>
#include <vector>

#include "base/functional/bind.h"
#include "skia/ext/platform_canvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"

namespace color_utils {

const unsigned char k1x1White[] = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
  0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
  0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53,
  0xde, 0x00, 0x00, 0x00, 0x01, 0x73, 0x52, 0x47,
  0x42, 0x00, 0xae, 0xce, 0x1c, 0xe9, 0x00, 0x00,
  0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00,
  0x0b, 0x13, 0x00, 0x00, 0x0b, 0x13, 0x01, 0x00,
  0x9a, 0x9c, 0x18, 0x00, 0x00, 0x00, 0x07, 0x74,
  0x49, 0x4d, 0x45, 0x07, 0xdb, 0x02, 0x11, 0x15,
  0x16, 0x1b, 0xaa, 0x58, 0x38, 0x76, 0x00, 0x00,
  0x00, 0x19, 0x74, 0x45, 0x58, 0x74, 0x43, 0x6f,
  0x6d, 0x6d, 0x65, 0x6e, 0x74, 0x00, 0x43, 0x72,
  0x65, 0x61, 0x74, 0x65, 0x64, 0x20, 0x77, 0x69,
  0x74, 0x68, 0x20, 0x47, 0x49, 0x4d, 0x50, 0x57,
  0x81, 0x0e, 0x17, 0x00, 0x00, 0x00, 0x0c, 0x49,
  0x44, 0x41, 0x54, 0x08, 0xd7, 0x63, 0xf8, 0xff,
  0xff, 0x3f, 0x00, 0x05, 0xfe, 0x02, 0xfe, 0xdc,
  0xcc, 0x59, 0xe7, 0x00, 0x00, 0x00, 0x00, 0x49,
  0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};

const unsigned char k1x3BlueWhite[] = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
  0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03,
  0x08, 0x02, 0x00, 0x00, 0x00, 0xdd, 0xbf, 0xf2,
  0xd5, 0x00, 0x00, 0x00, 0x01, 0x73, 0x52, 0x47,
  0x42, 0x00, 0xae, 0xce, 0x1c, 0xe9, 0x00, 0x00,
  0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00,
  0x0b, 0x13, 0x00, 0x00, 0x0b, 0x13, 0x01, 0x00,
  0x9a, 0x9c, 0x18, 0x00, 0x00, 0x00, 0x07, 0x74,
  0x49, 0x4d, 0x45, 0x07, 0xdb, 0x02, 0x12, 0x01,
  0x0a, 0x2c, 0xfd, 0x08, 0x64, 0x66, 0x00, 0x00,
  0x00, 0x19, 0x74, 0x45, 0x58, 0x74, 0x43, 0x6f,
  0x6d, 0x6d, 0x65, 0x6e, 0x74, 0x00, 0x43, 0x72,
  0x65, 0x61, 0x74, 0x65, 0x64, 0x20, 0x77, 0x69,
  0x74, 0x68, 0x20, 0x47, 0x49, 0x4d, 0x50, 0x57,
  0x81, 0x0e, 0x17, 0x00, 0x00, 0x00, 0x14, 0x49,
  0x44, 0x41, 0x54, 0x08, 0xd7, 0x63, 0xf8, 0xff,
  0xff, 0x3f, 0x13, 0x03, 0x03, 0x03, 0x03, 0x03,
  0xc3, 0x7f, 0x00, 0x1e, 0xfd, 0x03, 0xff, 0xde,
  0x72, 0x58, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x49,
  0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};

const unsigned char k1x3BlueRed[] = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
  0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03,
  0x08, 0x02, 0x00, 0x00, 0x00, 0xdd, 0xbf, 0xf2,
  0xd5, 0x00, 0x00, 0x00, 0x01, 0x73, 0x52, 0x47,
  0x42, 0x00, 0xae, 0xce, 0x1c, 0xe9, 0x00, 0x00,
  0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00,
  0x0b, 0x13, 0x00, 0x00, 0x0b, 0x13, 0x01, 0x00,
  0x9a, 0x9c, 0x18, 0x00, 0x00, 0x00, 0x07, 0x74,
  0x49, 0x4d, 0x45, 0x07, 0xdb, 0x02, 0x12, 0x01,
  0x07, 0x09, 0x03, 0xa2, 0xce, 0x6c, 0x00, 0x00,
  0x00, 0x19, 0x74, 0x45, 0x58, 0x74, 0x43, 0x6f,
  0x6d, 0x6d, 0x65, 0x6e, 0x74, 0x00, 0x43, 0x72,
  0x65, 0x61, 0x74, 0x65, 0x64, 0x20, 0x77, 0x69,
  0x74, 0x68, 0x20, 0x47, 0x49, 0x4d, 0x50, 0x57,
  0x81, 0x0e, 0x17, 0x00, 0x00, 0x00, 0x14, 0x49,
  0x44, 0x41, 0x54, 0x08, 0xd7, 0x63, 0xf8, 0xcf,
  0xc0, 0xc0, 0xc4, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
  0xf0, 0x1f, 0x00, 0x0c, 0x10, 0x02, 0x01, 0x2c,
  0x8f, 0x8b, 0x8c, 0x00, 0x00, 0x00, 0x00, 0x49,
  0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};

const HSL kDefaultLowerBound = {-1, -1, 0.15};
const HSL kDefaultUpperBound = {-1, -1, 0.85};

// Creates a 1-dimensional png of the pixel colors found in |colors|.
scoped_refptr<base::RefCountedMemory> CreateTestPNG(
    const std::vector<SkColor>& colors) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(colors.size(), 1);

  for (size_t i = 0; i < colors.size(); ++i) {
    bitmap.eraseArea(SkIRect::MakeXYWH(i, 0, 1, 1), colors[i]);
  }
  return gfx::Image::CreateFrom1xBitmap(bitmap).As1xPNGBytes();
}

class MockKMeanImageSampler : public KMeanImageSampler {
 public:
  MockKMeanImageSampler() : current_result_index_(0) {
  }

  explicit MockKMeanImageSampler(const std::vector<int>& samples)
      : prebaked_sample_results_(samples),
        current_result_index_(0) {
  }

  ~MockKMeanImageSampler() override {}

  void AddSample(int sample) {
    prebaked_sample_results_.push_back(sample);
  }

  int GetSample(int width, int height) override {
    if (current_result_index_ >= prebaked_sample_results_.size()) {
      current_result_index_ = 0;
    }

    if (prebaked_sample_results_.empty()) {
      return 0;
    }

    return prebaked_sample_results_[current_result_index_++];
  }

 protected:
  std::vector<int> prebaked_sample_results_;
  size_t current_result_index_;
};

// Return true if a color channel is approximately equal to an expected value.
bool ChannelApproximatelyEqual(int expected, uint8_t channel) {
  return (abs(expected - static_cast<int>(channel)) <= 1);
}

class ColorAnalysisTest : public testing::Test {
};

TEST_F(ColorAnalysisTest, CalculatePNGKMeanAllWhite) {
  MockKMeanImageSampler test_sampler;
  test_sampler.AddSample(0);

  SkColor color = CalculateKMeanColorOfPNG(k1x1White, kDefaultLowerBound,
                                           kDefaultUpperBound, &test_sampler);

  EXPECT_EQ(color, SK_ColorWHITE);
}

TEST_F(ColorAnalysisTest, CalculatePNGKMeanIgnoreWhiteLightness) {
  MockKMeanImageSampler test_sampler;
  test_sampler.AddSample(0);
  test_sampler.AddSample(1);
  test_sampler.AddSample(2);

  SkColor color = CalculateKMeanColorOfPNG(k1x3BlueWhite, kDefaultLowerBound,
                                           kDefaultUpperBound, &test_sampler);

  EXPECT_EQ(SkColorSetARGB(0xFF, 0x00, 0x00, 0xFF), color);
}

TEST_F(ColorAnalysisTest, CalculatePNGKMeanPickMostCommon) {
  MockKMeanImageSampler test_sampler;
  test_sampler.AddSample(0);
  test_sampler.AddSample(1);
  test_sampler.AddSample(2);

  SkColor color = CalculateKMeanColorOfPNG(k1x3BlueRed, kDefaultLowerBound,
                                           kDefaultUpperBound, &test_sampler);

  EXPECT_EQ(SkColorSetARGB(0xFF, 0xFF, 0x00, 0x00), color);
}

TEST_F(ColorAnalysisTest, CalculatePNGKMeanIgnoreRedHue) {
  MockKMeanImageSampler test_sampler;
  test_sampler.AddSample(0);
  test_sampler.AddSample(1);
  test_sampler.AddSample(2);

  std::vector<SkColor> colors(4, SK_ColorRED);
  colors[1] = SK_ColorBLUE;

  scoped_refptr<base::RefCountedMemory> png = CreateTestPNG(colors);

  HSL lower = {0.2, -1, 0.15};
  HSL upper = {0.8, -1, 0.85};
  SkColor color = CalculateKMeanColorOfPNG(*png, lower, upper, &test_sampler);

  EXPECT_EQ(SK_ColorBLUE, color);
}

TEST_F(ColorAnalysisTest, CalculatePNGKMeanIgnoreGreySaturation) {
  MockKMeanImageSampler test_sampler;
  test_sampler.AddSample(0);
  test_sampler.AddSample(1);
  test_sampler.AddSample(2);

  std::vector<SkColor> colors(4, SK_ColorGRAY);
  colors[1] = SK_ColorBLUE;

  scoped_refptr<base::RefCountedMemory> png = CreateTestPNG(colors);
  HSL lower = {-1, 0.3, -1};
  HSL upper = {-1, 1, -1};
  SkColor color = CalculateKMeanColorOfPNG(*png, lower, upper, &test_sampler);

  EXPECT_EQ(SK_ColorBLUE, color);
}

TEST_F(ColorAnalysisTest, GridSampler) {
  GridSampler sampler;
  const int kWidth = 16;
  const int kHeight = 16;
  // Sample starts at 1,1.
  EXPECT_EQ(1 + 1 * kWidth, sampler.GetSample(kWidth, kHeight));
  EXPECT_EQ(1 + 4 * kWidth, sampler.GetSample(kWidth, kHeight));
  EXPECT_EQ(1 + 7 * kWidth, sampler.GetSample(kWidth, kHeight));
  EXPECT_EQ(1 + 10 * kWidth, sampler.GetSample(kWidth, kHeight));
  // Step over by 3.
  EXPECT_EQ(4 + 1 * kWidth, sampler.GetSample(kWidth, kHeight));
  EXPECT_EQ(4 + 4 * kWidth, sampler.GetSample(kWidth, kHeight));
  EXPECT_EQ(4 + 7 * kWidth, sampler.GetSample(kWidth, kHeight));
  EXPECT_EQ(4 + 10 * kWidth, sampler.GetSample(kWidth, kHeight));
}

TEST_F(ColorAnalysisTest, FindClosestColor) {
  // Empty image returns input color.
  SkColor color = FindClosestColor(base::span<uint8_t>(), 0, 0, SK_ColorRED);
  EXPECT_EQ(SK_ColorRED, color);

  // Single color image returns that color.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  bitmap.eraseColor(SK_ColorWHITE);
  // SAFETY: There's no Skia API to retrieve the bitmap pixels as a span.
  // TODO(https://crbug.com/354829279): Remove this if/when Skia gets span APIs.
  UNSAFE_BUFFERS(base::span<uint8_t> bitmap_span(
      static_cast<uint8_t*>(bitmap.getPixels()), bitmap.computeByteSize()));
  color = FindClosestColor(bitmap_span, bitmap.width(), bitmap.height(),
                           SK_ColorRED);
  EXPECT_EQ(SK_ColorWHITE, color);

  // Write a black pixel into the image. A dark grey input pixel should match
  // the black one in the image.
  uint32_t* pixel = bitmap.getAddr32(0, 0);
  *pixel = SK_ColorBLACK;
  color = FindClosestColor(bitmap_span, bitmap.width(), bitmap.height(),
                           SK_ColorDKGRAY);
  EXPECT_EQ(SK_ColorBLACK, color);
}

TEST_F(ColorAnalysisTest, CalculateKMeanColorOfBitmap) {
  // Create a 16x16 bitmap to represent a favicon.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  bitmap.eraseARGB(255, 100, 150, 200);

  SkColor color = CalculateKMeanColorOfBitmap(bitmap);
  EXPECT_EQ(255u, SkColorGetA(color));
  // Color values are not exactly equal due to reversal of premultiplied alpha.
  EXPECT_TRUE(ChannelApproximatelyEqual(100, SkColorGetR(color)));
  EXPECT_TRUE(ChannelApproximatelyEqual(150, SkColorGetG(color)));
  EXPECT_TRUE(ChannelApproximatelyEqual(200, SkColorGetB(color)));

  // Test a bitmap with an alpha channel.
  bitmap.eraseARGB(128, 100, 150, 200);
  color = CalculateKMeanColorOfBitmap(bitmap);

  // Alpha channel should be ignored for dominant color calculation.
  EXPECT_EQ(255u, SkColorGetA(color));
  EXPECT_TRUE(ChannelApproximatelyEqual(100, SkColorGetR(color)));
  EXPECT_TRUE(ChannelApproximatelyEqual(150, SkColorGetG(color)));
  EXPECT_TRUE(ChannelApproximatelyEqual(200, SkColorGetB(color)));
}

// Regression test for heap-buffer-underflow. https://crbug.com/970343
TEST_F(ColorAnalysisTest, CalculateKMeanColorOfSmallImage) {
  SkBitmap bitmap;

  // Create a 1x41 bitmap, so it is not wide enough to have 1 pixel of padding
  // on both sides.
  bitmap.allocN32Pixels(1, 41);
  bitmap.eraseARGB(255, 100, 150, 200);

  SkColor color = CalculateKMeanColorOfBitmap(bitmap);
  EXPECT_EQ(255u, SkColorGetA(color));
  EXPECT_TRUE(ChannelApproximatelyEqual(100, SkColorGetR(color)));
  EXPECT_TRUE(ChannelApproximatelyEqual(150, SkColorGetG(color)));
  EXPECT_TRUE(ChannelApproximatelyEqual(200, SkColorGetB(color)));

  // Test a wide but narrow bitmap.
  bitmap.allocN32Pixels(41, 1);
  bitmap.eraseARGB(255, 100, 150, 200);
  color = CalculateKMeanColorOfBitmap(bitmap);
  EXPECT_EQ(255u, SkColorGetA(color));
  EXPECT_TRUE(ChannelApproximatelyEqual(100, SkColorGetR(color)));
  EXPECT_TRUE(ChannelApproximatelyEqual(150, SkColorGetG(color)));
  EXPECT_TRUE(ChannelApproximatelyEqual(200, SkColorGetB(color)));

  // Test a tiny bitmap.
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseARGB(255, 100, 150, 200);
  color = CalculateKMeanColorOfBitmap(bitmap);
  EXPECT_EQ(255u, SkColorGetA(color));
  EXPECT_TRUE(ChannelApproximatelyEqual(100, SkColorGetR(color)));
  EXPECT_TRUE(ChannelApproximatelyEqual(150, SkColorGetG(color)));
  EXPECT_TRUE(ChannelApproximatelyEqual(200, SkColorGetB(color)));
}

TEST_F(ColorAnalysisTest, ComputeProminentColors) {
  LumaRange lumas[] = {LumaRange::DARK, LumaRange::NORMAL, LumaRange::LIGHT};
  SaturationRange saturations[] = {SaturationRange::VIBRANT,
                                   SaturationRange::MUTED};
  std::vector<ColorProfile> color_profiles;
  for (auto s : saturations) {
    for (auto l : lumas)
      color_profiles.emplace_back(l, s);
  }

  // A totally dark gray image, which yields no prominent color as it's too
  // close to black.
  gfx::Canvas canvas(gfx::Size(300, 200), 1.0f, true);
  canvas.FillRect(gfx::Rect(0, 0, 300, 200), SkColorSetRGB(10, 10, 10));
  SkBitmap bitmap = canvas.GetBitmap();

  // All expectations start at SK_ColorTRANSPARENT (i.e. 0).
  std::vector<Swatch> expectations(color_profiles.size(),
                                   Swatch(SK_ColorTRANSPARENT, 0));
  std::vector<Swatch> computations = CalculateProminentColorsOfBitmap(
      bitmap, color_profiles, nullptr /* region */, ColorSwatchFilter());
  EXPECT_EQ(expectations, computations);

  // Add a green that could hit a couple values.
  const SkColor kVibrantGreen = SkColorSetRGB(25, 200, 25);
  canvas.FillRect(gfx::Rect(0, 1, 300, 1), kVibrantGreen);
  bitmap = canvas.GetBitmap();
  expectations[0] = Swatch(kVibrantGreen, 50);
  expectations[1] = Swatch(kVibrantGreen, 50);
  computations = CalculateProminentColorsOfBitmap(
      bitmap, color_profiles, nullptr /* region */, ColorSwatchFilter());
  EXPECT_EQ(expectations, computations);

  // Add a stripe of a dark, muted green (saturation .33, luma .29).
  const SkColor kDarkGreen = SkColorSetRGB(50, 100, 50);
  canvas.FillRect(gfx::Rect(0, 2, 300, 1), kDarkGreen);
  bitmap = canvas.GetBitmap();
  expectations[3] = Swatch(kDarkGreen, 50);
  computations = CalculateProminentColorsOfBitmap(
      bitmap, color_profiles, nullptr /* region */, ColorSwatchFilter());
  EXPECT_EQ(expectations, computations);

  // Now draw a little bit of pure green. That should be closer to the goal for
  // normal vibrant, but is out of range for other color profiles.
  const SkColor kPureGreen = SkColorSetRGB(0, 255, 0);
  canvas.FillRect(gfx::Rect(0, 3, 300, 1), kPureGreen);
  bitmap = canvas.GetBitmap();
  expectations[1] = Swatch(kPureGreen, 50);
  computations = CalculateProminentColorsOfBitmap(
      bitmap, color_profiles, nullptr /* region */, ColorSwatchFilter());
  EXPECT_EQ(expectations, computations);
}

TEST_F(ColorAnalysisTest, ComputeColorSwatches) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorMAGENTA);
  bitmap.erase(SK_ColorGREEN, {10, 10, 90, 90});
  bitmap.erase(SK_ColorYELLOW, {40, 40, 60, 60});

  const Swatch kYellowSwatch = Swatch(SK_ColorYELLOW, (20u * 20u));
  const Swatch kGreenSwatch =
      Swatch(SK_ColorGREEN, (80u * 80u) - kYellowSwatch.population);
  const Swatch kMagentaSwatch =
      Swatch(SK_ColorMAGENTA, (100u * 100u) - kGreenSwatch.population -
                                  kYellowSwatch.population);

  {
    std::vector<Swatch> colors =
        CalculateColorSwatches(bitmap, 10, gfx::Rect(100, 100), std::nullopt);
    EXPECT_EQ(3u, colors.size());
    EXPECT_EQ(kGreenSwatch, colors[0]);
    EXPECT_EQ(kMagentaSwatch, colors[1]);
    EXPECT_EQ(kYellowSwatch, colors[2]);
  }

  {
    std::vector<Swatch> colors = CalculateColorSwatches(
        bitmap, 10, gfx::Rect(10, 10, 80, 80), std::nullopt);
    EXPECT_EQ(2u, colors.size());
    EXPECT_EQ(kGreenSwatch, colors[0]);
    EXPECT_EQ(kYellowSwatch, colors[1]);
  }
}

TEST_F(ColorAnalysisTest, ComputeColorSwatches_MaxConsideredPixels) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(200, 200);
  bitmap.eraseColor(SK_ColorMAGENTA);
  bitmap.erase(SK_ColorGREEN, {0, 0, 160, 160});
  bitmap.erase(SK_ColorYELLOW, {80, 80, 120, 120});

  {
    std::vector<Swatch> colors =
        CalculateColorSwatches(bitmap, 10, gfx::Rect(200, 200), std::nullopt);

    size_t total_population = 0;
    for (auto& color : colors) {
      total_population += color.population;
    }
    EXPECT_EQ(static_cast<size_t>(kMaxConsideredPixelsForSwatches),
              total_population);
  }

  {
    std::vector<Swatch> colors =
        CalculateColorSwatches(bitmap, 10, gfx::Rect(200, 200), std::nullopt);
    EXPECT_EQ(3u, colors.size());
    EXPECT_EQ(SK_ColorGREEN, colors[0].color);
    EXPECT_NEAR(0.6,
                static_cast<float>(colors[0].population) /
                    kMaxConsideredPixelsForSwatches,
                0.001);
    EXPECT_EQ(SK_ColorMAGENTA, colors[1].color);
    EXPECT_NEAR(0.36,
                static_cast<float>(colors[1].population) /
                    kMaxConsideredPixelsForSwatches,
                0.001);
    EXPECT_EQ(SK_ColorYELLOW, colors[2].color);
    EXPECT_NEAR(0.04,
                static_cast<float>(colors[2].population) /
                    kMaxConsideredPixelsForSwatches,
                0.001);
  }

  {
    std::vector<Swatch> colors = CalculateColorSwatches(
        bitmap, 10, gfx::Rect(20, 20, 140, 140), std::nullopt);

    size_t total_population = 0;
    for (auto& color : colors) {
      total_population += color.population;
    }
    EXPECT_EQ(static_cast<size_t>(kMaxConsideredPixelsForSwatches),
              total_population);
    EXPECT_EQ(2u, colors.size());
    EXPECT_EQ(SK_ColorGREEN, colors[0].color);
    EXPECT_NEAR(0.918,
                static_cast<float>(colors[0].population) /
                    kMaxConsideredPixelsForSwatches,
                0.001);
    EXPECT_EQ(SK_ColorYELLOW, colors[1].color);
    EXPECT_NEAR(0.082,
                static_cast<float>(colors[1].population) /
                    kMaxConsideredPixelsForSwatches,
                0.001);
  }
}

TEST_F(ColorAnalysisTest, ComputeColorSwatches_Filter) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorMAGENTA);
  bitmap.erase(SK_ColorBLACK, {10, 10, 90, 90});
  bitmap.erase(SK_ColorWHITE, {40, 40, 60, 60});

  const Swatch kWhiteSwatch = Swatch(SK_ColorWHITE, (20u * 20u));
  const Swatch kBlackSwatch =
      Swatch(SK_ColorBLACK, (80u * 80u) - kWhiteSwatch.population);
  const Swatch kMagentaSwatch =
      Swatch(SK_ColorMAGENTA,
             (100u * 100u) - kBlackSwatch.population - kWhiteSwatch.population);

  {
    std::vector<Swatch> colors = CalculateColorSwatches(
        bitmap, 10, gfx::Rect(100, 100),
        base::BindRepeating([](const SkColor& candidate) {
          return candidate != SK_ColorBLACK;
        }));
    EXPECT_EQ(2u, colors.size());
    EXPECT_EQ(kMagentaSwatch, colors[0]);
    EXPECT_EQ(kWhiteSwatch, colors[1]);
  }

  {
    std::vector<Swatch> colors =
        CalculateColorSwatches(bitmap, 10, gfx::Rect(100, 100), std::nullopt);
    EXPECT_EQ(3u, colors.size());
    EXPECT_EQ(kBlackSwatch, colors[0]);
    EXPECT_EQ(kMagentaSwatch, colors[1]);
    EXPECT_EQ(kWhiteSwatch, colors[2]);
  }
}

}  // namespace color_utils
