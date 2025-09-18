// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/label_button_label.h"

#include <map>
#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_variant.h"
#include "ui/native_theme/mock_os_settings_provider.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/test/views_test_base.h"

namespace views {
namespace {

// LabelButtonLabel subclass that reports its text color whenever a paint is
// scheduled.
class TestLabel : public internal::LabelButtonLabel {
 public:
  explicit TestLabel(SkColor* last_color,
                     std::optional<ui::ColorVariant>* last_requested_color)
      : LabelButtonLabel(std::u16string(), views::style::CONTEXT_BUTTON),
        last_color_(last_color),
        last_requested_color_(last_requested_color) {}

  TestLabel(const TestLabel&) = delete;
  TestLabel& operator=(const TestLabel&) = delete;

  // LabelButtonLabel:
  void OnDidSchedulePaint(const gfx::Rect& r) override {
    LabelButtonLabel::OnDidSchedulePaint(r);
    *last_color_ = Label::GetEnabledColor();
    *last_requested_color_ = Label::GetRequestedEnabledColor();
  }

 private:
  raw_ptr<SkColor> last_color_;
  raw_ptr<std::optional<ui::ColorVariant>> last_requested_color_;
};

}  // namespace

class LabelButtonLabelTest : public ViewsTestBase {
 public:
  void SetUp() override {
    ViewsTestBase::SetUp();

    widget_ = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);

    widget_->SetContentsView(
        std::make_unique<TestLabel>(&last_color_, &last_requested_color_));
    label()->SetAutoColorReadabilityEnabled(false);
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  ui::MockOsSettingsProvider& os_settings_provider() {
    return os_settings_provider_;
  }

  TestLabel* label() {
    return static_cast<TestLabel*>(widget_->GetContentsView());
  }

  SkColor last_color() const { return last_color_; }
  std::optional<ui::ColorVariant> last_requested_color() const {
    return last_requested_color_;
  }

 private:
  ui::MockOsSettingsProvider os_settings_provider_;
  std::unique_ptr<views::Widget> widget_;
  SkColor last_color_ = gfx::kPlaceholderColor;
  std::optional<ui::ColorVariant> last_requested_color_;
};

// Test that LabelButtonLabel reacts properly to themed and overridden colors.
TEST_F(LabelButtonLabelTest, Colors) {
  // First one comes from the default theme. This check ensures the SK_ColorRED
  // placeholder initializers were replaced.
  SkColor default_theme_enabled_color =
      label()->GetColorProvider()->GetColor(ui::kColorLabelForeground);
  EXPECT_EQ(default_theme_enabled_color, last_color());

  label()->SetEnabled(false);
  SkColor default_theme_disabled_color =
      label()->GetColorProvider()->GetColor(ui::kColorLabelForegroundDisabled);
  EXPECT_EQ(default_theme_disabled_color, last_color());

  os_settings_provider().SetPreferredColorScheme(
      ui::NativeTheme::PreferredColorScheme::kDark);

  SkColor dark_theme_disabled_color =
      label()->GetColorProvider()->GetColor(ui::kColorLabelForegroundDisabled);
  EXPECT_NE(default_theme_disabled_color, dark_theme_disabled_color);
  EXPECT_EQ(dark_theme_disabled_color, last_color());

  label()->SetEnabled(true);
  SkColor dark_theme_enabled_color =
      label()->GetColorProvider()->GetColor(ui::kColorLabelForeground);
  EXPECT_NE(default_theme_enabled_color, dark_theme_enabled_color);
  EXPECT_EQ(dark_theme_enabled_color, last_color());

  // Override the theme for the disabled color.
  label()->SetDisabledColor(SK_ColorRED);
  EXPECT_NE(SK_ColorRED, dark_theme_disabled_color);

  // Still enabled, so not RED yet.
  EXPECT_EQ(dark_theme_enabled_color, last_color());

  label()->SetEnabled(false);
  EXPECT_EQ(SK_ColorRED, last_color());

  label()->SetDisabledColor(SK_ColorMAGENTA);
  EXPECT_EQ(SK_ColorMAGENTA, last_color());

  // Disabled still overridden after a theme change.
  os_settings_provider().SetPreferredColorScheme(
      ui::NativeTheme::PreferredColorScheme::kLight);
  EXPECT_EQ(SK_ColorMAGENTA, last_color());

  // The enabled color still gets its value from the theme.
  label()->SetEnabled(true);
  EXPECT_EQ(default_theme_enabled_color, last_color());

  label()->SetEnabledColor(SK_ColorYELLOW);
  label()->SetDisabledColor(SK_ColorCYAN);
  EXPECT_EQ(SK_ColorYELLOW, last_color());
  label()->SetEnabled(false);
  EXPECT_EQ(SK_ColorCYAN, last_color());
}

// Test that LabelButtonLabel reacts properly to themed and overridden color
// ids.
TEST_F(LabelButtonLabelTest, ColorIds) {
  // Default color id was set.
  EXPECT_TRUE(last_requested_color().has_value());

  // Override the theme for the enabled color.
  label()->SetEnabledColor(ui::kColorAccent);
  EXPECT_EQ(last_requested_color(), ui::kColorAccent);
  EXPECT_EQ(last_color(),
            label()->GetColorProvider()->GetColor(ui::kColorAccent));

  label()->SetEnabled(false);
  label()->SetDisabledColor(ui::kColorBadgeBackground);
  EXPECT_EQ(last_requested_color(), ui::kColorBadgeBackground);
  EXPECT_EQ(last_color(),
            label()->GetColorProvider()->GetColor(ui::kColorBadgeBackground));

  // Still overridden after a theme change.
  os_settings_provider().SetPreferredColorScheme(
      ui::NativeTheme::PreferredColorScheme::kDark);
  EXPECT_EQ(last_requested_color(), ui::kColorBadgeBackground);
  EXPECT_EQ(last_color(),
            label()->GetColorProvider()->GetColor(ui::kColorBadgeBackground));
  label()->SetEnabled(true);
  EXPECT_EQ(last_requested_color(), ui::kColorAccent);
  EXPECT_EQ(last_color(),
            label()->GetColorProvider()->GetColor(ui::kColorAccent));
}

}  // namespace views
