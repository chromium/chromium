// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <Cocoa/Cocoa.h>
#include <stddef.h>

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_png_rep.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace {

// Returns true if the structure of |ns_image| matches the structure
// described by |width|, |height|, and |scales|.
// The structure matches if:
// - |ns_image| is not nil.
// - |ns_image| has NSImageReps of |scales|.
// - Each of the NSImageReps has a pixel size of [|ns_image| size] *
//   scale.
bool NSImageStructureMatches(
    NSImage* ns_image,
    int width,
    int height,
    const std::vector<float>& scales) {
  if (!ns_image || ns_image.size.width != width ||
      ns_image.size.height != height ||
      ns_image.representations.count != scales.size()) {
    return false;
  }

  for (float scale : scales) {
    bool found_match = false;
    for (size_t j = 0; j < ns_image.representations.count; ++j) {
      NSImageRep* ns_image_rep = [ns_image representations][j];
      if (ns_image_rep && ns_image_rep.pixelsWide == width * scale &&
          ns_image_rep.pixelsHigh == height * scale) {
        found_match = true;
        break;
      }
    }
    if (!found_match)
      return false;
  }
  return true;
}

NSBitmapImageRep* BitmapImageRep(int width, int height) {
  NSBitmapImageRep* image_rep =
      [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nullptr
                                              pixelsWide:width
                                              pixelsHigh:height
                                           bitsPerSample:8
                                         samplesPerPixel:3
                                                hasAlpha:NO
                                                isPlanar:NO
                                          colorSpaceName:NSDeviceRGBColorSpace
                                            bitmapFormat:0
                                             bytesPerRow:0
                                            bitsPerPixel:0];
  unsigned char* image_rep_data = image_rep.bitmapData;
  for (int i = 0; i < width * height * 3; ++i) {
    image_rep_data[i] = 255;
  }

  return image_rep;
}

class ImageMacTest : public testing::Test {
 public:
  ImageMacTest() = default;
  ImageMacTest(const ImageMacTest&) = delete;
  ImageMacTest& operator=(const ImageMacTest&) = delete;
  ~ImageMacTest() override = default;

 private:
  ui::test::ScopedSetSupportedResourceScaleFactors supported_scale_factors_{
      {ui::k100Percent, ui::k200Percent}};
};

namespace gt = gfx::test;

TEST_F(ImageMacTest, MultiResolutionNSImageToImageSkia) {
  const int kWidth1x = 10;
  const int kHeight1x = 12;
  const int kWidth2x = 20;
  const int kHeight2x = 24;

  NSBitmapImageRep* ns_image_rep1 = BitmapImageRep(kWidth1x, kHeight1x);
  NSBitmapImageRep* ns_image_rep2 = BitmapImageRep(kWidth2x, kHeight2x);
  NSImage* ns_image =
      [[NSImage alloc] initWithSize:NSMakeSize(kWidth1x, kHeight1x)];
  [ns_image addRepresentation:ns_image_rep1];
  [ns_image addRepresentation:ns_image_rep2];

  gfx::Image image(ns_image);

  EXPECT_EQ(1u, image.RepresentationCount());

  const gfx::ImageSkia* image_skia = image.ToImageSkia();

  std::vector<float> scales;
  scales.push_back(1.0f);
  scales.push_back(2.0f);
  EXPECT_TRUE(gt::ImageSkiaStructureMatches(*image_skia, kWidth1x, kHeight1x,
                                            scales));

  // ToImageSkia should create a second representation.
  EXPECT_EQ(2u, image.RepresentationCount());
}

// Test that converting to an ImageSkia from an NSImage with scale factors other
// than 1x and 2x results in an ImageSkia with scale factors 1x and 2x;
TEST_F(ImageMacTest, UnalignedMultiResolutionNSImageToImageSkia) {
  const int kWidth1x = 10;
  const int kHeight1x= 12;
  const int kWidth4x = 40;
  const int kHeight4x = 48;

  NSBitmapImageRep* ns_image_rep4 = BitmapImageRep(kWidth4x, kHeight4x);
  NSImage* ns_image =
      [[NSImage alloc] initWithSize:NSMakeSize(kWidth1x, kHeight1x)];
  [ns_image addRepresentation:ns_image_rep4];

  gfx::Image image(ns_image);

  EXPECT_EQ(1u, image.RepresentationCount());

  const gfx::ImageSkia* image_skia = image.ToImageSkia();

  std::vector<float> scales;
  scales.push_back(1.0f);
  scales.push_back(2.0f);
  EXPECT_TRUE(gt::ImageSkiaStructureMatches(*image_skia, kWidth1x, kHeight1x,
                                            scales));

  // ToImageSkia should create a second representation.
  EXPECT_EQ(2u, image.RepresentationCount());
}

TEST_F(ImageMacTest, MultiResolutionImageSkiaToNSImage) {
  const int kWidth1x = 10;
  const int kHeight1x= 12;
  const int kWidth2x = 20;
  const int kHeight2x = 24;

  gfx::ImageSkia image_skia;
  image_skia.AddRepresentation(gfx::ImageSkiaRep(
      gt::CreateBitmap(kWidth1x, kHeight1x), 1.0f));
  image_skia.AddRepresentation(gfx::ImageSkiaRep(
      gt::CreateBitmap(kWidth2x, kHeight2x), 2.0f));

  gfx::Image image(image_skia);

  EXPECT_EQ(1u, image.RepresentationCount());
  EXPECT_EQ(2u, image.ToImageSkia()->image_reps().size());

  NSImage* ns_image = image.ToNSImage();

  std::vector<float> scales;
  scales.push_back(1.0f);
  scales.push_back(2.0f);
  EXPECT_TRUE(NSImageStructureMatches(ns_image, kWidth1x, kHeight1x, scales));

  // Request for NSImage* should create a second representation.
  EXPECT_EQ(2u, image.RepresentationCount());
}

TEST_F(ImageMacTest, MultiResolutionPNGToNSImage) {
  const int kSize1x = 25;
  const int kSize2x = 50;

  scoped_refptr<base::RefCountedMemory> bytes1x = gt::CreatePNGBytes(kSize1x);
  scoped_refptr<base::RefCountedMemory> bytes2x = gt::CreatePNGBytes(kSize2x);
  std::vector<gfx::ImagePNGRep> image_png_reps;
  image_png_reps.emplace_back(bytes1x, 1.0f);
  image_png_reps.emplace_back(bytes2x, 2.0f);

  gfx::Image image(image_png_reps);

  NSImage* ns_image = image.ToNSImage();
  std::vector<float> scales;
  scales.push_back(1.0f);
  scales.push_back(2.0f);
  EXPECT_TRUE(NSImageStructureMatches(ns_image, kSize1x, kSize1x, scales));

  // Converting from PNG to NSImage should not go through ImageSkia.
  EXPECT_FALSE(image.HasRepresentation(gfx::Image::kImageRepSkia));

  // Convert to ImageSkia to check pixel contents of NSImageReps.
  gfx::ImageSkia image_skia = gfx::ImageSkiaFromNSImage(ns_image);
  EXPECT_TRUE(gt::ArePNGBytesCloseToBitmap(
      *bytes1x, image_skia.GetRepresentation(1.0f).GetBitmap(),
      gt::MaxColorSpaceConversionColorShift()));
  EXPECT_TRUE(gt::ArePNGBytesCloseToBitmap(
      *bytes2x, image_skia.GetRepresentation(2.0f).GetBitmap(),
      gt::MaxColorSpaceConversionColorShift()));
}

} // namespace
