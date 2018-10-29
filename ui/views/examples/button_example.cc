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

ButtonExample::~ButtonExample() {
}

void ButtonExample::CreateExampleView(View* container) {
  container->SetBackground(CreateSolidBackground(SK_ColorWHITE));
  auto layout =
      std::make_unique<BoxLayout>(BoxLayout::kVertical, gfx::Insets(10), 10);
  layout->set_cross_axis_alignment(BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  container->SetLayoutManager(std::move(layout));

  label_button_ = new LabelButton(this, ASCIIToUTF16(kLabelButton));
  label_button_->SetFocusForPlatform();
  label_button_->set_request_focus_on_press(true);
  container->AddChildView(label_button_);

  styled_button_ = new LabelButton(this, ASCIIToUTF16("Styled Button"));
  styled_button_->SetStyleDeprecated(Button::STYLE_BUTTON);
  container->AddChildView(styled_button_);

  disabled_button_ = new LabelButton(this, ASCIIToUTF16("Disabled Button"));
  disabled_button_->SetStyleDeprecated(Button::STYLE_BUTTON);
  disabled_button_->SetState(Button::STATE_DISABLED);
  container->AddChildView(disabled_button_);

  md_button_ =
      MdTextButton::Create(this, base::ASCIIToUTF16("Material design"));
  container->AddChildView(md_button_);

  md_default_button_ =
      MdTextButton::Create(this, base::ASCIIToUTF16("Default"));
  md_default_button_->SetIsDefault(true);
  container->AddChildView(md_default_button_);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  image_button_ = new ImageButton(this);
  image_button_->SetFocusForPlatform();
  image_button_->set_request_focus_on_press(true);
  image_button_->SetImage(ImageButton::STATE_NORMAL,
                          rb.GetImageNamed(IDR_CLOSE).ToImageSkia());
  image_button_->SetImage(ImageButton::STATE_HOVERED,
                          rb.GetImageNamed(IDR_CLOSE_H).ToImageSkia());
  image_button_->SetImage(ImageButton::STATE_PRESSED,
                          rb.GetImageNamed(IDR_CLOSE_P).ToImageSkia());
  container->AddChildView(image_button_);
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
    } else {
      label_button->SetStyleDeprecated(static_cast<Button::ButtonStyle>(
          (label_button->style() + 1) % Button::STYLE_COUNT));
    }
  } else if (event.IsAltDown()) {
    label_button->SetIsDefault(!label_button->is_default());
  }
  example_view()->GetLayoutManager()->Layout(example_view());
}

void ButtonExample::ButtonPressed(Button* sender, const ui::Event& event) {
  if (sender == label_button_)
    LabelButtonPressed(label_button_, event);
  else if (sender == styled_button_)
    LabelButtonPressed(styled_button_, event);
  else if (sender == disabled_button_)
    LabelButtonPressed(disabled_button_, event);
  else if (sender == md_button_ || sender == md_default_button_)
    static_cast<Button*>(sender)->StartThrobbing(5);
  else
    PrintStatus("Image Button Pressed! count: %d", ++count_);
}

}  // namespace examples
}  // namespace views
