// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/button_example.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/resources/grit/views_resources.h"
#include "ui/views/view.h"

using base::ASCIIToUTF16;

namespace {
const char kLabelButton[] = "Label Button";
const char kLongText[] = "Start of Really Really Really Really Really Really "
                         "Really Really Really Really Really Really Really "
                         "Really Really Really Really Really Long Button Text";
}  // namespace

namespace views {
namespace examples {

ButtonExample::ButtonExample() : ExampleBase("Button") {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  icon_ = rb.GetImageNamed(IDR_CLOSE_H).ToImageSkia();
}

ButtonExample::~ButtonExample() = default;

void ButtonExample::CreateExampleView(View* container) {
  container->SetBackground(CreateSolidBackground(SK_ColorWHITE));
  auto layout = std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical,
                                            gfx::Insets(10), 10);
  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kCenter);
  container->SetLayoutManager(std::move(layout));

  auto label_button =
      std::make_unique<LabelButton>(this, ASCIIToUTF16(kLabelButton));
  label_button->SetFocusForPlatform();
  label_button->set_request_focus_on_press(true);
  label_button_ = container->AddChildView(std::move(label_button));

  md_button_ = container->AddChildView(
      MdTextButton::Create(this, base::ASCIIToUTF16("Material Design")));

  auto md_disabled_button = MdTextButton::Create(
      this, ASCIIToUTF16("Material Design Disabled Button"));
  md_disabled_button->SetState(Button::STATE_DISABLED);
  md_disabled_button_ = container->AddChildView(std::move(md_disabled_button));

  auto md_default_button =
      MdTextButton::Create(this, base::ASCIIToUTF16("Default"));
  md_default_button->SetIsDefault(true);
  md_default_button_ = container->AddChildView(std::move(md_default_button));

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  auto image_button = std::make_unique<ImageButton>(this);
  image_button->SetFocusForPlatform();
  image_button->set_request_focus_on_press(true);
  image_button->SetImage(ImageButton::STATE_NORMAL,
                         rb.GetImageNamed(IDR_CLOSE).ToImageSkia());
  image_button->SetImage(ImageButton::STATE_HOVERED,
                         rb.GetImageNamed(IDR_CLOSE_H).ToImageSkia());
  image_button->SetImage(ImageButton::STATE_PRESSED,
                         rb.GetImageNamed(IDR_CLOSE_P).ToImageSkia());
  image_button_ = container->AddChildView(std::move(image_button));
}

void ButtonExample::LabelButtonPressed(LabelButton* label_button,
                                       const ui::Event& event) {
  PrintStatus("Label Button Pressed! count: %d", ++count_);
  if (event.IsControlDown()) {
    if (event.IsShiftDown()) {
      label_button->SetText(ASCIIToUTF16(
          label_button->GetText().empty()
              ? kLongText
              : label_button->GetText().length() > 50 ? kLabelButton : ""));
    } else if (event.IsAltDown()) {
      label_button->SetImage(
          Button::STATE_NORMAL,
          label_button->GetImage(Button::STATE_NORMAL).isNull()
              ? *icon_
              : gfx::ImageSkia());
    } else {
      static int alignment = 0;
      label_button->SetHorizontalAlignment(
          static_cast<gfx::HorizontalAlignment>(++alignment % 3));
    }
  } else if (event.IsShiftDown()) {
    if (event.IsAltDown()) {
      // Toggle focusability.
      label_button_->IsAccessibilityFocusable()
          ? label_button_->SetFocusBehavior(View::FocusBehavior::NEVER)
          : label_button_->SetFocusForPlatform();
    }
  } else if (event.IsAltDown()) {
    label_button->SetIsDefault(!label_button->GetIsDefault());
  }
  example_view()->GetLayoutManager()->Layout(example_view());
}

void ButtonExample::ButtonPressed(Button* sender, const ui::Event& event) {
  if (sender == label_button_)
    LabelButtonPressed(label_button_, event);
  else if (sender == md_button_ || sender == md_default_button_)
    static_cast<Button*>(sender)->StartThrobbing(5);
  else if (sender == md_disabled_button_)
    LabelButtonPressed(md_disabled_button_, event);
  else
    PrintStatus("Image Button Pressed! count: %d", ++count_);
}

}  // namespace examples
}  // namespace views
