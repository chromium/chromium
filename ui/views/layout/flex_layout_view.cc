// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/flex_layout_view.h"

#include <memory>

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/metadata/type_conversion.h"

namespace views {

FlexLayoutView::FlexLayoutView()
    : layout_(SetLayoutManager(std::make_unique<FlexLayout>())),
      orientation_(layout_->orientation()),
      main_axis_alignment_(layout_->main_axis_alignment()),
      cross_axis_alignment_(layout_->cross_axis_alignment()),
      interior_margin_(layout_->interior_margin()),
      minimum_cross_axis_size_(layout_->minimum_cross_axis_size()),
      collapse_margins_(layout_->collapse_margins()),
      include_host_insets_in_layout_(layout_->include_host_insets_in_layout()),
      ignore_default_main_axis_margins_(
          layout_->ignore_default_main_axis_margins()),
      flex_allocation_order_(layout_->flex_allocation_order()) {}

FlexLayoutView::~FlexLayoutView() = default;

void FlexLayoutView::SetOrientation(LayoutOrientation orientation) {
  if (orientation_ == orientation)
    return;
  layout_->SetOrientation(orientation);
  orientation_ = orientation;
  OnPropertyChanged(&orientation_, kPropertyEffectsLayout);
}

LayoutOrientation FlexLayoutView::GetOrientation() const {
  return orientation_;
}

void FlexLayoutView::SetMainAxisAlignment(LayoutAlignment main_axis_alignment) {
  if (main_axis_alignment_ == main_axis_alignment)
    return;
  layout_->SetMainAxisAlignment(main_axis_alignment);
  main_axis_alignment_ = main_axis_alignment;
  OnPropertyChanged(&main_axis_alignment_, kPropertyEffectsLayout);
}

LayoutAlignment FlexLayoutView::GetMainAxisAlignment() const {
  return layout_->main_axis_alignment();
}

void FlexLayoutView::SetCrossAxisAlignment(
    LayoutAlignment cross_axis_alignment) {
  if (cross_axis_alignment_ == cross_axis_alignment)
    return;
  layout_->SetCrossAxisAlignment(cross_axis_alignment);
  cross_axis_alignment_ = cross_axis_alignment;
  OnPropertyChanged(&cross_axis_alignment_, kPropertyEffectsLayout);
}

LayoutAlignment FlexLayoutView::GetCrossAxisAlignment() const {
  return cross_axis_alignment_;
}

void FlexLayoutView::SetInteriorMargin(const gfx::Insets& interior_margin) {
  if (interior_margin_ == interior_margin)
    return;
  layout_->SetInteriorMargin(interior_margin);
  interior_margin_ = interior_margin;
  OnPropertyChanged(&interior_margin_, kPropertyEffectsLayout);
}

const gfx::Insets& FlexLayoutView::GetInteriorMargin() const {
  return interior_margin_;
}

void FlexLayoutView::SetMinimumCrossAxisSize(int size) {
  if (minimum_cross_axis_size_ == size)
    return;
  layout_->SetMinimumCrossAxisSize(size);
  minimum_cross_axis_size_ = size;
  OnPropertyChanged(&minimum_cross_axis_size_, kPropertyEffectsLayout);
}

int FlexLayoutView::GetMinimumCrossAxisSize() const {
  return minimum_cross_axis_size_;
}

void FlexLayoutView::SetCollapseMargins(bool collapse_margins) {
  if (collapse_margins_ == collapse_margins)
    return;
  layout_->SetCollapseMargins(collapse_margins);
  collapse_margins_ = collapse_margins;
  OnPropertyChanged(&collapse_margins_, kPropertyEffectsLayout);
}

bool FlexLayoutView::GetCollapseMargins() const {
  return collapse_margins_;
}

void FlexLayoutView::SetIncludeHostInsetsInLayout(
    bool include_host_insets_in_layout) {
  if (include_host_insets_in_layout_ == include_host_insets_in_layout)
    return;
  layout_->SetIncludeHostInsetsInLayout(include_host_insets_in_layout);
  include_host_insets_in_layout_ = include_host_insets_in_layout;
  OnPropertyChanged(&include_host_insets_in_layout_, kPropertyEffectsLayout);
}

bool FlexLayoutView::GetIncludeHostInsetsInLayout() const {
  return include_host_insets_in_layout_;
}

void FlexLayoutView::SetIgnoreDefaultMainAxisMargins(
    bool ignore_default_main_axis_margins) {
  if (ignore_default_main_axis_margins == ignore_default_main_axis_margins_) {
    return;
  }
  layout_->SetIgnoreDefaultMainAxisMargins(ignore_default_main_axis_margins);
  ignore_default_main_axis_margins_ = ignore_default_main_axis_margins;
  OnPropertyChanged(&ignore_default_main_axis_margins_, kPropertyEffectsLayout);
}

bool FlexLayoutView::GetIgnoreDefaultMainAxisMargins() const {
  return ignore_default_main_axis_margins_;
}

void FlexLayoutView::SetFlexAllocationOrder(
    FlexAllocationOrder flex_allocation_order) {
  if (flex_allocation_order_ == flex_allocation_order)
    return;
  layout_->SetFlexAllocationOrder(flex_allocation_order);
  flex_allocation_order_ = flex_allocation_order;
  OnPropertyChanged(&flex_allocation_order_, kPropertyEffectsLayout);
}

FlexAllocationOrder FlexLayoutView::GetFlexAllocationOrder() const {
  return flex_allocation_order_;
}

FlexRule FlexLayoutView::GetDefaultFlexRule() const {
  return layout_->GetDefaultFlexRule();
}

BEGIN_METADATA(FlexLayoutView)
ADD_PROPERTY_METADATA(LayoutOrientation, Orientation)
ADD_PROPERTY_METADATA(LayoutAlignment, MainAxisAlignment)
ADD_PROPERTY_METADATA(LayoutAlignment, CrossAxisAlignment)
ADD_PROPERTY_METADATA(const gfx::Insets, InteriorMargin)
ADD_PROPERTY_METADATA(int, MinimumCrossAxisSize)
ADD_PROPERTY_METADATA(bool, CollapseMargins)
ADD_PROPERTY_METADATA(bool, IncludeHostInsetsInLayout)
ADD_PROPERTY_METADATA(bool, IgnoreDefaultMainAxisMargins)
ADD_PROPERTY_METADATA(FlexAllocationOrder, FlexAllocationOrder)
END_METADATA

}  // namespace views

DEFINE_ENUM_CONVERTERS(views::LayoutOrientation,
                       {views::LayoutOrientation::kHorizontal, u"kHorizontal"},
                       {views::LayoutOrientation::kVertical, u"kVertical"})

DEFINE_ENUM_CONVERTERS(views::LayoutAlignment,
                       {views::LayoutAlignment::kStart, u"kStart"},
                       {views::LayoutAlignment::kCenter, u"kCenter"},
                       {views::LayoutAlignment::kEnd, u"kEnd"},
                       {views::LayoutAlignment::kStretch, u"kStretch"})

DEFINE_ENUM_CONVERTERS(views::FlexAllocationOrder,
                       {views::FlexAllocationOrder::kNormal, u"kNormal"},
                       {views::FlexAllocationOrder::kReverse, u"kReverse"})
