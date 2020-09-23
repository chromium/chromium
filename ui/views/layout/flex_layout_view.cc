// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/flex_layout_view.h"

#include <memory>

namespace {

// An enum giving different RenderText properties unique keys for the
// OnPropertyChanged call.
enum LabelPropertyKey {
  kOrientation = 1,
  kMainAxisAlignment,
  kCrossAxisAlignment,
  kInteriorMargin,
  kMinimumCrossAxisSize,
  kCollapseMargins,
  kIncludeHostInsetsInLayout,
  kIgnoreMainAxisMargins,
  kFlexAllocationOrder,
};

}  // namespace

namespace views {

FlexLayoutView::FlexLayoutView()
    : layout_(SetLayoutManager(std::make_unique<FlexLayout>())) {}

FlexLayoutView::~FlexLayoutView() = default;

void FlexLayoutView::SetOrientation(LayoutOrientation orientation) {
  if (orientation == layout_->orientation())
    return;
  layout_->SetOrientation(orientation);
  OnPropertyChanged(&layout_ + kOrientation, kPropertyEffectsLayout);
}

LayoutOrientation FlexLayoutView::GetOrientation() const {
  return layout_->orientation();
}

void FlexLayoutView::SetMainAxisAlignment(LayoutAlignment main_axis_alignment) {
  if (main_axis_alignment == layout_->main_axis_alignment())
    return;
  layout_->SetMainAxisAlignment(main_axis_alignment);
  OnPropertyChanged(&layout_ + kMainAxisAlignment, kPropertyEffectsLayout);
}

LayoutAlignment FlexLayoutView::GetMainAxisAlignment() const {
  return layout_->main_axis_alignment();
}

void FlexLayoutView::SetCrossAxisAlignment(
    LayoutAlignment cross_axis_alignment) {
  if (cross_axis_alignment == layout_->cross_axis_alignment())
    return;
  layout_->SetCrossAxisAlignment(cross_axis_alignment);
  OnPropertyChanged(&layout_ + kCrossAxisAlignment, kPropertyEffectsLayout);
}

LayoutAlignment FlexLayoutView::GetCrossAxisAlignment() const {
  return layout_->cross_axis_alignment();
}

void FlexLayoutView::SetInteriorMargin(const gfx::Insets& interior_margin) {
  if (interior_margin == layout_->interior_margin())
    return;
  layout_->SetInteriorMargin(interior_margin);
  OnPropertyChanged(&layout_ + kInteriorMargin, kPropertyEffectsLayout);
}

const gfx::Insets& FlexLayoutView::GetInteriorMargin() const {
  return layout_->interior_margin();
}

void FlexLayoutView::SetMinimumCrossAxisSize(int size) {
  if (size == layout_->minimum_cross_axis_size())
    return;
  layout_->SetMinimumCrossAxisSize(size);
  OnPropertyChanged(&layout_ + kMinimumCrossAxisSize, kPropertyEffectsLayout);
}

int FlexLayoutView::GetMinimumCrossAxisSize() const {
  return layout_->minimum_cross_axis_size();
}

void FlexLayoutView::SetCollapseMargins(bool collapse_margins) {
  if (collapse_margins == layout_->collapse_margins())
    return;
  layout_->SetCollapseMargins(collapse_margins);
  OnPropertyChanged(&layout_ + kCollapseMargins, kPropertyEffectsLayout);
}

bool FlexLayoutView::GetCollapseMargins() const {
  return layout_->collapse_margins();
}

void FlexLayoutView::SetIncludeHostInsetsInLayout(
    bool include_host_insets_in_layout) {
  if (include_host_insets_in_layout == layout_->include_host_insets_in_layout())
    return;
  layout_->SetIncludeHostInsetsInLayout(include_host_insets_in_layout);
  OnPropertyChanged(&layout_ + kIncludeHostInsetsInLayout,
                    kPropertyEffectsLayout);
}

bool FlexLayoutView::GetIncludeHostInsetsInLayout() const {
  return layout_->include_host_insets_in_layout();
}

void FlexLayoutView::SetIgnoreDefaultMainAxisMargins(
    bool ignore_default_main_axis_margins) {
  if (ignore_default_main_axis_margins ==
      layout_->ignore_default_main_axis_margins()) {
    return;
  }
  layout_->SetIgnoreDefaultMainAxisMargins(ignore_default_main_axis_margins);
  OnPropertyChanged(&layout_ + kIgnoreMainAxisMargins, kPropertyEffectsLayout);
}

bool FlexLayoutView::GetIgnoreDefaultMainAxisMargins() const {
  return layout_->ignore_default_main_axis_margins();
}

void FlexLayoutView::SetFlexAllocationOrder(
    FlexAllocationOrder flex_allocation_order) {
  if (flex_allocation_order == layout_->flex_allocation_order())
    return;
  layout_->SetFlexAllocationOrder(flex_allocation_order);
  OnPropertyChanged(&layout_ + kFlexAllocationOrder, kPropertyEffectsLayout);
}

FlexAllocationOrder FlexLayoutView::GetFlexAllocationOrder() const {
  return layout_->flex_allocation_order();
}

FlexRule FlexLayoutView::GetDefaultFlexRule() const {
  return layout_->GetDefaultFlexRule();
}

}  // namespace views
