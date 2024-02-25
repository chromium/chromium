// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_BOX_LAYOUT_VIEW_H_
#define UI_VIEWS_LAYOUT_BOX_LAYOUT_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

namespace gfx {
class Insets;
}  // namespace gfx

namespace views {

class VIEWS_EXPORT BoxLayoutView : public View {
  METADATA_HEADER(BoxLayoutView, View)

 public:
  BoxLayoutView();
  BoxLayoutView(BoxLayoutView&) = delete;
  BoxLayoutView& operator=(BoxLayoutView&) = delete;
  ~BoxLayoutView() override = default;

  void SetOrientation(BoxLayout::Orientation orientation);
  BoxLayout::Orientation GetOrientation() const;

  void SetMainAxisAlignment(BoxLayout::MainAxisAlignment main_axis_alignment);
  BoxLayout::MainAxisAlignment GetMainAxisAlignment() const;

  void SetCrossAxisAlignment(
      BoxLayout::CrossAxisAlignment cross_axis_alignment);
  BoxLayout::CrossAxisAlignment GetCrossAxisAlignment() const;

  void SetInsideBorderInsets(const gfx::Insets& insets);
  const gfx::Insets& GetInsideBorderInsets() const;

  void SetMinimumCrossAxisSize(int size);
  int GetMinimumCrossAxisSize() const;

  void SetBetweenChildSpacing(int spacing);
  int GetBetweenChildSpacing() const;

  void SetCollapseMarginsSpacing(bool collapse_margins_spacing);
  bool GetCollapseMarginsSpacing() const;

  void SetDefaultFlex(int default_flex);
  int GetDefaultFlex() const;

  void SetFlexForView(const View* view, int flex, bool use_min_size = false);
  void ClearFlexForView(const View* view);

 private:
  const raw_ptr<BoxLayout> layout_;

  // TODO(tluk): Merge these with the values in BoxLayout after transition to
  // layout views is complete.
  BoxLayout::Orientation orientation_;
  BoxLayout::MainAxisAlignment main_axis_alignment_;
  BoxLayout::CrossAxisAlignment cross_axis_alignment_;
  gfx::Insets inside_border_insets_;
  int minimum_cross_axis_size_;
  int between_child_spacing_;
  int collapse_margins_spacing_;
  int default_flex_;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, BoxLayoutView, View)
VIEW_BUILDER_PROPERTY(BoxLayout::Orientation, Orientation)
VIEW_BUILDER_PROPERTY(BoxLayout::MainAxisAlignment, MainAxisAlignment)
VIEW_BUILDER_PROPERTY(BoxLayout::CrossAxisAlignment, CrossAxisAlignment)
VIEW_BUILDER_PROPERTY(const gfx::Insets, InsideBorderInsets)
VIEW_BUILDER_PROPERTY(int, MinimumCrossAxisSize)
VIEW_BUILDER_PROPERTY(int, BetweenChildSpacing)
VIEW_BUILDER_PROPERTY(int, CollapseMarginsSpacing)
VIEW_BUILDER_PROPERTY(int, DefaultFlex)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, BoxLayoutView)

#endif  // UI_VIEWS_LAYOUT_BOX_LAYOUT_VIEW_H_
