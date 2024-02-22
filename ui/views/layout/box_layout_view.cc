// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/box_layout_view.h"

#include <memory>

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/layout/layout_manager.h"

namespace views {

BoxLayoutView::BoxLayoutView()
    : layout_(SetLayoutManager(std::make_unique<BoxLayout>())),
      orientation_(layout_->GetOrientation()),
      main_axis_alignment_(layout_->main_axis_alignment()),
      cross_axis_alignment_(layout_->cross_axis_alignment()),
      inside_border_insets_(layout_->inside_border_insets()),
      minimum_cross_axis_size_(layout_->minimum_cross_axis_size()),
      between_child_spacing_(layout_->between_child_spacing()),
      collapse_margins_spacing_(layout_->GetCollapseMarginsSpacing()),
      default_flex_(layout_->GetDefaultFlex()) {}

void BoxLayoutView::SetOrientation(BoxLayout::Orientation orientation) {
  if (orientation_ == orientation)
    return;
  layout_->SetOrientation(orientation);
  orientation_ = orientation;
  OnPropertyChanged(&orientation_, kPropertyEffectsLayout);
}

BoxLayout::Orientation BoxLayoutView::GetOrientation() const {
  return orientation_;
}

void BoxLayoutView::SetMainAxisAlignment(
    BoxLayout::MainAxisAlignment main_axis_alignment) {
  if (main_axis_alignment_ == main_axis_alignment)
    return;
  layout_->set_main_axis_alignment(main_axis_alignment);
  main_axis_alignment_ = main_axis_alignment;
  OnPropertyChanged(&main_axis_alignment_, kPropertyEffectsLayout);
}

BoxLayout::MainAxisAlignment BoxLayoutView::GetMainAxisAlignment() const {
  return main_axis_alignment_;
}

void BoxLayoutView::SetCrossAxisAlignment(
    BoxLayout::CrossAxisAlignment cross_axis_alignment) {
  if (cross_axis_alignment_ == cross_axis_alignment)
    return;
  layout_->set_cross_axis_alignment(cross_axis_alignment);
  cross_axis_alignment_ = cross_axis_alignment;
  OnPropertyChanged(&cross_axis_alignment_, kPropertyEffectsLayout);
}

BoxLayout::CrossAxisAlignment BoxLayoutView::GetCrossAxisAlignment() const {
  return cross_axis_alignment_;
}

void BoxLayoutView::SetInsideBorderInsets(const gfx::Insets& insets) {
  if (inside_border_insets_ == insets)
    return;
  layout_->set_inside_border_insets(insets);
  inside_border_insets_ = insets;
  OnPropertyChanged(&inside_border_insets_, kPropertyEffectsLayout);
}

const gfx::Insets& BoxLayoutView::GetInsideBorderInsets() const {
  return inside_border_insets_;
}

void BoxLayoutView::SetMinimumCrossAxisSize(int size) {
  if (minimum_cross_axis_size_ == size)
    return;
  layout_->set_minimum_cross_axis_size(size);
  minimum_cross_axis_size_ = size;
  OnPropertyChanged(&minimum_cross_axis_size_, kPropertyEffectsLayout);
}

int BoxLayoutView::GetMinimumCrossAxisSize() const {
  return minimum_cross_axis_size_;
}

void BoxLayoutView::SetBetweenChildSpacing(int spacing) {
  if (between_child_spacing_ == spacing)
    return;
  layout_->set_between_child_spacing(spacing);
  between_child_spacing_ = spacing;
  OnPropertyChanged(&between_child_spacing_, kPropertyEffectsLayout);
}

int BoxLayoutView::GetBetweenChildSpacing() const {
  return between_child_spacing_;
}

void BoxLayoutView::SetCollapseMarginsSpacing(bool collapse_margins_spacing) {
  if (collapse_margins_spacing_ == collapse_margins_spacing)
    return;
  layout_->SetCollapseMarginsSpacing(collapse_margins_spacing);
  collapse_margins_spacing_ = collapse_margins_spacing;
  OnPropertyChanged(&collapse_margins_spacing_, kPropertyEffectsLayout);
}

bool BoxLayoutView::GetCollapseMarginsSpacing() const {
  return collapse_margins_spacing_;
}

void BoxLayoutView::SetDefaultFlex(int default_flex) {
  if (default_flex_ == default_flex)
    return;
  layout_->SetDefaultFlex(default_flex);
  default_flex_ = default_flex;
  OnPropertyChanged(&default_flex_, kPropertyEffectsLayout);
}

int BoxLayoutView::GetDefaultFlex() const {
  return default_flex_;
}

void BoxLayoutView::SetFlexForView(const View* view,
                                   int flex,
                                   bool use_min_size) {
  layout_->SetFlexForView(view, flex, use_min_size);
  InvalidateLayout();
}

void BoxLayoutView::ClearFlexForView(const View* view) {
  layout_->ClearFlexForView(view);
  InvalidateLayout();
}

BEGIN_METADATA(BoxLayoutView)
ADD_PROPERTY_METADATA(BoxLayout::Orientation, Orientation)
ADD_PROPERTY_METADATA(BoxLayout::MainAxisAlignment, MainAxisAlignment)
ADD_PROPERTY_METADATA(BoxLayout::CrossAxisAlignment, CrossAxisAlignment)
ADD_PROPERTY_METADATA(gfx::Insets, InsideBorderInsets)
ADD_PROPERTY_METADATA(int, MinimumCrossAxisSize)
ADD_PROPERTY_METADATA(int, BetweenChildSpacing)
ADD_PROPERTY_METADATA(int, CollapseMarginsSpacing)
ADD_PROPERTY_METADATA(int, DefaultFlex)
END_METADATA

}  // namespace views
