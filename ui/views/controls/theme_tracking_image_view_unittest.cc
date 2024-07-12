// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/theme_tracking_image_view.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

constexpr int kImageSize = 16;

}  // namespace

class ThemeTrackingImageViewTest : public ViewsTestBase {
 public:
  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  SkColor GetSimulatedBackgroundColor() const {
    return is_dark_ ? SK_ColorBLACK : SK_ColorWHITE;
  }

 protected:
  bool IsDarkMode() const { return is_dark_; }
  void SetIsDarkMode(bool is_dark) {
    is_dark_ = is_dark;
    if (view()) {
      view()->OnThemeChanged();
    }
  }

  void SetView(std::unique_ptr<ThemeTrackingImageView> view) {
    widget_->SetContentsView(std::move(view))
        ->SetBounds(0, 0, kImageSize, kImageSize);
  }

  ThemeTrackingImageView* view() {
    return static_cast<ThemeTrackingImageView*>(widget_->GetContentsView());
  }
  Widget* widget() { return widget_.get(); }

 private:
  std::unique_ptr<Widget> widget_;

  bool is_dark_ = false;
};

TEST_F(ThemeTrackingImageViewTest, CreateWithImageSkia) {
  gfx::ImageSkia light_image =
      gfx::test::CreateImageSkia(kImageSize, SK_ColorRED);
  gfx::ImageSkia dark_image =
      gfx::test::CreateImageSkia(kImageSize, SK_ColorBLUE);

  SetView(std::make_unique<ThemeTrackingImageView>(
      light_image, dark_image,
      base::BindRepeating(
          &ThemeTrackingImageViewTest::GetSimulatedBackgroundColor,
          base::Unretained(this))));

  ASSERT_FALSE(IsDarkMode());
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(*view()->GetImage().bitmap(),
                                         *light_image.bitmap()));

  SetIsDarkMode(true);
  EXPECT_FALSE(gfx::test::AreBitmapsEqual(*view()->GetImage().bitmap(),
                                          *light_image.bitmap()));
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(*view()->GetImage().bitmap(),
                                         *dark_image.bitmap()));
}

TEST_F(ThemeTrackingImageViewTest, CreateWithImageModel) {
  ui::ImageModel light_model{ui::ImageModel::FromVectorIcon(
      vector_icons::kSyncIcon, ui::kColorMenuIcon, kImageSize)};
  ui::ImageModel dark_model{ui::ImageModel::FromVectorIcon(
      vector_icons::kCallIcon, ui::kColorMenuIcon, kImageSize)};

  SetView(std::make_unique<ThemeTrackingImageView>(
      light_model, dark_model,
      base::BindRepeating(
          &ThemeTrackingImageViewTest::GetSimulatedBackgroundColor,
          base::Unretained(this))));

  ASSERT_FALSE(IsDarkMode());
  EXPECT_EQ(view()->GetImageModel(), light_model);

  SetIsDarkMode(true);
  EXPECT_NE(view()->GetImageModel(), light_model);
  EXPECT_EQ(view()->GetImageModel(), dark_model);
}

TEST_F(ThemeTrackingImageViewTest, SetLightImage) {
  ui::ImageModel light_model1{ui::ImageModel::FromVectorIcon(
      vector_icons::kSyncIcon, ui::kColorMenuIcon, kImageSize)};
  ui::ImageModel light_model2{ui::ImageModel::FromVectorIcon(
      vector_icons::kUsbIcon, ui::kColorMenuIcon, kImageSize)};
  ui::ImageModel dark_model{ui::ImageModel::FromVectorIcon(
      vector_icons::kCallIcon, ui::kColorMenuIcon, kImageSize)};

  SetView(std::make_unique<ThemeTrackingImageView>(
      light_model1, dark_model,
      base::BindRepeating(
          &ThemeTrackingImageViewTest::GetSimulatedBackgroundColor,
          base::Unretained(this))));

  ASSERT_FALSE(IsDarkMode());
  EXPECT_EQ(view()->GetImageModel(), light_model1);

  SetIsDarkMode(true);
  view()->SetLightImage(light_model2);
  // The image remains the one for dark mode.
  EXPECT_EQ(view()->GetImageModel(), dark_model);

  // Upon switching, the image is updated.
  SetIsDarkMode(false);
  EXPECT_EQ(view()->GetImageModel(), light_model2);

  // If light mode is currently on, the image is updated immediately.
  view()->SetLightImage(light_model1);
  EXPECT_EQ(view()->GetImageModel(), light_model1);
}

TEST_F(ThemeTrackingImageViewTest, SetDarkImage) {
  ui::ImageModel light_model{ui::ImageModel::FromVectorIcon(
      vector_icons::kSyncIcon, ui::kColorMenuIcon, kImageSize)};
  ui::ImageModel dark_model1{ui::ImageModel::FromVectorIcon(
      vector_icons::kUsbIcon, ui::kColorMenuIcon, kImageSize)};
  ui::ImageModel dark_model2{ui::ImageModel::FromVectorIcon(
      vector_icons::kCallIcon, ui::kColorMenuIcon, kImageSize)};

  SetView(std::make_unique<ThemeTrackingImageView>(
      light_model, dark_model1,
      base::BindRepeating(
          &ThemeTrackingImageViewTest::GetSimulatedBackgroundColor,
          base::Unretained(this))));

  ASSERT_FALSE(IsDarkMode());
  EXPECT_EQ(view()->GetImageModel(), light_model);

  SetIsDarkMode(true);
  EXPECT_EQ(view()->GetImageModel(), dark_model1);

  SetIsDarkMode(false);
  view()->SetDarkImage(dark_model2);
  EXPECT_EQ(view()->GetImageModel(), light_model);

  SetIsDarkMode(true);
  EXPECT_EQ(view()->GetImageModel(), dark_model2);

  view()->SetDarkImage(dark_model1);
  EXPECT_EQ(view()->GetImageModel(), dark_model1);
}

}  // namespace views
