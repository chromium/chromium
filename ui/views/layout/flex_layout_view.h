// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_FLEX_LAYOUT_VIEW_H_
#define UI_VIEWS_LAYOUT_FLEX_LAYOUT_VIEW_H_

#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace views {

class VIEWS_EXPORT FlexLayoutView : public View {
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
  }

  // Copies and uses |value| as the default value for layout property |key|.
  template <class T, class U>
  void SetDefault(const ui::ClassProperty<T>* key, const U& value) {
    layout_->SetDefault(key, value);
  }

 private:
  FlexLayout* layout_;
};

}  // namespace views

#endif  // UI_VIEWS_LAYOUT_FLEX_LAYOUT_VIEW_H_
