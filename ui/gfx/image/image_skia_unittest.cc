// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_skia.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/simple_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/switches.h"

namespace gfx {

namespace {

class FixedSource : public ImageSkiaSource {
 public:
  explicit FixedSource(const ImageSkiaRep& image) : image_(image) {}

  ~FixedSource() override {}

  ImageSkiaRep GetImageForScale(float scale) override { return image_; }

 private:
  ImageSkiaRep image_;

  DISALLOW_COPY_AND_ASSIGN(FixedSource);
};

class FixedScaleSource : public ImageSkiaSource {
 public:
  explicit FixedScaleSource(const ImageSkiaRep& image) : image_(image) {}

  ~FixedScaleSource() override {}

  ImageSkiaRep GetImageForScale(float scale) override {
    if (!image_.unscaled() && image_.scale() != scale)
      return ImageSkiaRep();
    return image_;
  }

 private:
  ImageSkiaRep image_;

  DISALLOW_COPY_AND_ASSIGN(FixedScaleSource);
};

class DynamicSource : public ImageSkiaSource {
 public:
  explicit DynamicSource(const gfx::Size& size)
      : size_(size),
        last_requested_scale_(0.0f) {}

  ~DynamicSource() override {}

  ImageSkiaRep GetImageForScale(float scale) override {
    last_requested_scale_ = scale;
    return gfx::ImageSkiaRep(size_, scale);
  }

  float GetLastRequestedScaleAndReset() {
    float result = last_requested_scale_;
    last_requested_scale_ = 0.0f;
    return result;
  }

 private:
  gfx::Size size_;
  float last_requested_scale_;

  DISALLOW_COPY_AND_ASSIGN(DynamicSource);
};

class NullSource: public ImageSkiaSource {
 public:
  NullSource() {
  }

  ~NullSource() override {}

  ImageSkiaRep GetImageForScale(float scale) override {
    return gfx::ImageSkiaRep();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NullSource);
};

}  // namespace

namespace test {
class TestOnThread : public base::SimpleThread {
 public:
  explicit TestOnThread(ImageSkia* image_skia)
      : SimpleThread("image_skia_on_thread"),
        image_skia_(image_skia),
        can_read_(false),
        can_modify_(false) {
  }

  void Run() override {
    can_read_ = image_skia_->CanRead();
    can_modify_ = image_skia_->CanModify();
    if (can_read_)
      image_skia_->image_reps();
  }

  void StartAndJoin() {
    Start();
    Join();
  }

  bool can_read() const { return can_read_; }

  bool can_modify() const { return can_modify_; }

 private:
  ImageSkia* image_skia_;

  bool can_read_;
  bool can_modify_;

  DISALLOW_COPY_AND_ASSIGN(TestOnThread);
};

}  // namespace test

class ImageSkiaTest : public testing::Test {
 public:
  ImageSkiaTest() {
    // In the test, we assume that we support 1.0f and 2.0f DSFs.
    old_scales_ = ImageSkia::GetSupportedScales();

    // Sets the list of scale factors supported by resource bundle.
    std::vector<float> supported_scales;
    supported_scales.push_back(1.0f);
    supported_scales.push_back(2.0f);
    ImageSkia::SetSupportedScales(supported_scales);
  }
  ~ImageSkiaTest() override { ImageSkia::SetSupportedScales(old_scales_); }

 private:
  std::vector<float> old_scales_;
  DISALLOW_COPY_AND_ASSIGN(ImageSkiaTest);
};

TEST_F(ImageSkiaTest, FixedSource) {
  ImageSkiaRep image(Size(100, 200), 0.0f);
  ImageSkia image_skia(std::make_unique<FixedSource>(image), Size(100, 200));
  EXPECT_EQ(0U, image_skia.image_reps().size());

  const ImageSkiaRep& result_100p = image_skia.GetRepresentation(1.0f);
  EXPECT_EQ(100, result_100p.GetWidth());
  EXPECT_EQ(200, result_100p.GetHeight());
  EXPECT_EQ(1.0f, result_100p.scale());
  EXPECT_EQ(1U, image_skia.image_reps().size());

  const ImageSkiaRep& result_200p = image_skia.GetRepresentation(2.0f);

  EXPECT_EQ(100, result_200p.GetWidth());
  EXPECT_EQ(200, result_200p.GetHeight());
  EXPECT_EQ(100, result_200p.pixel_width());
  EXPECT_EQ(200, result_200p.pixel_height());
  EXPECT_EQ(1.0f, result_200p.scale());
  EXPECT_EQ(1U, image_skia.image_reps().size());

  const ImageSkiaRep& result_300p = image_skia.GetRepresentation(3.0f);

  EXPECT_EQ(100, result_300p.GetWidth());
  EXPECT_EQ(200, result_300p.GetHeight());
  EXPECT_EQ(100, result_300p.pixel_width());
  EXPECT_EQ(200, result_300p.pixel_height());
  EXPECT_EQ(1.0f, result_300p.scale());
  EXPECT_EQ(1U, image_skia.image_reps().size());

  // Get the representation again and make sure it doesn't
  // generate new image skia rep.
  image_skia.GetRepresentation(1.0f);
  image_skia.GetRepresentation(2.0f);
  image_skia.GetRepresentation(3.0f);
  EXPECT_EQ(1U, image_skia.image_reps().size());
}

TEST_F(ImageSkiaTest, FixedScaledSource) {
  ImageSkiaRep image(Size(100, 200), 1.0f);
  ImageSkia image_skia(std::make_unique<FixedScaleSource>(image),
                       Size(100, 200));
  EXPECT_EQ(0U, image_skia.image_reps().size());

  const ImageSkiaRep& result_100p = image_skia.GetRepresentation(1.0f);
  EXPECT_EQ(100, result_100p.GetWidth());
  EXPECT_EQ(200, result_100p.GetHeight());
  EXPECT_EQ(1.0f, result_100p.scale());
  EXPECT_EQ(1U, image_skia.image_reps().size());

  // 2.0f data doesn't exist, then it falls back to 1.0f and rescale it.
  const ImageSkiaRep& result_200p = image_skia.GetRepresentation(2.0f);

  EXPECT_EQ(100, result_200p.GetWidth());
  EXPECT_EQ(200, result_200p.GetHeight());
  EXPECT_EQ(200, result_200p.pixel_width());
  EXPECT_EQ(400, result_200p.pixel_height());
  EXPECT_EQ(2.0f, result_200p.scale());
  EXPECT_EQ(2U, image_skia.image_reps().size());

  // Get the representation again and make sure it doesn't
  // generate new image skia rep.
  image_skia.GetRepresentation(1.0f);
  image_skia.GetRepresentation(2.0f);
  EXPECT_EQ(2U, image_skia.image_reps().size());
}

TEST_F(ImageSkiaTest, FixedUnscaledSource) {
  ImageSkiaRep image(Size(100, 200), 0.0f);
  ImageSkia image_skia(std::make_unique<FixedScaleSource>(image),
                       Size(100, 200));
  EXPECT_EQ(0U, image_skia.image_reps().size());

  const ImageSkiaRep& result_100p = image_skia.GetRepresentation(1.0f);
  EXPECT_EQ(100, result_100p.pixel_width());
  EXPECT_EQ(200, result_100p.pixel_height());
  EXPECT_TRUE(result_100p.unscaled());
  EXPECT_EQ(1U, image_skia.image_reps().size());

  // 2.0f data doesn't exist, but unscaled ImageSkiaRep shouldn't be rescaled.
  const ImageSkiaRep& result_200p = image_skia.GetRepresentation(2.0f);

  EXPECT_EQ(100, result_200p.pixel_width());
  EXPECT_EQ(200, result_200p.pixel_height());
  EXPECT_TRUE(result_200p.unscaled());
  EXPECT_EQ(1U, image_skia.image_reps().size());

  // Get the representation again and make sure it doesn't
  // generate new image skia rep.
  image_skia.GetRepresentation(1.0f);
  image_skia.GetRepresentation(2.0f);
  EXPECT_EQ(1U, image_skia.image_reps().size());
}

TEST_F(ImageSkiaTest, DynamicSource) {
  ImageSkia image_skia(std::make_unique<DynamicSource>(Size(100, 200)),
                       Size(100, 200));
  EXPECT_EQ(0U, image_skia.image_reps().size());
  const ImageSkiaRep& result_100p = image_skia.GetRepresentation(1.0f);
  EXPECT_EQ(100, result_100p.GetWidth());
  EXPECT_EQ(200, result_100p.GetHeight());
  EXPECT_EQ(1.0f, result_100p.scale());
  EXPECT_EQ(1U, image_skia.image_reps().size());

  const ImageSkiaRep& result_200p =
      image_skia.GetRepresentation(2.0f);
  EXPECT_EQ(100, result_200p.GetWidth());
  EXPECT_EQ(200, result_200p.GetHeight());
  EXPECT_EQ(200, result_200p.pixel_width());
  EXPECT_EQ(400, result_200p.pixel_height());
  EXPECT_EQ(2.0f, result_200p.scale());
  EXPECT_EQ(2U, image_skia.image_reps().size());

  // Get the representation again and make sure it doesn't
  // generate new image skia rep.
  image_skia.GetRepresentation(1.0f);
  EXPECT_EQ(2U, image_skia.image_reps().size());
  image_skia.GetRepresentation(2.0f);
  EXPECT_EQ(2U, image_skia.image_reps().size());
}

// Tests that image_reps returns all of the representations in the
// image when there are multiple representations for a scale factor.
// This currently is the case with ImageLoader::LoadImages.
TEST_F(ImageSkiaTest, ManyRepsPerScaleFactor) {
  const int kSmallIcon1x = 16;
  const int kSmallIcon2x = 32;
  const int kLargeIcon1x = 32;

  ImageSkia image(std::make_unique<NullSource>(),
                  gfx::Size(kSmallIcon1x, kSmallIcon1x));
  // Simulate a source which loads images on a delay. Upon
  // GetImageForScaleFactor, it immediately returns null and starts loading
  // image reps slowly.
  image.GetRepresentation(1.0f);
  image.GetRepresentation(2.0f);

  // After a lengthy amount of simulated time, finally loaded image reps.
  image.AddRepresentation(ImageSkiaRep(
      gfx::Size(kSmallIcon1x, kSmallIcon1x), 1.0f));
  image.AddRepresentation(ImageSkiaRep(
      gfx::Size(kSmallIcon2x, kSmallIcon2x), 2.0f));
  image.AddRepresentation(ImageSkiaRep(
      gfx::Size(kLargeIcon1x, kLargeIcon1x), 1.0f));

  std::vector<ImageSkiaRep> image_reps = image.image_reps();
  EXPECT_EQ(3u, image_reps.size());

  int num_1x = 0;
  int num_2x = 0;
  for (size_t i = 0; i < image_reps.size(); ++i) {
    if (image_reps[i].scale() == 1.0f)
      num_1x++;
    else if (image_reps[i].scale() == 2.0f)
      num_2x++;
  }
  EXPECT_EQ(2, num_1x);
  EXPECT_EQ(1, num_2x);
}

TEST_F(ImageSkiaTest, GetBitmap) {
  ImageSkia image_skia(std::make_unique<DynamicSource>(Size(100, 200)),
                       Size(100, 200));
  const SkBitmap* bitmap = image_skia.bitmap();
  EXPECT_NE(static_cast<SkBitmap*>(NULL), bitmap);
  EXPECT_FALSE(bitmap->isNull());
}

TEST_F(ImageSkiaTest, GetBitmapFromEmpty) {
  // Create an image with 1 representation and remove it so the ImageSkiaStorage
  // is left with no representations.
  ImageSkia empty_image(ImageSkiaRep(Size(100, 200), 1.0f));
  ImageSkia empty_image_copy(empty_image);
  empty_image.RemoveRepresentation(1.0f);

  // Check that ImageSkia::bitmap() still returns a valid SkBitmap pointer for
  // the image and all its copies.
  const SkBitmap* bitmap = empty_image_copy.bitmap();
  ASSERT_NE(static_cast<SkBitmap*>(NULL), bitmap);
  EXPECT_TRUE(bitmap->isNull());
  EXPECT_TRUE(bitmap->empty());
}

TEST_F(ImageSkiaTest, BackedBySameObjectAs) {
  // Null images should all be backed by the same object (NULL).
  ImageSkia image;
  ImageSkia unrelated;
  EXPECT_TRUE(image.BackedBySameObjectAs(unrelated));

  image.AddRepresentation(gfx::ImageSkiaRep(gfx::Size(10, 10),
                                            1.0f));
  ImageSkia copy = image;
  copy.AddRepresentation(gfx::ImageSkiaRep(gfx::Size(10, 10),
                                           2.0f));
  unrelated.AddRepresentation(gfx::ImageSkiaRep(gfx::Size(10, 10),
                                                1.0f));
  EXPECT_TRUE(image.BackedBySameObjectAs(copy));
  EXPECT_FALSE(image.BackedBySameObjectAs(unrelated));
  EXPECT_FALSE(copy.BackedBySameObjectAs(unrelated));
}

#if DCHECK_IS_ON()
TEST_F(ImageSkiaTest, EmptyOnThreadTest) {
  ImageSkia empty;
  test::TestOnThread empty_on_thread(&empty);
  empty_on_thread.Start();
  empty_on_thread.Join();
  EXPECT_TRUE(empty_on_thread.can_read());
  EXPECT_TRUE(empty_on_thread.can_modify());
}

TEST_F(ImageSkiaTest, StaticOnThreadTest) {
  ImageSkia image(ImageSkiaRep(Size(100, 200), 1.0f));
  EXPECT_FALSE(image.IsThreadSafe());

  test::TestOnThread image_on_thread(&image);
  // an image that was never accessed on this thread can be
  // read by other thread.
  image_on_thread.StartAndJoin();
  EXPECT_TRUE(image_on_thread.can_read());
  EXPECT_TRUE(image_on_thread.can_modify());
  EXPECT_FALSE(image.CanRead());
  EXPECT_FALSE(image.CanModify());

  image.DetachStorageFromSequence();
  // An image is accessed by this thread,
  // so other thread cannot read/modify it.
  image.image_reps();
  test::TestOnThread image_on_thread2(&image);
  image_on_thread2.StartAndJoin();
  EXPECT_FALSE(image_on_thread2.can_read());
  EXPECT_FALSE(image_on_thread2.can_modify());
  EXPECT_TRUE(image.CanRead());
  EXPECT_TRUE(image.CanModify());

  image.DetachStorageFromSequence();
  ImageSkia deep_copy(image.DeepCopy());
  EXPECT_FALSE(deep_copy.IsThreadSafe());
  test::TestOnThread deepcopy_on_thread(&deep_copy);
  deepcopy_on_thread.StartAndJoin();
  EXPECT_TRUE(deepcopy_on_thread.can_read());
  EXPECT_TRUE(deepcopy_on_thread.can_modify());
  EXPECT_FALSE(deep_copy.CanRead());
  EXPECT_FALSE(deep_copy.CanModify());

  ImageSkia deep_copy2(image.DeepCopy());
  EXPECT_EQ(1U, deep_copy2.image_reps().size());
  // Access it from current thread so that it can't be
  // accessed from another thread.
  deep_copy2.image_reps();
  EXPECT_FALSE(deep_copy2.IsThreadSafe());
  test::TestOnThread deepcopy2_on_thread(&deep_copy2);
  deepcopy2_on_thread.StartAndJoin();
  EXPECT_FALSE(deepcopy2_on_thread.can_read());
  EXPECT_FALSE(deepcopy2_on_thread.can_modify());
  EXPECT_TRUE(deep_copy2.CanRead());
  EXPECT_TRUE(deep_copy2.CanModify());

  image.DetachStorageFromSequence();
  image.SetReadOnly();
  // A read-only ImageSkia with no source is thread safe.
  EXPECT_TRUE(image.IsThreadSafe());
  test::TestOnThread readonly_on_thread(&image);
  readonly_on_thread.StartAndJoin();
  EXPECT_TRUE(readonly_on_thread.can_read());
  EXPECT_FALSE(readonly_on_thread.can_modify());
  EXPECT_TRUE(image.CanRead());
  EXPECT_FALSE(image.CanModify());

  image.DetachStorageFromSequence();
  image.MakeThreadSafe();
  EXPECT_TRUE(image.IsThreadSafe());
  test::TestOnThread threadsafe_on_thread(&image);
  threadsafe_on_thread.StartAndJoin();
  EXPECT_TRUE(threadsafe_on_thread.can_read());
  EXPECT_FALSE(threadsafe_on_thread.can_modify());
  EXPECT_TRUE(image.CanRead());
  EXPECT_FALSE(image.CanModify());
}

TEST_F(ImageSkiaTest, SourceOnThreadTest) {
  ImageSkia image(std::make_unique<DynamicSource>(Size(100, 200)),
                  Size(100, 200));
  EXPECT_FALSE(image.IsThreadSafe());

  test::TestOnThread image_on_thread(&image);
  image_on_thread.StartAndJoin();
  // an image that was never accessed on this thread can be
  // read by other thread.
  EXPECT_TRUE(image_on_thread.can_read());
  EXPECT_TRUE(image_on_thread.can_modify());
  EXPECT_FALSE(image.CanRead());
  EXPECT_FALSE(image.CanModify());

  image.DetachStorageFromSequence();
  // An image is accessed by this thread,
  // so other thread cannot read/modify it.
  image.image_reps();
  test::TestOnThread image_on_thread2(&image);
  image_on_thread2.StartAndJoin();
  EXPECT_FALSE(image_on_thread2.can_read());
  EXPECT_FALSE(image_on_thread2.can_modify());
  EXPECT_TRUE(image.CanRead());
  EXPECT_TRUE(image.CanModify());

  image.DetachStorageFromSequence();
  image.SetReadOnly();
  EXPECT_FALSE(image.IsThreadSafe());
  test::TestOnThread readonly_on_thread(&image);
  readonly_on_thread.StartAndJoin();
  EXPECT_TRUE(readonly_on_thread.can_read());
  EXPECT_FALSE(readonly_on_thread.can_modify());
  EXPECT_FALSE(image.CanRead());
  EXPECT_FALSE(image.CanModify());

  image.DetachStorageFromSequence();
  image.MakeThreadSafe();
  EXPECT_TRUE(image.IsThreadSafe());
  // Check if image reps are generated for supported scale factors.
  EXPECT_EQ(ImageSkia::GetSupportedScales().size(),
           image.image_reps().size());
  test::TestOnThread threadsafe_on_thread(&image);
  threadsafe_on_thread.StartAndJoin();
  EXPECT_TRUE(threadsafe_on_thread.can_read());
  EXPECT_FALSE(threadsafe_on_thread.can_modify());
  EXPECT_TRUE(image.CanRead());
  EXPECT_FALSE(image.CanModify());
}
#endif  // DCHECK_IS_ON()

TEST_F(ImageSkiaTest, Unscaled) {
  SkBitmap bitmap;

  // An ImageSkia created with 1x bitmap is unscaled.
  ImageSkia image_skia = ImageSkia::CreateFrom1xBitmap(bitmap);
  EXPECT_TRUE(image_skia.GetRepresentation(1.0f).unscaled());
  ImageSkiaRep rep_2x(Size(100, 100), 2.0f);

  // When reps for other scales are added, the unscaled image
  // becomes scaled.
  image_skia.AddRepresentation(rep_2x);
  EXPECT_FALSE(image_skia.GetRepresentation(1.0f).unscaled());
  EXPECT_FALSE(image_skia.GetRepresentation(2.0f).unscaled());
}

namespace {

std::vector<float> GetSortedScaleFactors(const gfx::ImageSkia& image) {
  const std::vector<ImageSkiaRep>& image_reps = image.image_reps();
  std::vector<float> scale_factors;
  for (size_t i = 0; i < image_reps.size(); ++i) {
    scale_factors.push_back(image_reps[i].scale());
  }
  std::sort(scale_factors.begin(), scale_factors.end());
  return scale_factors;
}

}  // namespace

TEST_F(ImageSkiaTest, ArbitraryScaleFactor) {
  // source is owned by |image|
  DynamicSource* source = new DynamicSource(Size(100, 200));
  ImageSkia image(base::WrapUnique(source), gfx::Size(100, 200));

  image.GetRepresentation(1.5f);
  EXPECT_EQ(2.0f, source->GetLastRequestedScaleAndReset());
  std::vector<ImageSkiaRep> image_reps = image.image_reps();
  EXPECT_EQ(2u, image_reps.size());

  std::vector<float> scale_factors = GetSortedScaleFactors(image);
  EXPECT_EQ(1.5f, scale_factors[0]);
  EXPECT_EQ(2.0f, scale_factors[1]);

  // Requesting 1.75 scale factor also falls back to 2.0f and rescale.
  // However, the image already has the 2.0f data, so it won't fetch again.
  image.GetRepresentation(1.75f);
  EXPECT_EQ(0.0f, source->GetLastRequestedScaleAndReset());
  image_reps = image.image_reps();
  EXPECT_EQ(3u, image_reps.size());

  scale_factors = GetSortedScaleFactors(image);
  EXPECT_EQ(1.5f, scale_factors[0]);
  EXPECT_EQ(1.75f, scale_factors[1]);
  EXPECT_EQ(2.0f, scale_factors[2]);

  // Requesting 1.25 scale factor also falls back to 2.0f and rescale.
  // However, the image already has the 2.0f data, so it won't fetch again.
  image.GetRepresentation(1.25f);
  EXPECT_EQ(0.0f, source->GetLastRequestedScaleAndReset());
  image_reps = image.image_reps();
  EXPECT_EQ(4u, image_reps.size());
  scale_factors = GetSortedScaleFactors(image);
  EXPECT_EQ(1.25f, scale_factors[0]);
  EXPECT_EQ(1.5f, scale_factors[1]);
  EXPECT_EQ(1.75f, scale_factors[2]);
  EXPECT_EQ(2.0f, scale_factors[3]);

  // 1.20 is falled back to 1.0.
  image.GetRepresentation(1.20f);
  EXPECT_EQ(1.0f, source->GetLastRequestedScaleAndReset());
  image_reps = image.image_reps();
  EXPECT_EQ(6u, image_reps.size());
  scale_factors = GetSortedScaleFactors(image);
  EXPECT_EQ(1.0f, scale_factors[0]);
  EXPECT_EQ(1.2f, scale_factors[1]);
  EXPECT_EQ(1.25f, scale_factors[2]);
  EXPECT_EQ(1.5f, scale_factors[3]);
  EXPECT_EQ(1.75f, scale_factors[4]);
  EXPECT_EQ(2.0f, scale_factors[5]);

  // Scale factor less than 1.0f will be falled back to 1.0f
  image.GetRepresentation(0.75f);
  EXPECT_EQ(0.0f, source->GetLastRequestedScaleAndReset());
  image_reps = image.image_reps();
  EXPECT_EQ(7u, image_reps.size());

  scale_factors = GetSortedScaleFactors(image);
  EXPECT_EQ(0.75f, scale_factors[0]);
  EXPECT_EQ(1.0f, scale_factors[1]);
  EXPECT_EQ(1.2f, scale_factors[2]);
  EXPECT_EQ(1.25f, scale_factors[3]);
  EXPECT_EQ(1.5f, scale_factors[4]);
  EXPECT_EQ(1.75f, scale_factors[5]);
  EXPECT_EQ(2.0f, scale_factors[6]);

  // Scale factor greater than 2.0f is falled back to 2.0f because it's not
  // supported.
  image.GetRepresentation(3.0f);
  EXPECT_EQ(0.0f, source->GetLastRequestedScaleAndReset());
  image_reps = image.image_reps();
  EXPECT_EQ(8u, image_reps.size());
}

TEST_F(ImageSkiaTest, ArbitraryScaleFactorWithMissingResource) {
  ImageSkia image(
      std::make_unique<FixedScaleSource>(ImageSkiaRep(Size(100, 200), 1.0f)),
      Size(100, 200));

  // Requesting 1.5f -- falls back to 2.0f, but couldn't find. It should
  // look up 1.0f and then rescale it. Note that the rescaled ImageSkiaRep will
  // have 2.0f scale.
  const ImageSkiaRep& rep = image.GetRepresentation(1.5f);
  EXPECT_EQ(1.5f, rep.scale());
  EXPECT_EQ(2U, image.image_reps().size());
  EXPECT_EQ(2.0f, image.image_reps()[0].scale());
  EXPECT_EQ(1.5f, image.image_reps()[1].scale());
}

TEST_F(ImageSkiaTest, UnscaledImageForArbitraryScaleFactor) {
  // 0.0f means unscaled.
  ImageSkia image(
      std::make_unique<FixedScaleSource>(ImageSkiaRep(Size(100, 200), 0.0f)),
      Size(100, 200));

  // Requesting 2.0f, which should return 1.0f unscaled image.
  const ImageSkiaRep& rep = image.GetRepresentation(2.0f);
  EXPECT_EQ(1.0f, rep.scale());
  EXPECT_EQ("100x200", rep.pixel_size().ToString());
  EXPECT_TRUE(rep.unscaled());
  EXPECT_EQ(1U, image.image_reps().size());

  // Same for any other scale factors.
  const ImageSkiaRep& rep15 = image.GetRepresentation(1.5f);
  EXPECT_EQ(1.0f, rep15.scale());
  EXPECT_EQ("100x200", rep15.pixel_size().ToString());
  EXPECT_TRUE(rep15.unscaled());
  EXPECT_EQ(1U, image.image_reps().size());

  const ImageSkiaRep& rep12 = image.GetRepresentation(1.2f);
  EXPECT_EQ(1.0f, rep12.scale());
  EXPECT_EQ("100x200", rep12.pixel_size().ToString());
  EXPECT_TRUE(rep12.unscaled());
  EXPECT_EQ(1U, image.image_reps().size());
}

}  // namespace gfx
