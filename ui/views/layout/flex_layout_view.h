// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_FLEX_LAYOUT_VIEW_H_
#define UI_VIEWS_LAYOUT_FLEX_LAYOUT_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace views {

class VIEWS_EXPORT FlexLayoutView : public View {
  METADATA_HEADER(FlexLayoutView, View)

 public:
  FlexLayoutView();
  FlexLayoutView(const FlexLayoutView&) = delete;
  FlexLayoutView operator=(const FlexLayoutView&) = delete;
  ~FlexLayoutView() override;

  void SetOrientation(LayoutOrientation orientation);
  LayoutOrientation GetOrientation() const;

  void SetMainAxisAlignment(LayoutAlignment main_axis_alignment);
  LayoutAlignment GetMainAxisAlignment() const;

  void SetCrossAxisAlignment(LayoutAlignment cross_axis_alignment);
  LayoutAlignment GetCrossAxisAlignment() const;

  void SetInteriorMargin(const gfx::Insets& interior_margin);
  const gfx::Insets& GetInteriorMargin() const;

  void SetMinimumCrossAxisSize(int size);
  int GetMinimumCrossAxisSize() const;

  void SetCollapseMargins(bool collapse_margins);
  bool GetCollapseMargins() const;

  void SetIncludeHostInsetsInLayout(bool include_host_insets_in_layout);
  bool GetIncludeHostInsetsInLayout() const;

  void SetIgnoreDefaultMainAxisMargins(bool ignore_default_main_axis_margins);
  bool GetIgnoreDefaultMainAxisMargins() const;

  void SetFlexAllocationOrder(FlexAllocationOrder flex_allocation_order);
  FlexAllocationOrder GetFlexAllocationOrder() const;

  // Returns a flex rule that allows flex layouts to be nested with expected
  // behavior.
  FlexRule GetDefaultFlexRule() const;

  // Moves and uses |value| as the default value for layout property |key|.
  template <class T, class U>
  void SetDefault(const ui::ClassProperty<T>* key, U&& value) {
    layout_->SetDefault(key, value);
    InvalidateLayout();
  }

  // Copies and uses |value| as the default value for layout property |key|.
  template <class T, class U>
  void SetDefault(const ui::ClassProperty<T>* key, const U& value) {
    layout_->SetDefault(key, value);
    InvalidateLayout();
  }

 private:
  raw_ptr<FlexLayout> layout_;
  LayoutOrientation orientation_;
  LayoutAlignment main_axis_alignment_;
  LayoutAlignment cross_axis_alignment_;
  gfx::Insets interior_margin_;
  int minimum_cross_axis_size_;
  bool collapse_margins_;
  bool include_host_insets_in_layout_;
  bool ignore_default_main_axis_margins_;
  FlexAllocationOrder flex_allocation_order_;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, FlexLayoutView, View)
VIEW_BUILDER_PROPERTY(LayoutOrientation, Orientation)
VIEW_BUILDER_PROPERTY(LayoutAlignment, MainAxisAlignment)
VIEW_BUILDER_PROPERTY(LayoutAlignment, CrossAxisAlignment)
VIEW_BUILDER_PROPERTY(const gfx::Insets, InteriorMargin)
VIEW_BUILDER_PROPERTY(int, MinimumCrossAxisSize)
VIEW_BUILDER_PROPERTY(bool, CollapseMargins)
VIEW_BUILDER_PROPERTY(bool, IncludeHostInsetsInLayout)
VIEW_BUILDER_PROPERTY(bool, IgnoreDefaultMainAxisMargins)
VIEW_BUILDER_PROPERTY(FlexAllocationOrder, FlexAllocationOrder)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, FlexLayoutView)

#endif  // UI_VIEWS_LAYOUT_FLEX_LAYOUT_VIEW_H_
