// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/image_model_utils.h"

#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace views {

TEST(GetImageSkiaFromImageModel, ShouldConvertEmptyModel) {
  gfx::ImageSkia image_skia = GetImageSkiaFromImageModel(ui::ImageModel());
  EXPECT_TRUE(image_skia.isNull());
}

TEST(GetImageSkiaFromImageModel, ShouldConvertVectorIcon) {
  ui::ColorProvider color_provider;
  gfx::ImageSkia image_skia = GetImageSkiaFromImageModel(
      ui::ImageModel::FromVectorIcon(vector_icons::kSyncIcon), &color_provider);
  EXPECT_FALSE(image_skia.isNull());
}

TEST(GetImageSkiaFromImageModel, ShouldConvertImage) {
  gfx::Image image = gfx::test::CreateImage(16, 16);
  gfx::ImageSkia image_skia =
      GetImageSkiaFromImageModel(ui::ImageModel::FromImage(image));
  EXPECT_FALSE(image_skia.isNull());
  EXPECT_TRUE(image_skia.BackedBySameObjectAs(image.AsImageSkia()));
}

}  // namespace views
