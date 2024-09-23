// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/box_layout_example.h"

#include <memory>
#include <string>
#include <utility>

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

namespace views::examples {

BoxLayoutExample::BoxLayoutExample() : LayoutExampleBase("Box Layout") {}

BoxLayoutExample::~BoxLayoutExample() {
  if (between_child_spacing_) {
    between_child_spacing_->set_controller(nullptr);
  }
  if (default_flex_) {
    default_flex_->set_controller(nullptr);
  }
  if (min_cross_axis_size_) {
    min_cross_axis_size_->set_controller(nullptr);
  }
  border_insets_.ResetControllers();
}

void BoxLayoutExample::ContentsChanged(Textfield* textfield,
                                       const std::u16string& new_contents) {
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

void BoxLayoutExample::CreateAdditionalControls() {
  constexpr auto kOrientationValues =
      std::to_array<const char* const>({"Horizontal", "Vertical"});
  orientation_ = CreateAndAddCombobox(
      u"Orientation", kOrientationValues,
      base::BindRepeating(&LayoutExampleBase::RefreshLayoutPanel,
                          base::Unretained(this), true));

  constexpr auto kMainAxisValues =
      std::to_array<const char* const>({"Start", "Center", "End"});
  main_axis_alignment_ = CreateAndAddCombobox(
      u"Main axis", kMainAxisValues,
      base::BindRepeating(&BoxLayoutExample::MainAxisAlignmentChanged,
                          base::Unretained(this)));

  constexpr auto kCrossAxisValues =
      std::to_array<const char* const>({"Start", "Center", "End", "Stretch"});
  cross_axis_alignment_ = CreateAndAddCombobox(
      u"Cross axis", kCrossAxisValues,
      base::BindRepeating(&BoxLayoutExample::CrossAxisAlignmentChanged,
                          base::Unretained(this)));
  // Select Stretch as the default.
  cross_axis_alignment_->SetSelectedIndex(3);

  between_child_spacing_ = CreateAndAddTextfield(u"Child spacing");
  default_flex_ = CreateAndAddTextfield(u"Default flex");
  min_cross_axis_size_ = CreateAndAddTextfield(u"Min cross axis");

  CreateMarginsTextFields(u"Insets", &border_insets_);

  collapse_margins_ = CreateAndAddCheckbox(
      u"Collapse margins",
      base::BindRepeating(&LayoutExampleBase::RefreshLayoutPanel,
                          base::Unretained(this), true));

  UpdateLayoutManager();
}

void BoxLayoutExample::UpdateLayoutManager() {
  View* const panel = layout_panel();
  int child_spacing;
  base::StringToInt(between_child_spacing_->GetText(), &child_spacing);
  layout_ = nullptr;
  layout_ = panel->SetLayoutManager(std::make_unique<BoxLayout>(
      orientation_->GetSelectedIndex() == 0u
          ? BoxLayout::Orientation::kHorizontal
          : BoxLayout::Orientation::kVertical,
      gfx::Insets(), child_spacing, collapse_margins_->GetChecked()));

  layout_->set_cross_axis_alignment(static_cast<BoxLayout::CrossAxisAlignment>(
      cross_axis_alignment_->GetSelectedIndex().value()));
  layout_->set_main_axis_alignment(static_cast<BoxLayout::MainAxisAlignment>(
      main_axis_alignment_->GetSelectedIndex().value()));

  int default_flex;
  base::StringToInt(default_flex_->GetText(), &default_flex);
  layout_->SetDefaultFlex(default_flex);

  int min_cross_size;
  base::StringToInt(min_cross_axis_size_->GetText(), &min_cross_size);
  layout_->set_minimum_cross_axis_size(min_cross_size);

  UpdateBorderInsets();

  for (View* child : panel->children()) {
    const int flex = static_cast<ChildPanel*>(child)->GetFlex();
    if (flex < 0)
      layout_->ClearFlexForView(child);
    else
      layout_->SetFlexForView(child, flex);
  }
}

void BoxLayoutExample::UpdateBorderInsets() {
  layout_->set_inside_border_insets(TextfieldsToInsets(border_insets_));
}

void BoxLayoutExample::MainAxisAlignmentChanged() {
  layout_->set_main_axis_alignment(static_cast<BoxLayout::MainAxisAlignment>(
      main_axis_alignment_->GetSelectedIndex().value()));
  RefreshLayoutPanel(false);
}

void BoxLayoutExample::CrossAxisAlignmentChanged() {
  layout_->set_cross_axis_alignment(static_cast<BoxLayout::CrossAxisAlignment>(
      cross_axis_alignment_->GetSelectedIndex().value()));
  RefreshLayoutPanel(false);
}

}  // namespace views::examples
