// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/image_button_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/test/ink_drop_host_test_api.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/views_test_base.h"

namespace views {

using ImageButtonFactoryTest = ViewsTestBase;

TEST_F(ImageButtonFactoryTest, CreateVectorImageButton) {
  auto button = CreateVectorImageButton(Button::PressedCallback());
  EXPECT_EQ(ImageButton::ALIGN_CENTER, button->h_alignment_);
  EXPECT_EQ(ImageButton::ALIGN_MIDDLE, button->v_alignment_);
}

class ImageButtonFactoryWidgetTest : public ViewsTestBase {
 public:
  ImageButtonFactoryWidgetTest() = default;

  ImageButtonFactoryWidgetTest(const ImageButtonFactoryWidgetTest&) = delete;
  ImageButtonFactoryWidgetTest& operator=(const ImageButtonFactoryWidgetTest&) =
      delete;

  ~ImageButtonFactoryWidgetTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    // Create a widget so that buttons can get access to their ColorProvider
    // instance.
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(0, 0, 650, 650);
    widget_->Init(std::move(params));
    widget_->Show();
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  void AddImageButton(std::unique_ptr<ImageButton> button) {
    widget_->SetContentsView(std::move(button));
  }

 protected:
  Widget* widget() { return widget_.get(); }
  ImageButton* button() {
    return static_cast<ImageButton*>(widget_->GetContentsView());
  }

 private:
  std::unique_ptr<Widget> widget_;
};

TEST_F(ImageButtonFactoryWidgetTest, SetImageFromVectorIconWithColor) {
  AddImageButton(CreateVectorImageButton(Button::PressedCallback()));
  SetImageFromVectorIconWithColor(button(), vector_icons::kCloseRoundedIcon,
                                  SK_ColorRED, SK_ColorRED);
  EXPECT_FALSE(button()->GetImage(Button::STATE_NORMAL).isNull());
  EXPECT_FALSE(button()->GetImage(Button::STATE_DISABLED).isNull());
  EXPECT_EQ(SK_ColorRED, InkDrop::Get(button())->GetBaseColor());
}

TEST_F(ImageButtonFactoryWidgetTest, CreateVectorImageButtonWithNativeTheme) {
  AddImageButton(CreateVectorImageButtonWithNativeTheme(
      Button::PressedCallback(), vector_icons::kCloseRoundedIcon));
  EXPECT_EQ(button()->GetColorProvider()->GetColor(ui::kColorIcon),
            InkDrop::Get(button())->GetBaseColor());
}

TEST_F(ImageButtonFactoryWidgetTest,
       CreateVectorImageButtonWithNativeThemeWithSize) {
  constexpr int kSize = 15;
  AddImageButton(CreateVectorImageButtonWithNativeTheme(
      Button::PressedCallback(), vector_icons::kEditIcon, kSize));
  EXPECT_EQ(kSize, button()->GetImage(Button::STATE_NORMAL).width());
}

TEST_F(ImageButtonFactoryWidgetTest, AccessibleCheckedStateChange) {
  auto toggle_image_button =
      CreateVectorToggleImageButton(Button::PressedCallback());
  ui::AXNodeData data;
  const ui::ImageModel& image =
      ui::ImageModel::FromVectorIcon(gfx::VectorIcon(), 23, 56);
  toggle_image_button->SetToggledImageModel(Button::STATE_NORMAL, image);
  EXPECT_EQ(data.GetCheckedState(), ax::mojom::CheckedState::kNone);

  toggle_image_button->SetToggled(true);
  toggle_image_button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetCheckedState(), ax::mojom::CheckedState::kTrue);

  data = ui::AXNodeData();
  toggle_image_button->SetToggled(false);
  toggle_image_button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetCheckedState(), ax::mojom::CheckedState::kFalse);
}

TEST_F(ImageButtonFactoryWidgetTest, AccessibleName) {
  const std::u16string untoggled_text(u"untoggled_text");
  const std::u16string toggled_tooltip_text(u"toggled_tooltip_text");
  const std::u16string toggled_accessible_name(u"toggled_accessible_name");
  const std::u16string toggled_accessible_empty_name(u"");

  std::unique_ptr<ToggleImageButton> button =
      CreateVectorToggleImageButton(Button::PressedCallback());

  button->SetTooltipText(untoggled_text);
  button->SetToggledAccessibleName(toggled_accessible_name);
  button->SetToggledTooltipText(toggled_tooltip_text);

  ui::AXNodeData data;
  button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            untoggled_text);

  button->SetToggled(true);
  data = ui::AXNodeData();
  button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            toggled_accessible_name);

  button->SetToggledAccessibleName(toggled_accessible_empty_name);
  data = ui::AXNodeData();
  button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            toggled_tooltip_text);

  button->SetToggled(false);
  data = ui::AXNodeData();
  button->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            untoggled_text);
}

}  // namespace views
