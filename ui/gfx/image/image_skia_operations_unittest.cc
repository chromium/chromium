// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_skia_operations.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace gfx {
namespace {

TEST(ImageSkiaOperationsTest, ResizeFailure) {
  ImageSkia image(ImageSkiaRep(gfx::Size(10, 10), 1.f));

  // Try to resize to empty. This isn't a valid resize and fails gracefully.
  ImageSkia resized = ImageSkiaOperations::CreateResizedImage(
      image, skia::ImageOperations::RESIZE_BEST, gfx::Size());
  EXPECT_TRUE(resized.GetRepresentation(1.0f).is_null());
}

}  // namespace
}  // namespace gfx
