// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/md_text_button.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/actions/actions.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/views/background.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/test/views_drawing_test_utils.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"

namespace views {

using MdTextButtonTest = ViewsTestBase;

TEST_F(MdTextButtonTest, CustomPadding) {
  const std::u16string text = u"abc";
  auto button = std::make_unique<MdTextButton>(Button::PressedCallback(), text);

  const auto custom_padding = gfx::Insets::VH(10, 20);
  ASSERT_NE(button->GetInsets(), custom_padding);

  button->SetCustomPadding(custom_padding);
  EXPECT_EQ(button->GetInsets(), custom_padding);
}

TEST_F(MdTextButtonTest, BackgroundColorChangesWithWidgetActivation) {
  // Test whether the button's background color changes when its containing
  // widget's activation changes.
  if (!PlatformStyle::kInactiveWidgetControlsAppearDisabled)
    GTEST_SKIP() << "Button colors do not change with widget activation here.";

  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* button = widget->SetContentsView(
      std::make_unique<MdTextButton>(Button::PressedCallback(), u" "));
  button->SetStyle(ui::ButtonStyle::kProminent);
  button->SetBounds(0, 0, 70, 20);
  widget->LayoutRootViewIfNecessary();

  const ui::ColorProvider* color_provider = button->GetColorProvider();

  test::WidgetTest::SimulateNativeActivate(widget.get());
  EXPECT_TRUE(widget->IsActive());
  SkBitmap active_bitmap = views::test::PaintViewToBitmap(button);

  auto background_color = [button](const SkBitmap& bitmap) {
    return bitmap.getColor(button->size().width() / 2.,
                           button->size().height() / 2.);
  };

  EXPECT_EQ(background_color(active_bitmap),
            color_provider->GetColor(ui::kColorButtonBackgroundProminent));

  // It would be neat to also check the text color here, but the label's text
  // ends up drawn on top of the background with antialiasing, which means there
  // aren't any pixels that are actually *exactly*
  // kColorButtonForegroundProminent. Bummer.

  // Activate another widget to cause the original widget to deactivate.
  std::unique_ptr<Widget> other_widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  test::WidgetTest::SimulateNativeActivate(other_widget.get());
  EXPECT_FALSE(widget->IsActive());
  SkBitmap inactive_bitmap = views::test::PaintViewToBitmap(button);

  EXPECT_EQ(
      background_color(inactive_bitmap),
      color_provider->GetColor(ui::kColorButtonBackgroundProminentDisabled));
}

using MdTextButtonActionViewInterfaceTest = ViewsTestBase;

TEST_F(MdTextButtonActionViewInterfaceTest, TestActionChanged) {
  auto md_text_button = std::make_unique<MdTextButton>();
  const std::u16string test_string = u"test_string";
  std::unique_ptr<actions::ActionItem> action_item =
      actions::ActionItem::Builder()
          .SetText(test_string)
          .SetActionId(0)
          .SetEnabled(false)
          .Build();
  action_item->SetText(test_string);
  md_text_button->GetActionViewInterface()->ActionItemChangedImpl(
      action_item.get());
  // Test some properties to ensure that the right ActionViewInterface is linked
  // to the view.
  EXPECT_EQ(test_string, md_text_button->GetText());
  EXPECT_FALSE(md_text_button->GetEnabled());
}

TEST_F(MdTextButtonActionViewInterfaceTest,
       DefaultCornerRadiusDependsOnButtonSize) {
  auto md_text_button = std::make_unique<MdTextButton>();
  constexpr gfx::Size kSize1(100, 100);
  constexpr gfx::Size kSize2(50, 50);

  const int corner_radius_1 = LayoutProvider::Get()->GetCornerRadiusMetric(
      ShapeContextTokens::kButtonRadius, kSize1);
  const int corner_radius_2 = LayoutProvider::Get()->GetCornerRadiusMetric(
      ShapeContextTokens::kButtonRadius, kSize2);
  ASSERT_NE(corner_radius_1, corner_radius_2);

  md_text_button->SetBoundsRect(gfx::Rect(kSize1));
  EXPECT_EQ(md_text_button->GetCornerRadiusValue(), corner_radius_1);
  EXPECT_EQ(md_text_button->GetFocusRingCornerRadius(), corner_radius_1);

  md_text_button->SetBoundsRect(gfx::Rect(kSize2));
  EXPECT_EQ(md_text_button->GetCornerRadiusValue(), corner_radius_2);
  EXPECT_EQ(md_text_button->GetFocusRingCornerRadius(), corner_radius_2);
}

TEST_F(MdTextButtonActionViewInterfaceTest,
       CustomCornerRadiusIsNotOverriddenOnButtonSizeChange) {
  auto md_text_button = std::make_unique<MdTextButton>();
  md_text_button->SetBoundsRect(gfx::Rect(100, 100));

  constexpr int kCustomCornerRadius = 1234;
  md_text_button->SetCornerRadius(kCustomCornerRadius);
  ASSERT_EQ(md_text_button->GetCornerRadiusValue(), kCustomCornerRadius);

  md_text_button->SetBoundsRect(gfx::Rect(50, 50));
  EXPECT_EQ(md_text_button->GetCornerRadiusValue(), kCustomCornerRadius);
  EXPECT_EQ(md_text_button->GetFocusRingCornerRadius(), kCustomCornerRadius);
}

TEST_F(MdTextButtonTest, StrokeColorIdOverride) {
  auto button = std::make_unique<MdTextButton>();

  ASSERT_FALSE(button->GetStrokeColorIdOverride().has_value());

  button->SetStrokeColorIdOverride(ui::kColorButtonBorder);
  EXPECT_EQ(ui::kColorButtonBorder, button->GetStrokeColorIdOverride().value());
}

}  // namespace views
