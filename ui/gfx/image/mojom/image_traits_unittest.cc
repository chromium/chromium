// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/image/mojom/image.mojom.h"
#include "ui/gfx/image/mojom/image_skia_mojom_traits.h"

namespace gfx {

namespace {

// A test ImageSkiaSource that creates an ImageSkiaRep for any scale.
class TestImageSkiaSource : public ImageSkiaSource {
 public:
  explicit TestImageSkiaSource(const Size& dip_size) : dip_size_(dip_size) {}

  TestImageSkiaSource(const TestImageSkiaSource&) = delete;
  TestImageSkiaSource& operator=(const TestImageSkiaSource&) = delete;

  ~TestImageSkiaSource() override = default;

  // ImageSkiaSource:
  ImageSkiaRep GetImageForScale(float scale) override {
    return ImageSkiaRep(ScaleToCeiledSize(dip_size_, scale), scale);
  }

 private:
  const Size dip_size_;
};

// A helper to construct a skia.mojom.BitmapN32 without using StructTraits
// to bypass checks on the sending/serialization side.
mojo::StructPtr<mojom::ImageSkiaRep> ConstructImageSkiaRep(
    const SkBitmap& bitmap,
    float scale) {
  auto mojom_rep = mojom::ImageSkiaRep::New();
  mojom_rep->bitmap = bitmap;
  mojom_rep->scale = scale;
  return mojom_rep;
}

}  // namespace

TEST(ImageTraitsTest, VerifyMojomConstruction) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  mojo::StructPtr<mojom::ImageSkiaRep> input = ConstructImageSkiaRep(bitmap, 1);
  ImageSkiaRep output;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::ImageSkiaRep>(input, output));
}

TEST(ImageTraitsTest, BadColorTypeImageSkiaRep_Deserialize) {
  SkBitmap a8_bitmap;
  a8_bitmap.allocPixels(SkImageInfo::MakeA8(1, 1));

  mojo::StructPtr<mojom::ImageSkiaRep> input =
      ConstructImageSkiaRep(a8_bitmap, 1);
  ImageSkiaRep output;
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::ImageSkiaRep>(input, output));
}

TEST(ImageTraitsTest, EmptyImageSkiaRep_Deserialize) {
  SkBitmap empty_bitmap;
  empty_bitmap.allocN32Pixels(0, 0);
  // Empty SkBitmap is not null.
  EXPECT_FALSE(empty_bitmap.isNull());
  EXPECT_TRUE(empty_bitmap.drawsNothing());

  mojo::StructPtr<mojom::ImageSkiaRep> input =
      ConstructImageSkiaRep(empty_bitmap, 1);
  ImageSkiaRep output;
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::ImageSkiaRep>(input, output));
}

TEST(ImageTraitsTest, ValidImageSkiaRep) {
  ImageSkiaRep image_rep(Size(2, 4), 2.0f);

  ImageSkiaRep output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::ImageSkiaRep>(
      image_rep, output));

  EXPECT_FALSE(output.is_null());
  EXPECT_EQ(image_rep.scale(), output.scale());
  EXPECT_TRUE(test::AreBitmapsEqual(image_rep.GetBitmap(), output.GetBitmap()));
}

TEST(ImageTraitsTest, UnscaledImageSkiaRep) {
  ImageSkiaRep image_rep(Size(2, 4), 0.0f);
  ASSERT_TRUE(image_rep.unscaled());

  ImageSkiaRep output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::ImageSkiaRep>(
      image_rep, output));
  EXPECT_TRUE(output.unscaled());
  EXPECT_TRUE(test::AreBitmapsEqual(image_rep.GetBitmap(), output.GetBitmap()));
}

TEST(ImageTraitsTest, NullImageSkia) {
  ImageSkia null_image;
  ASSERT_TRUE(null_image.isNull());

  ImageSkia output(ImageSkiaRep(Size(1, 1), 1.0f));
  ASSERT_FALSE(output.isNull());
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::ImageSkia>(null_image,
                                                                    output));
  EXPECT_TRUE(output.isNull());
}

TEST(ImageTraitsTest, ImageSkiaRepsAreCreatedAsNeeded) {
  const Size kSize(1, 2);
  ImageSkia image(std::make_unique<TestImageSkiaSource>(kSize), kSize);
  EXPECT_FALSE(image.isNull());
  EXPECT_TRUE(image.image_reps().empty());

  ImageSkia output;
  EXPECT_TRUE(output.isNull());
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::ImageSkia>(image, output));
  EXPECT_FALSE(image.image_reps().empty());
  EXPECT_FALSE(output.isNull());
}

TEST(ImageTraitsTest, ImageSkia) {
  const Size kSize(1, 2);
  ImageSkia image(std::make_unique<TestImageSkiaSource>(kSize), kSize);
  image.GetRepresentation(1.0f);
  image.GetRepresentation(2.0f);

  ImageSkia output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::ImageSkia>(image, output));

  EXPECT_TRUE(test::AreImagesEqual(Image(output), Image(image)));
}

TEST(ImageTraitsTest, ImageSkiaWithOperations) {
  const Size kSize(32, 32);
  ImageSkia image(std::make_unique<TestImageSkiaSource>(kSize), kSize);

  const Size kNewSize(16, 16);
  ImageSkia resized = ImageSkiaOperations::CreateResizedImage(
      image, skia::ImageOperations::RESIZE_BEST, kNewSize);
  resized.GetRepresentation(1.0f);
  resized.GetRepresentation(2.0f);

  ImageSkia output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::ImageSkia>(resized, output));

  EXPECT_TRUE(test::AreImagesEqual(Image(output), Image(resized)));
}

}  // namespace gfx
