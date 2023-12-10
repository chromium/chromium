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

  std::unique_ptr<Widget> widget = CreateTestWidget();
  auto* button = widget->SetContentsView(
      std::make_unique<MdTextButton>(Button::PressedCallback(), u" "));
  button->SetProminent(true);
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
  std::unique_ptr<Widget> other_widget = CreateTestWidget();
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
}  // namespace views
