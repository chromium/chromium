// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/flex_layout_example.h"

#include <memory>

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
#include "ui/views/view_class_properties.h"

namespace views {
namespace examples {

FlexLayoutExample::FlexLayoutExample() : LayoutExampleBase("Flex Layout") {}

FlexLayoutExample::~FlexLayoutExample() = default;

void FlexLayoutExample::CreateAdditionalControls(int vertical_pos) {
  static const char* const orientation_values[2] = {"Horizontal", "Vertical"};
  static const char* const main_axis_values[3] = {"Start", "Center", "End"};
  static const char* const cross_axis_values[4] = {"Stretch", "Start", "Center",
                                                   "End"};

  orientation_ = CreateCombobox(base::ASCIIToUTF16("Orientation"),
                                orientation_values, 2, &vertical_pos);
  main_axis_alignment_ = CreateCombobox(base::ASCIIToUTF16("Main axis"),
                                        main_axis_values, 3, &vertical_pos);
  cross_axis_alignment_ = CreateCombobox(base::ASCIIToUTF16("Cross axis"),
                                         cross_axis_values, 4, &vertical_pos);

  between_child_spacing_ =
      CreateTextfield(base::ASCIIToUTF16("Child spacing"), &vertical_pos);

  CreateMarginsTextFields(base::ASCIIToUTF16("Interior margin"),
                          &interior_margin_, &vertical_pos);

  CreateMarginsTextFields(base::ASCIIToUTF16("Default margins"),
                          &default_child_margins_, &vertical_pos);

  collapse_margins_ =
      CreateCheckbox(base::ASCIIToUTF16("Collapse margins"), &vertical_pos);

  ignore_default_main_axis_margins_ = CreateCheckbox(
      base::ASCIIToUTF16("Ignore main axis margins"), &vertical_pos);

  layout_ = layout_panel()->SetLayoutManager(std::make_unique<FlexLayout>());
}

void FlexLayoutExample::OnPerformAction(Combobox* combobox) {
  static const LayoutOrientation orientations[2] = {
      LayoutOrientation::kHorizontal, LayoutOrientation::kVertical};
  static const LayoutAlignment main_axis_alignments[3] = {
      LayoutAlignment::kStart, LayoutAlignment::kCenter, LayoutAlignment::kEnd};
  static const LayoutAlignment cross_axis_alignments[4] = {
      LayoutAlignment::kStretch, LayoutAlignment::kStart,
      LayoutAlignment::kCenter, LayoutAlignment::kEnd};

  if (combobox == orientation_) {
    layout_->SetOrientation(orientations[combobox->GetSelectedIndex()]);
  } else if (combobox == main_axis_alignment_) {
    layout_->SetMainAxisAlignment(
        main_axis_alignments[combobox->GetSelectedIndex()]);
  } else if (combobox == cross_axis_alignment_) {
    layout_->SetCrossAxisAlignment(
        cross_axis_alignments[combobox->GetSelectedIndex()]);
  }
  RefreshLayoutPanel(false);
}

void FlexLayoutExample::ContentsChanged(Textfield* sender,
                                        const base::string16& new_contents) {
  layout_->SetInteriorMargin(
      LayoutExampleBase::TextfieldsToInsets(interior_margin_));
  layout_->SetDefault(views::kMarginsKey, LayoutExampleBase::TextfieldsToInsets(
                                              default_child_margins_));
  if (sender == between_child_spacing_) {
    int spacing;
    if (base::StringToInt(between_child_spacing_->GetText(), &spacing))
      layout_->SetBetweenChildSpacing(spacing);
  }
  RefreshLayoutPanel(false);
}

void FlexLayoutExample::ButtonPressedImpl(Button* sender) {
  if (sender == collapse_margins_) {
    layout_->SetCollapseMargins(collapse_margins_->GetChecked());
  } else if (sender == ignore_default_main_axis_margins_) {
    layout_->SetIgnoreDefaultMainAxisMargins(
        ignore_default_main_axis_margins_->GetChecked());
  }
  RefreshLayoutPanel(false);
}

void FlexLayoutExample::UpdateLayoutManager() {
  for (View* child : layout_panel()->children()) {
    ChildPanel* panel = static_cast<ChildPanel*>(child);
    int flex = panel->GetFlex();
    if (flex < 0)
      panel->ClearProperty(views::kFlexBehaviorKey);
    else
      panel->SetProperty(views::kFlexBehaviorKey, GetFlexSpecification(flex));
  }
}

FlexSpecification FlexLayoutExample::GetFlexSpecification(int weight) const {
  return weight > 0
             ? FlexSpecification::ForSizeRule(MinimumFlexSizeRule::kScaleToZero,
                                              MaximumFlexSizeRule::kUnbounded)
                   .WithWeight(weight)
             : FlexSpecification::ForSizeRule(
                   MinimumFlexSizeRule::kPreferredSnapToZero,
                   MaximumFlexSizeRule::kPreferred)
                   .WithWeight(0);
}

}  // namespace examples
}  // namespace views
