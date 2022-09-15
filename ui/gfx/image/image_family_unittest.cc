// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_family.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace {

namespace gt = gfx::test;

// Tests that |image| != NULL, and has the given width and height.
// This is a macro instead of a function, so that the correct line numbers are
// reported when a test fails.
#define EXPECT_IMAGE_NON_NULL_AND_SIZE(image, expected_width, expected_height) \
do { \
  const gfx::Image* image_ = image; \
  EXPECT_TRUE(image_); \
  EXPECT_EQ(expected_width, image_->Width()); \
  EXPECT_EQ(expected_height, image_->Height()); \
} while(0)

// Tests that |image| has the given width and height.
// This is a macro instead of a function, so that the correct line numbers are
// reported when a test fails.
#define EXPECT_IMAGE_SIZE(image, expected_width, expected_height) \
do { \
  const gfx::Image& image_ = image; \
  EXPECT_FALSE(image_.IsEmpty()); \
  EXPECT_EQ(expected_width, image_.Width()); \
  EXPECT_EQ(expected_height, image_.Height()); \
} while(0)

class ImageFamilyTest : public testing::Test {
 public:
  // Construct an ImageFamily. Implicitly tests Add and Empty.
  void SetUp() override {
    EXPECT_TRUE(image_family_.empty());

    // Aspect ratio 1:1.
    image_family_.Add(gt::CreateImageSkia(32, 32));
    EXPECT_FALSE(image_family_.empty());
    image_family_.Add(gt::CreateImageSkia(16, 16));
    image_family_.Add(gt::CreateImageSkia(64, 64));
    // Duplicate (should override previous one).
    // Insert an Image directly, instead of an ImageSkia.
    gfx::Image image(gt::CreateImageSkia(32, 32));
    image_family_.Add(image);
    // Aspect ratio 1:4.
    image_family_.Add(gt::CreateImageSkia(3, 12));
    image_family_.Add(gt::CreateImageSkia(12, 48));
    // Aspect ratio 4:1.
    image_family_.Add(gt::CreateImageSkia(512, 128));
    image_family_.Add(gt::CreateImageSkia(256, 64));

    EXPECT_FALSE(image_family_.empty());
  }

  gfx::ImageFamily image_family_;
};

TEST_F(ImageFamilyTest, Clear) {
  image_family_.clear();
  EXPECT_TRUE(image_family_.empty());
}

TEST_F(ImageFamilyTest, MoveConstructor) {
  gfx::ImageFamily family(std::move(image_family_));
  EXPECT_TRUE(image_family_.empty());
  EXPECT_FALSE(family.empty());
}

TEST_F(ImageFamilyTest, MoveAssignment) {
  gfx::ImageFamily family;
  EXPECT_TRUE(family.empty());
  family = std::move(image_family_);
  EXPECT_TRUE(image_family_.empty());
  EXPECT_FALSE(family.empty());
}

TEST_F(ImageFamilyTest, Clone) {
  gfx::ImageFamily family = image_family_.Clone();
  EXPECT_FALSE(image_family_.empty());
  EXPECT_FALSE(family.empty());
}

// Tests iteration over an ImageFamily.
TEST_F(ImageFamilyTest, Iteration) {
  gfx::ImageFamily::const_iterator it = image_family_.begin();
  gfx::ImageFamily::const_iterator end = image_family_.end();

  // Expect iteration in order of aspect ratio (from thinnest to widest), then
  // size.
  EXPECT_TRUE(it != end);
  EXPECT_EQ(gfx::Size(3, 12), it->Size());
  ++it;
  EXPECT_TRUE(it != end);
  EXPECT_EQ(gfx::Size(12, 48), it->Size());
  it++;   // Test post-increment.
  EXPECT_TRUE(it != end);
  EXPECT_EQ(gfx::Size(16, 16), it->Size());
  ++it;
  EXPECT_TRUE(it != end);
  EXPECT_EQ(gfx::Size(32, 32), it->Size());
  --it;   // Test decrement
  EXPECT_TRUE(it != end);
  EXPECT_EQ(gfx::Size(16, 16), it->Size());
  ++it;
  ++it;
  EXPECT_TRUE(it != end);
  EXPECT_EQ(gfx::Size(64, 64), (*it).Size());   // Test operator*.
  ++it;
  EXPECT_TRUE(it != end);
  EXPECT_EQ(gfx::Size(256, 64), it->Size());
  ++it;
  EXPECT_TRUE(it != end);
  EXPECT_EQ(gfx::Size(512, 128), it->Size());
  ++it;

  EXPECT_TRUE(it == end);
}

TEST_F(ImageFamilyTest, GetBest) {
  // Get on an empty family.
  gfx::ImageFamily empty_family;
  EXPECT_TRUE(empty_family.empty());
  EXPECT_FALSE(empty_family.GetBest(32, 32));
  EXPECT_FALSE(empty_family.GetBest(0, 32));
  EXPECT_FALSE(empty_family.GetBest(32, 0));

  // Get various aspect ratios and sizes on the sample family.

  // 0x0 (expect the smallest square image).
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(0, 0), 16, 16);
  // GetBest(0, N) or GetBest(N, 0) should be treated the same as GetBest(0, 0).
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(0, 16), 16, 16);
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(0, 64), 16, 16);
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(16, 0), 16, 16);
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(64, 0), 16, 16);

  // Thinner than thinnest image.
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(2, 12), 3, 12);
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(2, 13), 12, 48);
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(10, 60), 12, 48);

  // Between two images' aspect ratio.
  // Note: Testing the boundary around 1:2 and 2:1, half way to 1:4 and 4:1.
  // Ties are broken by favouring the thinner aspect ratio.
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(63, 32), 64, 64);
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(64, 32), 64, 64);
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(65, 32), 256, 64);
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(32, 63), 64, 64);
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(32, 64), 12, 48);
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(32, 65), 12, 48);

  // Exact match aspect ratio.
  // Exact match size.
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(32, 32), 32, 32);
  // Slightly smaller.
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(31, 31), 32, 32);
  // Much smaller.
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(17, 17), 32, 32);
  // Exact match size.
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(16, 16), 16, 16);
  // Smaller than any image.
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(3, 3), 16, 16);
  // Larger than any image.
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(512, 512), 64, 64);
  // 1:4 aspect ratio.
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(16, 64), 12, 48);
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(2, 8), 3, 12);
  // 4:1 aspect ratio.
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(64, 16), 256, 64);
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(260, 65), 512, 128);

  // Wider than widest image.
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(255, 51), 256, 64);
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(260, 52), 512, 128);
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(654, 129), 512, 128);
}

TEST_F(ImageFamilyTest, CreateExact) {
  // CreateExact on an empty family.
  gfx::ImageFamily empty_family;
  EXPECT_TRUE(empty_family.empty());
  EXPECT_TRUE(empty_family.CreateExact(32, 32).IsEmpty());
  EXPECT_TRUE(empty_family.CreateExact(0, 32).IsEmpty());
  EXPECT_TRUE(empty_family.CreateExact(32, 0).IsEmpty());

  // CreateExact on a family with only empty images results in an empty image,
  // despite the requested image size.
  gfx::ImageFamily family_with_empty_image;
  family_with_empty_image.Add(gfx::Image());
  EXPECT_FALSE(family_with_empty_image.empty());
  EXPECT_TRUE(family_with_empty_image.CreateExact(32, 32).IsEmpty());

  // CreateExact on various aspect ratios and sizes on the sample family.

  // Targeting an image with width and/or height of 0 results in empty image.
  EXPECT_TRUE(image_family_.CreateExact(0, 0).IsEmpty());
  EXPECT_TRUE(image_family_.CreateExact(0, 64).IsEmpty());
  EXPECT_TRUE(image_family_.CreateExact(64, 0).IsEmpty());

  // Thinner than thinnest image.
  EXPECT_IMAGE_SIZE(image_family_.CreateExact(2, 12), 2, 12);

  // Exact match aspect ratio.
  // Exact match size.
  EXPECT_IMAGE_SIZE(image_family_.CreateExact(32, 32), 32, 32);
  // Much smaller.
  EXPECT_IMAGE_SIZE(image_family_.CreateExact(17, 17), 17, 17);
  // Smaller than any image.
  EXPECT_IMAGE_SIZE(image_family_.CreateExact(3, 3), 3, 3);
  // Larger than any image.
  EXPECT_IMAGE_SIZE(image_family_.CreateExact(512, 512), 512, 512);

  // Wider than widest image.
  EXPECT_IMAGE_SIZE(image_family_.CreateExact(255, 51), 255, 51);
}

// Test adding and looking up images with 0 width and height.
TEST_F(ImageFamilyTest, ZeroWidthAndHeight) {
  // An empty Image. Should be considered to have 0 width and height.
  image_family_.Add(gfx::Image());
  // Images with 0 width OR height should be treated the same as an image with 0
  // width AND height (in fact, the ImageSkias should be indistinguishable).
  image_family_.Add(gt::CreateImageSkia(32, 0));
  image_family_.Add(gt::CreateImageSkia(0, 32));

  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(0, 0), 0, 0);
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(1, 1), 16, 16);
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(32, 32), 32, 32);

  // GetBest(0, N) or GetBest(N, 0) should be treated the same as GetBest(0, 0).
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(0, 1), 0, 0);
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(0, 32), 0, 0);
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(1, 0), 0, 0);
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(32, 0), 0, 0);

  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(1, 32), 12, 48);
  EXPECT_IMAGE_NON_NULL_AND_SIZE(image_family_.GetBest(32, 1), 256, 64);
}

}  // namespace
