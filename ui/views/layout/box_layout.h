// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_BOX_LAYOUT_H_
#define UI_VIEWS_LAYOUT_BOX_LAYOUT_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/normalized_geometry.h"
#include "ui/views/view.h"

namespace views {

class VIEWS_EXPORT BoxLayoutFlexSpecification {
 public:
  BoxLayoutFlexSpecification();
  ~BoxLayoutFlexSpecification();

  // Makes a copy of this specification with a different weight.
  BoxLayoutFlexSpecification WithWeight(int weight) const;

  // Whether to use minimum values to make copies of this specification.
  BoxLayoutFlexSpecification UseMinSize(bool use_min_size) const;

  int weight() const { return weight_; }
  bool use_min_size() const { return use_min_size_; }

 private:
  int weight_ = 1;
  bool use_min_size_ = false;
};

// A Layout manager that arranges child views vertically or horizontally in a
// side-by-side fashion with spacing around and between the child views. The
// child views are always sized according to their preferred size. If the
// host's bounds provide insufficient space, child views will be clamped.
// Excess space will not be distributed.
class VIEWS_EXPORT BoxLayout : public LayoutManagerBase {
 public:
  using Orientation = LayoutOrientation;

  // This specifies that the start/center/end of the collective child views is
  // aligned with the start/center/end of the host view. e.g. a horizontal
  // layout of MainAxisAlignment::kEnd will result in the child views being
  // right-aligned.
  using MainAxisAlignment = LayoutAlignment;

  // This specifies where along the cross axis the children should be laid out.
  // e.g. a horizontal layout of kEnd will result in the child views being
  // bottom-aligned.
  using CrossAxisAlignment = LayoutAlignment;

  // Use |inside_border_insets| to add additional space between the child
  // view area and the host view border. |between_child_spacing| controls the
  // space in between child views. Use view->SetProperty(kMarginsKey,
  // gfx::Insets(xxx)) to add additional margins on a per-view basis. The
  // |collapse_margins_spacing| parameter controls whether or not adjacent
  // spacing/margins are collapsed based on the max of the two values. For the
  // cross axis, |collapse_margins_spacing| will collapse to the max of
  // inside_border_xxxxx_spacing and the corresponding margin edge from each
  // view.
  //
  // Given the following views where V = view bounds, M = Margins property,
  // B = between child spacing, S = inside border spacing and
  // <space> = added margins for alignment
  //
  // MMMMM  MMVVVVMM  MMMM
  // VVVVM            MMMM
  // VVVVM            MMMM
  // VVVVM            VVVV
  // MMMMM
  //
  // With collapse_margins_spacing = false, orientation = kHorizontal,
  // inside_border_spacing_horizontal = 2, inside_border_spacing_vertical = 2
  // and between_child_spacing = 1:
  //
  // -----------------------
  // SSSSSSSSSSSSSSSSSSSSSSS
  // SSSSSSSSSSSSSSSSSSSSSSS
  // SS    MBMM    MMBMMMMSS
  // SS    MBMM    MMBMMMMSS
  // SSMMMMMBMM    MMBMMMMSS
  // SSVVVVMBMMVVVVMMBVVVVSS
  // SSVVVVMBMMVVVVMMBVVVVSS
  // SSVVVVMBMMVVVVMMBVVVVSS
  // SSMMMMMBMMVVVVMMBVVVVSS
  // SSSSSSSSSSSSSSSSSSSSSSS
  // SSSSSSSSSSSSSSSSSSSSSSS
  // -----------------------
  //
  // Same as above except, collapse_margins_spacing = true.
  //
  // --------------------
  // SS          MMMMMMSS
  // SS          MMMMMMSS
  // SSMMMMMM    MMMMMMSS
  // SSVVVVMMVVVVMMVVVVSS
  // SSVVVVMMVVVVMMVVVVSS
  // SSVVVVMMVVVVMMVVVVSS
  // SSSSSSSSSSSSSSSSSSSS
  // SSSSSSSSSSSSSSSSSSSS
  // --------------------
  //
  explicit BoxLayout(Orientation orientation = Orientation::kHorizontal,
                     const gfx::Insets& inside_border_insets = gfx::Insets(),
                     int between_child_spacing = 0,
                     bool collapse_margins_spacing = false);
  ~BoxLayout() override;

  void SetOrientation(Orientation orientation);
  Orientation GetOrientation() const;

  // TODO(tluk): These class member setters should likely be calling
  // LayoutManager::InvalidateLayout() .
  void set_main_axis_alignment(MainAxisAlignment main_axis_alignment);
  MainAxisAlignment main_axis_alignment() const { return main_axis_alignment_; }

  void set_cross_axis_alignment(CrossAxisAlignment cross_axis_alignment);
  CrossAxisAlignment cross_axis_alignment() const {
    return cross_axis_alignment_;
  }

  void set_inside_border_insets(const gfx::Insets& insets);
  const gfx::Insets& inside_border_insets() const {
    return inside_border_insets_;
  }

  void set_minimum_cross_axis_size(int size) {
    minimum_cross_axis_size_ = size;
  }
  int minimum_cross_axis_size() const { return minimum_cross_axis_size_; }

  void set_between_child_spacing(int spacing) {
    between_child_spacing_ = spacing;
  }
  int between_child_spacing() const { return between_child_spacing_; }

  void SetCollapseMarginsSpacing(bool collapse_margins_spacing);
  bool GetCollapseMarginsSpacing() const;

  // Sets the flex weight for the given |view|. Using the preferred size as
  // the basis, free space along the main axis is distributed to views in the
  // ratio of their flex weights. Similarly, if the views will overflow the
  // parent, space is subtracted in these ratios.
  // If true is passed in for |use_min_size|, the given view's minimum size
  // is then obtained from calling View::GetMinimumSize(). This will be the
  // minimum allowed size for the view along the main axis. False
  // for |use_min_size| (the default) will allow the |view| to be resized to a
  // minimum size of 0.
  //
  // A flex of 0 means this view is not resized. Flex values must not be
  // negative.
  void SetFlexForView(const View* view, int flex, bool use_min_size = false);

  // Clears the flex for the given |view|, causing it to use the default
  // flex.
  void ClearFlexForView(const View* view);

  // Sets the flex for views to use when none is specified.
  void SetDefaultFlex(int default_flex);
  int GetDefaultFlex() const;

 protected:
  // Overridden from views::LayoutManager:
  ProposedLayout CalculateProposedLayout(
      const SizeBounds& size_bounds) const override;

 private:
  struct BoxLayoutData;

  // Returns the flex for the specified |view|.
  int GetFlexForView(const View* view) const;

  // Returns the minimum size for the specified |view|.
  int GetMinimumSizeForView(const View* view) const;

  // Update `BoxChildData::margins` to account for the option to collapse
  // margin spacing.
  void UpdateChildMarginsIfCollapseMarginsSpacing(BoxLayoutData& data) const;

  // Returns the preferred size of the current view under `size_bounds`.
  gfx::Size GetPreferredSizeForView(
      const View* view,
      const NormalizedSizeBounds& size_bounds) const;

  // Ensure that the vertical axis size of the view is no less than
  // minimum_cross_axis_size_.
  void EnsureCrossSize(BoxLayoutData& data) const;

  // Data required to initialize the layout, including filtering views that do
  // not participate in the layout and calculating the maximum leading and
  // trailing margin.
  void InitializeChildData(BoxLayoutData& data) const;

  // Calculate the maximum child width, only used in vertical layout when no
  // main view bounds are provided.
  SizeBound CalculateMaxChildWidth(BoxLayoutData& data) const;

  // Calculate the preferred size of each subview by assuming that it takes the
  // entire available space.
  void CalculatePreferredSize(const SizeBounds& size_bounds,
                              BoxLayoutData& data) const;

  // Calculate the total size of host_view. If no main view bounds are provided,
  // this will be the total size of the content
  void CalculatePreferredTotalSize(BoxLayoutData& data) const;

  // Update and calculate the actual positions of all subviews based on flex
  // rules.
  void UpdateFlexLayout(const NormalizedSizeBounds& bounds,
                        BoxLayoutData& data) const;

  // Get actual main size and update preferred size if needed.
  // The actual main size is the original preferred size plus
  // `current_padding`. Recalculate the preferred size if the
  // size is shrunk.
  int GetActualMainSizeAndUpdateChildPreferredSizeIfNeeded(
      const NormalizedSizeBounds& bounds,
      BoxLayoutData& data,
      size_t index,
      int current_padding,
      SizeBound cross_axis_size) const;

  // Apply alignment rules to the subview, this will crop the subview when it
  // exceeds the bounds.
  void CalculateChildBounds(const SizeBounds& size_bounds,
                            BoxLayoutData& data) const;

  Orientation orientation_;

  // Spacing between child views and host view border.
  gfx::Insets inside_border_insets_;

  // Spacing to put in between child views.
  int between_child_spacing_;

  // The alignment of children in the main axis. This is
  // MainAxisAlignment::kStart by default.
  MainAxisAlignment main_axis_alignment_ = MainAxisAlignment::kStart;

  // The alignment of children in the cross axis. This is
  // kStretch by default.
  CrossAxisAlignment cross_axis_alignment_ = CrossAxisAlignment::kStretch;

  // The flex weight for views if none is set. Defaults to 0.
  int default_flex_ = 0;

  // The minimum cross axis size for the layout.
  int minimum_cross_axis_size_ = 0;

  // Adjacent view margins and spacing should be collapsed.
  bool collapse_margins_spacing_;
};

}  // namespace views

#endif  // UI_VIEWS_LAYOUT_BOX_LAYOUT_H_
