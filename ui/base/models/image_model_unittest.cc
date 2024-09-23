// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/image_model.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"

namespace ui {

namespace {

const gfx::VectorIcon& GetCircleVectorIcon() {
  static constexpr gfx::PathElement path[] = {gfx::CommandType::CIRCLE, 24, 18,
                                              5};
  static const gfx::VectorIconRep rep[] = {{path, 4}};
  static constexpr gfx::VectorIcon circle_icon = {rep, 1, "circle"};

  return circle_icon;
}

const gfx::VectorIcon& GetRectVectorIcon() {
  static constexpr gfx::PathElement path[] = {
      gfx::CommandType::LINE_TO, 0,  10, gfx::CommandType::LINE_TO, 10, 10,
      gfx::CommandType::LINE_TO, 10, 0,  gfx::CommandType::CLOSE};
  static const gfx::VectorIconRep rep[] = {{path, 10}};
  static constexpr gfx::VectorIcon rect_icon = {rep, 1, "rect"};

  return rect_icon;
}

}  // namespace

TEST(ImageModelTest, DefaultEmpty) {
  ImageModel image_model;

  EXPECT_TRUE(image_model.IsEmpty());
}

TEST(ImageModelTest, DefaultVectorIconEmpty) {
  VectorIconModel vector_icon_model;

  EXPECT_TRUE(vector_icon_model.is_empty());
}

TEST(ImageModelTest, CheckForVectorIcon) {
  ImageModel image_model =
      ImageModel::FromVectorIcon(GetCircleVectorIcon(), ui::kColorMenuIcon, 16);

  EXPECT_FALSE(image_model.IsEmpty());
  EXPECT_TRUE(image_model.IsVectorIcon());
}

TEST(ImageModelTest, CheckForImage) {
  ImageModel image_model =
      ImageModel::FromImage(gfx::test::CreateImage(16, 16));

  EXPECT_FALSE(image_model.IsEmpty());
  EXPECT_TRUE(image_model.IsImage());
}

TEST(ImageModelTest, CheckForImageGenerator) {
  ImageModel image_model = ImageModel::FromImageGenerator(
      base::BindRepeating([](const ui::ColorProvider*) {
        return gfx::test::CreateImage(16, 16).AsImageSkia();
      }),
      gfx::Size(16, 16));

  EXPECT_FALSE(image_model.IsEmpty());
  EXPECT_TRUE(image_model.IsImageGenerator());
}

TEST(ImageModelTest, Size) {
  EXPECT_EQ(gfx::Size(), ImageModel().Size());
  EXPECT_EQ(
      gfx::Size(16, 16),
      ImageModel::FromVectorIcon(GetCircleVectorIcon(), ui::kColorMenuIcon, 16)
          .Size());
  EXPECT_EQ(gfx::Size(16, 16),
            ImageModel::FromImage(gfx::test::CreateImage(16, 16)).Size());
  EXPECT_EQ(gfx::Size(16, 16),
            ImageModel::FromImageGenerator(
                base::BindRepeating([](const ui::ColorProvider*) {
                  return gfx::test::CreateImage(16, 16).AsImageSkia();
                }),
                gfx::Size(16, 16))
                .Size());
}

TEST(ImageModelTest, CheckAssignVectorIcon) {
  VectorIconModel vector_icon_model_dest;
  VectorIconModel vector_icon_model_src =
      ImageModel::FromVectorIcon(GetCircleVectorIcon(), ui::kColorMenuIcon, 16)
          .GetVectorIcon();

  EXPECT_TRUE(vector_icon_model_dest.is_empty());
  EXPECT_FALSE(vector_icon_model_src.is_empty());

  vector_icon_model_dest = vector_icon_model_src;
  EXPECT_FALSE(vector_icon_model_dest.is_empty());
}

TEST(ImageModelTest, CheckAssignImage) {
  ImageModel image_model_dest;
  ImageModel image_model_src =
      ImageModel::FromImage(gfx::test::CreateImage(16, 16));

  EXPECT_TRUE(image_model_dest.IsEmpty());
  EXPECT_FALSE(image_model_src.IsEmpty());
  EXPECT_TRUE(image_model_src.IsImage());

  image_model_dest = image_model_src;

  EXPECT_FALSE(image_model_dest.IsEmpty());
  EXPECT_TRUE(image_model_dest.IsImage());

  image_model_src =
      ImageModel::FromVectorIcon(GetCircleVectorIcon(), ui::kColorMenuIcon, 16);

  EXPECT_TRUE(image_model_src.IsVectorIcon());

  image_model_dest = image_model_src;

  EXPECT_TRUE(image_model_dest.IsVectorIcon());

  image_model_src = ImageModel::FromImageGenerator(
      base::BindRepeating([](const ui::ColorProvider*) {
        return gfx::test::CreateImage(16, 16).AsImageSkia();
      }),
      gfx::Size(16, 16));

  EXPECT_TRUE(image_model_src.IsImageGenerator());

  image_model_dest = image_model_src;

  EXPECT_TRUE(image_model_dest.IsImageGenerator());
}

TEST(ImageModelTest, CheckEqual) {
  ImageModel image_model_src;
  ImageModel image_model_dest;
  EXPECT_EQ(image_model_src, image_model_dest);

  auto first_image = gfx::test::CreateImage(16, 16);
  image_model_src = ImageModel::FromImage(first_image);
  EXPECT_NE(image_model_src, image_model_dest);
  image_model_dest = ImageModel::FromImage(first_image);
  EXPECT_EQ(image_model_src, image_model_dest);
  image_model_dest = ImageModel::FromImage(gfx::test::CreateImage(16, 16));
  EXPECT_NE(image_model_src, image_model_dest);
  image_model_src = image_model_dest;
  EXPECT_EQ(image_model_src, image_model_dest);

  image_model_dest =
      ImageModel::FromVectorIcon(GetRectVectorIcon(), ui::kColorMenuIcon, 16);
  EXPECT_NE(image_model_src, image_model_dest);
  image_model_src =
      ImageModel::FromVectorIcon(GetRectVectorIcon(), ui::kColorMenuIcon, 16);
  EXPECT_EQ(image_model_src, image_model_dest);
  image_model_dest =
      ImageModel::FromVectorIcon(GetCircleVectorIcon(), ui::kColorMenuIcon, 16);
  EXPECT_NE(image_model_src, image_model_dest);
  image_model_src = image_model_dest;
  EXPECT_EQ(image_model_src, image_model_dest);

  image_model_src =
      ImageModel::FromVectorIcon(GetCircleVectorIcon(), ui::kColorMenuIcon, 16);
  image_model_dest =
      ImageModel::FromVectorIcon(GetCircleVectorIcon(), SK_ColorMAGENTA, 16);
  EXPECT_NE(image_model_src, image_model_dest);

  image_model_src =
      ImageModel::FromVectorIcon(GetCircleVectorIcon(), ui::kColorMenuIcon, 16);
  image_model_dest = ImageModel::FromVectorIcon(
      GetCircleVectorIcon(), ui::kColorMenuItemForeground, 16);
  EXPECT_NE(image_model_src, image_model_dest);

  image_model_src =
      ImageModel::FromVectorIcon(GetCircleVectorIcon(), SK_ColorCYAN, 16);
  image_model_dest =
      ImageModel::FromVectorIcon(GetCircleVectorIcon(), SK_ColorMAGENTA, 16);
  EXPECT_NE(image_model_src, image_model_dest);

  image_model_src =
      ImageModel::FromVectorIcon(GetCircleVectorIcon(), SK_ColorMAGENTA, 1);
  image_model_dest =
      ImageModel::FromVectorIcon(GetCircleVectorIcon(), SK_ColorMAGENTA, 2);
  EXPECT_NE(image_model_src, image_model_dest);

  auto generator = base::BindRepeating([](const ui::ColorProvider*) {
    return gfx::test::CreateImage(16, 16).AsImageSkia();
  });
  image_model_src =
      ImageModel::FromImageGenerator(generator, gfx::Size(16, 16));
  EXPECT_NE(image_model_src, image_model_dest);
  image_model_dest =
      ImageModel::FromImageGenerator(generator, gfx::Size(16, 16));
  EXPECT_EQ(image_model_src, image_model_dest);
  image_model_dest = ImageModel::FromImageGenerator(generator, gfx::Size(8, 8));
  EXPECT_NE(image_model_src, image_model_dest);
  image_model_dest = ImageModel::FromImageGenerator(
      base::BindRepeating([](const ui::ColorProvider*) {
        return gfx::test::CreateImage(8, 8).AsImageSkia();
      }),
      gfx::Size(16, 16));
  EXPECT_NE(image_model_src, image_model_dest);
  image_model_src = image_model_dest;
  EXPECT_EQ(image_model_src, image_model_dest);
}

#if !BUILDFLAG(IS_IOS)
TEST(ImageModelTest, ShouldRasterizeEmptyModel) {
  gfx::ImageSkia image_skia = ui::ImageModel().Rasterize(nullptr);
  EXPECT_TRUE(image_skia.isNull());
}

TEST(ImageModelTest, ShouldRasterizeVectorIcon) {
  ui::ColorProvider color_provider;
  gfx::ImageSkia image_skia =
      ui::ImageModel::FromVectorIcon(vector_icons::kSyncIcon)
          .Rasterize(&color_provider);
  EXPECT_FALSE(image_skia.isNull());
}

TEST(ImageModelTest, ShouldRasterizeImage) {
  gfx::Image image = gfx::test::CreateImage(16, 16);
  gfx::ImageSkia image_skia =
      ui::ImageModel::FromImage(image).Rasterize(nullptr);
  EXPECT_FALSE(image_skia.isNull());
  EXPECT_TRUE(image_skia.BackedBySameObjectAs(image.AsImageSkia()));
}
#endif  // !BUILDFLAG(IS_IOS)

}  // namespace ui
