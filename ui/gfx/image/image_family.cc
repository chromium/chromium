// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_family.h"

#include <cmath>

#include "skia/ext/image_operations.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace gfx {

ImageFamily::const_iterator::const_iterator() {}

ImageFamily::const_iterator::const_iterator(const const_iterator& other)
    : map_iterator_(other.map_iterator_) {}

ImageFamily::const_iterator::const_iterator(
    const std::map<MapKey, gfx::Image>::const_iterator& other)
    : map_iterator_(other) {}

ImageFamily::const_iterator::~const_iterator() {}

ImageFamily::ImageFamily() {}
ImageFamily::ImageFamily(ImageFamily&& other) = default;
ImageFamily::~ImageFamily() {}

ImageFamily& ImageFamily::operator=(ImageFamily&& other) = default;

ImageFamily ImageFamily::Clone() const {
  ImageFamily clone;
  clone.map_ = map_;
  return clone;
}

void ImageFamily::Add(const gfx::Image& image) {
  gfx::Size size = image.Size();
  if (size.IsEmpty()) {
    map_[MapKey(1.0f, 0)] = image;
  } else {
    float aspect = static_cast<float>(size.width()) / size.height();
    DCHECK_GT(aspect, 0.0f);
    map_[MapKey(aspect, size.width())] = image;
  }
}

void ImageFamily::Add(const gfx::ImageSkia& image_skia) {
  Add(gfx::Image(image_skia));
}

const gfx::Image* ImageFamily::GetBest(int width, int height) const {
  if (map_.empty())
    return NULL;

  // If either |width| or |height| is 0, both are.
  float desired_aspect;
  if (height == 0 || width == 0) {
    desired_aspect = 1.0f;
    height = 0;
    width = 0;
  } else {
    desired_aspect = static_cast<float>(width) / height;
  }
  DCHECK_GT(desired_aspect, 0.0f);

  float closest_aspect = GetClosestAspect(desired_aspect);

  // If thinner than desired, search for images with width such that the
  // corresponding height is greater than or equal to the desired |height|.
  int desired_width = closest_aspect <= desired_aspect ?
      width : static_cast<int>(ceilf(height * closest_aspect));

  // Get the best-sized image with the aspect ratio.
  return GetWithExactAspect(closest_aspect, desired_width);
}

float ImageFamily::GetClosestAspect(float desired_aspect) const {
  // Find the two aspect ratios on either side of |desired_aspect|.
  auto greater_or_equal = map_.lower_bound(MapKey(desired_aspect, 0));
  // Early exit optimization if there is an exact match.
  if (greater_or_equal != map_.end() &&
      greater_or_equal->first.aspect() == desired_aspect) {
    return desired_aspect;
  }

  // No exact match; |greater_or_equal| will point to the first image with
  // aspect ratio >= |desired_aspect|, and |less_than| will point to the last
  // image with aspect ratio < |desired_aspect|.
  if (greater_or_equal != map_.begin()) {
    auto less_than = greater_or_equal;
    --less_than;
    float thinner_aspect = less_than->first.aspect();
    DCHECK_GT(thinner_aspect, 0.0f);
    DCHECK_LT(thinner_aspect, desired_aspect);
    if (greater_or_equal != map_.end()) {
      float wider_aspect = greater_or_equal->first.aspect();
      DCHECK_GT(wider_aspect, desired_aspect);
      if ((wider_aspect / desired_aspect) < (desired_aspect / thinner_aspect))
        return wider_aspect;
    }
    return thinner_aspect;
  } else {
    // No aspect ratio is less than or equal to |desired_aspect|.
    DCHECK(greater_or_equal != map_.end());
    float wider_aspect = greater_or_equal->first.aspect();
    DCHECK_GT(wider_aspect, desired_aspect);
    return wider_aspect;
  }
}

const gfx::Image* ImageFamily::GetBest(const gfx::Size& size) const {
  return GetBest(size.width(), size.height());
}

gfx::Image ImageFamily::CreateExact(int width, int height) const {
  // Resize crashes if width or height is 0, so just return an empty image.
  if (width == 0 || height == 0)
    return gfx::Image();

  const gfx::Image* image = GetBest(width, height);
  if (!image)
    return gfx::Image();

  if (image->Width() == width && image->Height() == height) {
    // Make a copy at gfx::ImageSkia level, so that resulting image's ref count
    // is not racy to |image|. Since this function can run on a different thread
    // than the thread |image| created on, we should not touch the
    // non-thread-safe ref count in gfx::Image here.
    return gfx::Image(image->AsImageSkia());
  }

  SkBitmap bitmap = image->AsBitmap();
  SkBitmap resized_bitmap = skia::ImageOperations::Resize(
      bitmap, skia::ImageOperations::RESIZE_LANCZOS3, width, height);
  return gfx::Image::CreateFrom1xBitmap(resized_bitmap);
}

gfx::Image ImageFamily::CreateExact(const gfx::Size& size) const {
  return CreateExact(size.width(), size.height());
}

const gfx::Image* ImageFamily::GetWithExactAspect(float aspect,
                                                  int width) const {
  // Find the two images of given aspect ratio on either side of |width|.
  auto greater_or_equal = map_.lower_bound(MapKey(aspect, width));
  if (greater_or_equal != map_.end() &&
      greater_or_equal->first.aspect() == aspect) {
    // We have found the smallest image of the same size or greater.
    return &greater_or_equal->second;
  }

  DCHECK(greater_or_equal != map_.begin());
  auto less_than = greater_or_equal;
  --less_than;
  // This must be true because there must be at least one image with |aspect|.
  DCHECK_EQ(less_than->first.aspect(), aspect);
  // We have found the largest image smaller than desired.
  return &less_than->second;
}

}  // namespace gfx
