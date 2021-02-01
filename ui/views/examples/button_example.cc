// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/button_example.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/resources/grit/views_resources.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

using base::ASCIIToUTF16;

namespace {
const char kLabelButton[] = "Label Button";
const char kLongText[] =
    "Start of Really Really Really Really Really Really "
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
  container->SetLayoutManager(std::make_unique<FillLayout>());
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  auto start_throbber_cb = [](MdTextButton* button) {
    button->StartThrobbing(5);
  };
  auto view =
      Builder<BoxLayoutView>()
          .SetOrientation(BoxLayout::Orientation::kVertical)
          .SetInsideBorderInsets(gfx::Insets(10))
          .SetBetweenChildSpacing(10)
          .SetCrossAxisAlignment(BoxLayout::CrossAxisAlignment::kCenter)
          .SetBackground(CreateSolidBackground(SK_ColorWHITE))
          .AddChildren(
              {Builder<LabelButton>()
                   .CopyAddressTo(&label_button_)
                   .SetText(ASCIIToUTF16(kLabelButton))
                   .SetRequestFocusOnPress(true)
                   .SetCallback(base::BindRepeating(
                       &ButtonExample::LabelButtonPressed,
                       base::Unretained(this), label_button_)),
               Builder<MdTextButton>()
                   .CopyAddressTo(&md_button_)
                   .SetText(base::ASCIIToUTF16("Material Design"))
                   .SetCallback(
                       base::BindRepeating(start_throbber_cb, md_button_)),
               Builder<MdTextButton>()
                   .CopyAddressTo(&md_disabled_button_)
                   .SetText(ASCIIToUTF16("Material Design Disabled Button"))
                   .SetState(Button::STATE_DISABLED)
                   .SetCallback(base::BindRepeating(start_throbber_cb,
                                                    md_disabled_button_)),
               Builder<MdTextButton>()
                   .CopyAddressTo(&md_default_button_)
                   .SetText(base::ASCIIToUTF16("Default"))
                   .SetIsDefault(true)
                   .SetCallback(base::BindRepeating(start_throbber_cb,
                                                    md_default_button_)),
               Builder<ImageButton>()
                   .CopyAddressTo(&image_button_)
                   .SetRequestFocusOnPress(true)
                   .SetCallback(
                       base::BindRepeating(&ButtonExample::ImageButtonPressed,
                                           base::Unretained(this)))})
          .Build();

  image_button_->SetImage(ImageButton::STATE_NORMAL,
                          rb.GetImageNamed(IDR_CLOSE).ToImageSkia());
  image_button_->SetImage(ImageButton::STATE_HOVERED,
                          rb.GetImageNamed(IDR_CLOSE_H).ToImageSkia());
  image_button_->SetImage(ImageButton::STATE_PRESSED,
                          rb.GetImageNamed(IDR_CLOSE_P).ToImageSkia());

  container->AddChildView(std::move(view));
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
      label_button->SetImageModel(
          Button::STATE_NORMAL,
          label_button->GetImage(Button::STATE_NORMAL).isNull()
              ? ui::ImageModel::FromImageSkia(*icon_)
              : ui::ImageModel());
    } else {
      static int alignment = 0;
      label_button->SetHorizontalAlignment(
          static_cast<gfx::HorizontalAlignment>(++alignment % 3));
    }
  } else if (event.IsShiftDown()) {
    if (event.IsAltDown()) {
      // Toggle focusability.
      label_button->IsAccessibilityFocusable()
          ? label_button->SetFocusBehavior(View::FocusBehavior::NEVER)
          : label_button->SetFocusBehavior(
                PlatformStyle::kDefaultFocusBehavior);
    }
  } else if (event.IsAltDown()) {
    label_button->SetIsDefault(!label_button->GetIsDefault());
  }
  example_view()->GetLayoutManager()->Layout(example_view());
  PrintViewHierarchy(example_view());
}

void ButtonExample::ImageButtonPressed() {
  PrintStatus("Image Button Pressed! count: %d", ++count_);
}

}  // namespace examples
}  // namespace views
