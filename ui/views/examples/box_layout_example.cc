// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/box_layout_example.h"

#include <memory>
#include <utility>

#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/models/combobox_model.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/examples/example_combobox_model.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace views {
namespace examples {

BoxLayoutExample::BoxLayoutExample() : LayoutExampleBase("Box Layout") {}

BoxLayoutExample::~BoxLayoutExample() = default;

void BoxLayoutExample::CreateAdditionalControls(int vertical_pos) {
  static const char* orientation_values[2] = {"Horizontal", "Vertical"};
  static const char* main_axis_values[3] = {"Start", "Center", "End"};
  static const char* cross_axis_values[4] = {"Stretch", "Start", "Center",
                                             "End"};

  orientation_ = CreateCombobox(base::ASCIIToUTF16("Orientation"),
                                orientation_values, 2, &vertical_pos);
  main_axis_alignment_ = CreateCombobox(base::ASCIIToUTF16("Main axis"),
                                        main_axis_values, 3, &vertical_pos);
  cross_axis_alignment_ = CreateCombobox(base::ASCIIToUTF16("Cross axis"),
                                         cross_axis_values, 4, &vertical_pos);

  between_child_spacing_ =
      CreateTextfield(base::ASCIIToUTF16("Child spacing"), &vertical_pos);
  default_flex_ =
      CreateTextfield(base::ASCIIToUTF16("Default flex"), &vertical_pos);
  min_cross_axis_size_ =
      CreateTextfield(base::ASCIIToUTF16("Min cross axis"), &vertical_pos);

  CreateMarginsTextFields(base::ASCIIToUTF16("Insets"), &border_insets_,
                          &vertical_pos);

  collapse_margins_ =
      CreateCheckbox(base::ASCIIToUTF16("Collapse margins"), &vertical_pos);

  UpdateLayoutManager();
}

void BoxLayoutExample::ButtonPressedImpl(Button* sender) {
  if (sender == collapse_margins_) {
    RefreshLayoutPanel(true);
  }
}

void BoxLayoutExample::OnPerformAction(Combobox* combobox) {
  if (combobox == orientation_) {
    UpdateLayoutManager();
  } else if (combobox == main_axis_alignment_) {
    layout_->set_main_axis_alignment(static_cast<BoxLayout::MainAxisAlignment>(
        main_axis_alignment_->GetSelectedIndex()));
  } else if (combobox == cross_axis_alignment_) {
    layout_->set_cross_axis_alignment(
        static_cast<BoxLayout::CrossAxisAlignment>(
            cross_axis_alignment_->GetSelectedIndex()));
  }
  RefreshLayoutPanel(false);
}

void BoxLayoutExample::ContentsChanged(Textfield* textfield,
                                       const base::string16& new_contents) {
  if (textfield == between_child_spacing_) {
    UpdateLayoutManager();
  } else if (textfield == default_flex_) {
    int default_flex;
    base::StringToInt(default_flex_->GetText(), &default_flex);
    layout_->SetDefaultFlex(default_flex);
  } else if (textfield == min_cross_axis_size_) {
    int min_cross_size;
    base::StringToInt(min_cross_axis_size_->GetText(), &min_cross_size);
    layout_->set_minimum_cross_axis_size(min_cross_size);
  } else if (textfield == border_insets_.left ||
             textfield == border_insets_.top ||
             textfield == border_insets_.right ||
             textfield == border_insets_.bottom) {
    UpdateBorderInsets();
  }
  RefreshLayoutPanel(false);
}

void BoxLayoutExample::UpdateBorderInsets() {
  layout_->set_inside_border_insets(TextfieldsToInsets(border_insets_));
}

void BoxLayoutExample::UpdateLayoutManager() {
  int child_spacing;
  int default_flex;
  int min_cross_size;
  base::StringToInt(between_child_spacing_->GetText(), &child_spacing);
  base::StringToInt(default_flex_->GetText(), &default_flex);
  base::StringToInt(min_cross_axis_size_->GetText(), &min_cross_size);
  auto layout = std::make_unique<BoxLayout>(
      orientation_->GetSelectedIndex() == 0
          ? BoxLayout::Orientation::kHorizontal
          : BoxLayout::Orientation::kVertical,
      gfx::Insets(0, 0), child_spacing, collapse_margins_->GetChecked());
  layout->set_cross_axis_alignment(static_cast<BoxLayout::CrossAxisAlignment>(
      cross_axis_alignment_->GetSelectedIndex()));
  layout->set_main_axis_alignment(static_cast<BoxLayout::MainAxisAlignment>(
      main_axis_alignment_->GetSelectedIndex()));
  layout->SetDefaultFlex(default_flex);
  layout->set_minimum_cross_axis_size(min_cross_size);
  View* const panel = layout_panel();
  layout_ = panel->SetLayoutManager(std::move(layout));
  UpdateBorderInsets();
  for (View* child : panel->children()) {
    ChildPanel* child_panel = static_cast<ChildPanel*>(child);
    int flex = child_panel->GetFlex();
    if (flex < 0)
      layout_->ClearFlexForView(child_panel);
    else
      layout_->SetFlexForView(child_panel, flex);
  }
}

}  // namespace examples
}  // namespace views
