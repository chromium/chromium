// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/flex_layout_example.h"

#include <memory>
#include <string>

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

namespace views::examples {

FlexLayoutExample::FlexLayoutExample() : LayoutExampleBase("Flex Layout") {}

FlexLayoutExample::~FlexLayoutExample() = default;

void FlexLayoutExample::ContentsChanged(Textfield* sender,
                                        const std::u16string& new_contents) {
  layout_->SetInteriorMargin(
      LayoutExampleBase::TextfieldsToInsets(interior_margin_));
  layout_->SetDefault(views::kMarginsKey, LayoutExampleBase::TextfieldsToInsets(
                                              default_child_margins_));
  RefreshLayoutPanel(false);
}

void FlexLayoutExample::CreateAdditionalControls() {
  constexpr const char* kOrientationValues[2] = {"Horizontal", "Vertical"};
  orientation_ = CreateAndAddCombobox(
      u"Orientation", kOrientationValues, std::size(kOrientationValues),
      base::BindRepeating(&FlexLayoutExample::OrientationChanged,
                          base::Unretained(this)));

  constexpr const char* kMainAxisValues[3] = {"Start", "Center", "End"};
  main_axis_alignment_ = CreateAndAddCombobox(
      u"Main axis", kMainAxisValues, std::size(kMainAxisValues),
      base::BindRepeating(&FlexLayoutExample::MainAxisAlignmentChanged,
                          base::Unretained(this)));

  constexpr const char* kCrossAxisValues[4] = {"Stretch", "Start", "Center",
                                               "End"};
  cross_axis_alignment_ = CreateAndAddCombobox(
      u"Cross axis", kCrossAxisValues, std::size(kCrossAxisValues),
      base::BindRepeating(&FlexLayoutExample::CrossAxisAlignmentChanged,
                          base::Unretained(this)));

  CreateMarginsTextFields(u"Interior margin", &interior_margin_);

  CreateMarginsTextFields(u"Default margins", &default_child_margins_);

  collapse_margins_ = CreateAndAddCheckbox(
      u"Collapse margins", base::BindRepeating(
                               [](FlexLayoutExample* example) {
                                 example->layout_->SetCollapseMargins(
                                     example->collapse_margins_->GetChecked());
                                 example->RefreshLayoutPanel(false);
                               },
                               base::Unretained(this)));

  ignore_default_main_axis_margins_ = CreateAndAddCheckbox(
      u"Ignore main axis margins",
      base::BindRepeating(
          [](FlexLayoutExample* example) {
            example->layout_->SetIgnoreDefaultMainAxisMargins(
                example->ignore_default_main_axis_margins_->GetChecked());
            example->RefreshLayoutPanel(false);
          },
          base::Unretained(this)));

  layout_ = layout_panel()->SetLayoutManager(std::make_unique<FlexLayout>());
}

void FlexLayoutExample::UpdateLayoutManager() {
  for (View* child : layout_panel()->children()) {
    const int flex = static_cast<ChildPanel*>(child)->GetFlex();
    if (flex < 0)
      child->ClearProperty(views::kFlexBehaviorKey);
    else
      child->SetProperty(views::kFlexBehaviorKey, GetFlexSpecification(flex));
  }
}

FlexSpecification FlexLayoutExample::GetFlexSpecification(int weight) const {
  return weight > 0
             ? FlexSpecification(MinimumFlexSizeRule::kScaleToZero,
                                 MaximumFlexSizeRule::kUnbounded)
                   .WithWeight(weight)
             : FlexSpecification(MinimumFlexSizeRule::kPreferredSnapToZero,
                                 MaximumFlexSizeRule::kPreferred)
                   .WithWeight(0);
}

void FlexLayoutExample::OrientationChanged() {
  constexpr LayoutOrientation kOrientations[2] = {
      LayoutOrientation::kHorizontal, LayoutOrientation::kVertical};
  layout_->SetOrientation(
      kOrientations[orientation_->GetSelectedIndex().value()]);
  RefreshLayoutPanel(false);
}

void FlexLayoutExample::MainAxisAlignmentChanged() {
  constexpr LayoutAlignment kMainAxisAlignments[3] = {
      LayoutAlignment::kStart, LayoutAlignment::kCenter, LayoutAlignment::kEnd};
  layout_->SetMainAxisAlignment(
      kMainAxisAlignments[main_axis_alignment_->GetSelectedIndex().value()]);
  RefreshLayoutPanel(false);
}

void FlexLayoutExample::CrossAxisAlignmentChanged() {
  constexpr LayoutAlignment kCrossAxisAlignments[4] = {
      LayoutAlignment::kStretch, LayoutAlignment::kStart,
      LayoutAlignment::kCenter, LayoutAlignment::kEnd};
  layout_->SetCrossAxisAlignment(
      kCrossAxisAlignments[cross_axis_alignment_->GetSelectedIndex().value()]);
  RefreshLayoutPanel(false);
}

}  // namespace views::examples
