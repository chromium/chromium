// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/theme_tracking_animated_image_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "ui/base/resource/mock_resource_bundle_delegate.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_variant.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace views {

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgPointee;

namespace {

constexpr int kLightLottieId = 1;
constexpr int kDarkLottieId = 2;

// A string with the "LOTTIE" prefix that GRIT adds to Lottie assets.
constexpr char kLottiePrefix[] = "LOTTIE";

// Minimal valid Lottie JSON strings that differ only in the frame rate.
constexpr char kLightLottieRawData[] =
    R"({"v":"5.5.4","ip":0,"op":10,"fr":24,"w":100,"h":100,"layers":[]})";
constexpr char kDarkLottieRawData[] =
    R"({"v":"5.5.4","ip":0,"op":10,"fr":30,"w":100,"h":100,"layers":[]})";

std::vector<uint8_t> ToIntVector(const std::string_view& data) {
  return base::ToVector(data,
                        [](char c) { return static_cast<unsigned char>(c); });
}

}  // namespace

class ThemeTrackingAnimatedImageViewTest : public ViewsTestBase {
 public:
  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);

    const std::string kLightLottieData =
        base::StrCat({kLottiePrefix, kLightLottieRawData});
    const std::string kDarkLottieData =
        base::StrCat({kLottiePrefix, kDarkLottieRawData});
    ON_CALL(mock_resource_bundle_delegate_,
            GetRawDataResource(kLightLottieId, _, _))
        .WillByDefault(DoAll(SetArgPointee<2>(kLightLottieData), Return(true)));
    ON_CALL(mock_resource_bundle_delegate_,
            GetRawDataResource(kDarkLottieId, _, _))
        .WillByDefault(DoAll(SetArgPointee<2>(kDarkLottieData), Return(true)));

    test_resource_bundle_with_mock_delegate_ =
        std::make_unique<ui::ResourceBundle>(&mock_resource_bundle_delegate_);
    original_resource_bundle_ =
        ui::ResourceBundle::SwapSharedInstanceForTesting(
            test_resource_bundle_with_mock_delegate_.get());
  }

  void TearDown() override {
    widget_.reset();
    ui::ResourceBundle::SwapSharedInstanceForTesting(original_resource_bundle_);
    ViewsTestBase::TearDown();
  }

  ui::ColorVariant GetSimulatedBackgroundColor() const {
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

  void SetView(std::unique_ptr<ThemeTrackingAnimatedImageView> view) {
    widget_->SetContentsView(std::move(view));
  }

  ThemeTrackingAnimatedImageView* view() {
    return static_cast<ThemeTrackingAnimatedImageView*>(
        widget_->GetContentsView());
  }

 private:
  std::unique_ptr<Widget> widget_;
  testing::NiceMock<ui::MockResourceBundleDelegate>
      mock_resource_bundle_delegate_;
  raw_ptr<ui::ResourceBundle> original_resource_bundle_;
  std::unique_ptr<ui::ResourceBundle> test_resource_bundle_with_mock_delegate_;
  bool is_dark_ = false;
};

TEST_F(ThemeTrackingAnimatedImageViewTest, ShouldUpdateWithThemeChanges) {
  SetView(std::make_unique<ThemeTrackingAnimatedImageView>(
      kLightLottieId, kDarkLottieId,
      base::BindRepeating(
          &ThemeTrackingAnimatedImageViewTest::GetSimulatedBackgroundColor,
          base::Unretained(this))));

  ASSERT_FALSE(IsDarkMode());
  EXPECT_EQ(view()->GetAnimatedImageForTesting()->skottie()->raw_data(),
            ToIntVector(kLightLottieRawData));

  SetIsDarkMode(true);
  EXPECT_EQ(view()->GetAnimatedImageForTesting()->skottie()->raw_data(),
            ToIntVector(kDarkLottieRawData));

  SetIsDarkMode(false);
  EXPECT_EQ(view()->GetAnimatedImageForTesting()->skottie()->raw_data(),
            ToIntVector(kLightLottieRawData));
}

}  // namespace views
