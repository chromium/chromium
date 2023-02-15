// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/md_text_button.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
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

  std::unique_ptr<Widget> widget = CreateTestWidget();
  auto* button = widget->SetContentsView(
      std::make_unique<MdTextButton>(Button::PressedCallback(), u"button"));
  button->SetProminent(true);
  button->SetBounds(0, 0, 70, 20);
  widget->LayoutRootViewIfNecessary();

  const ui::ColorProvider* color_provider = button->GetColorProvider();

  test::WidgetTest::SimulateNativeActivate(widget.get());
  EXPECT_TRUE(widget->IsActive());
  SkBitmap active_bitmap = views::test::PaintViewToBitmap(button);

  auto background_color = [button](const SkBitmap& bitmap) {
    // The very edge of the bitmap contains the button's border, which we aren't
    // interested in here. Instead, grab a pixel that is inset by the button's
    // corner radius from the top-left point to avoid the border.
    //
    // It would make a bit more sense to inset by the border thickness or
    // something, but MdTextButton doesn't expose (or even know) that value
    // without some major abstraction violation.
    int corner_radius = button->GetCornerRadiusValue();
    return bitmap.getColor(corner_radius, corner_radius);
  };

  EXPECT_EQ(background_color(active_bitmap),
            color_provider->GetColor(ui::kColorButtonBackgroundProminent));

  // It would be neat to also check the text color here, but the label's text
  // ends up drawn on top of the background with antialiasing, which means there
  // aren't any pixels that are actually *exactly*
  // kColorButtonForegroundProminent. Bummer.

  // Activate another widget to cause the original widget to deactivate.
  std::unique_ptr<Widget> other_widget = CreateTestWidget();
  test::WidgetTest::SimulateNativeActivate(other_widget.get());
  EXPECT_FALSE(widget->IsActive());
  SkBitmap inactive_bitmap = views::test::PaintViewToBitmap(button);

  EXPECT_EQ(
      background_color(inactive_bitmap),
      color_provider->GetColor(ui::kColorButtonBackgroundProminentDisabled));
}

}  // namespace views
