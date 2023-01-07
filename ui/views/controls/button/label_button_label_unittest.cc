// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/label_button_label.h"

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/test/views_test_base.h"

namespace views {
namespace {

// LabelButtonLabel subclass that reports its text color whenever a paint is
// scheduled.
class TestLabel : public internal::LabelButtonLabel {
 public:
  explicit TestLabel(SkColor* last_color)
      : LabelButtonLabel(std::u16string(), views::style::CONTEXT_BUTTON),
        last_color_(last_color) {}

  TestLabel(const TestLabel&) = delete;
  TestLabel& operator=(const TestLabel&) = delete;

  // LabelButtonLabel:
  void OnDidSchedulePaint(const gfx::Rect& r) override {
    LabelButtonLabel::OnDidSchedulePaint(r);
    *last_color_ = GetEnabledColor();
  }

 private:
  raw_ptr<SkColor> last_color_;
};

}  // namespace

class LabelButtonLabelTest : public ViewsTestBase {
 public:
  LabelButtonLabelTest() = default;

  LabelButtonLabelTest(const LabelButtonLabelTest&) = delete;
  LabelButtonLabelTest& operator=(const LabelButtonLabelTest&) = delete;

  void SetUp() override {
    ViewsTestBase::SetUp();

    widget_ = CreateTestWidget();
    widget_->GetNativeTheme()->set_use_dark_colors(false);

    label_ =
        widget_->SetContentsView(std::make_unique<TestLabel>(&last_color_));
    label_->SetAutoColorReadabilityEnabled(false);
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  void SetUseDarkColors(bool use_dark_colors) {
    ui::NativeTheme* native_theme = widget_->GetNativeTheme();
    native_theme->set_use_dark_colors(use_dark_colors);
    native_theme->NotifyOnNativeThemeUpdated();
  }

 protected:
  SkColor last_color_ = gfx::kPlaceholderColor;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<TestLabel> label_;
};

// Test that LabelButtonLabel reacts properly to themed and overridden colors.
TEST_F(LabelButtonLabelTest, Colors) {
  // First one comes from the default theme. This check ensures the SK_ColorRED
  // placeholder initializers were replaced.
  SkColor default_theme_enabled_color =
      label_->GetColorProvider()->GetColor(ui::kColorLabelForeground);
  EXPECT_EQ(default_theme_enabled_color, last_color_);

  label_->SetEnabled(false);
  SkColor default_theme_disabled_color =
      label_->GetColorProvider()->GetColor(ui::kColorLabelForegroundDisabled);
  EXPECT_EQ(default_theme_disabled_color, last_color_);

  SetUseDarkColors(true);

  SkColor dark_theme_disabled_color =
      label_->GetColorProvider()->GetColor(ui::kColorLabelForegroundDisabled);
  EXPECT_NE(default_theme_disabled_color, dark_theme_disabled_color);
  EXPECT_EQ(dark_theme_disabled_color, last_color_);

  label_->SetEnabled(true);
  SkColor dark_theme_enabled_color =
      label_->GetColorProvider()->GetColor(ui::kColorLabelForeground);
  EXPECT_NE(default_theme_enabled_color, dark_theme_enabled_color);
  EXPECT_EQ(dark_theme_enabled_color, last_color_);

  // Override the theme for the disabled color.
  label_->SetDisabledColor(SK_ColorRED);
  EXPECT_NE(SK_ColorRED, dark_theme_disabled_color);

  // Still enabled, so not RED yet.
  EXPECT_EQ(dark_theme_enabled_color, last_color_);

  label_->SetEnabled(false);
  EXPECT_EQ(SK_ColorRED, last_color_);

  label_->SetDisabledColor(SK_ColorMAGENTA);
  EXPECT_EQ(SK_ColorMAGENTA, last_color_);

  // Disabled still overridden after a theme change.
  SetUseDarkColors(false);
  EXPECT_EQ(SK_ColorMAGENTA, last_color_);

  // The enabled color still gets its value from the theme.
  label_->SetEnabled(true);
  EXPECT_EQ(default_theme_enabled_color, last_color_);

  label_->SetEnabledColor(SK_ColorYELLOW);
  label_->SetDisabledColor(SK_ColorCYAN);
  EXPECT_EQ(SK_ColorYELLOW, last_color_);
  label_->SetEnabled(false);
  EXPECT_EQ(SK_ColorCYAN, last_color_);
}

}  // namespace views
