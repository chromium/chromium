// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_BOX_LAYOUT_H_
#define UI_VIEWS_LAYOUT_BOX_LAYOUT_H_

#include <map>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/view.h"

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace views {

// A Layout manager that arranges child views vertically or horizontally in a
// side-by-side fashion with spacing around and between the child views. The
// child views are always sized according to their preferred size. If the
// host's bounds provide insufficient space, child views will be clamped.
// Excess space will not be distributed.
class VIEWS_EXPORT BoxLayout : public LayoutManager {
 public:
  enum class Orientation {
    kHorizontal,
    kVertical,
  };

  // This specifies that the start/center/end of the collective child views is
  // aligned with the start/center/end of the host view. e.g. a horizontal
  // layout of MainAxisAlignment::kEnd will result in the child views being
  // right-aligned.
  enum class MainAxisAlignment {
    kStart,
    kCenter,
    kEnd,
    // TODO(calamity): Add MAIN_AXIS_ALIGNMENT_JUSTIFY which spreads blank space
    // in-between the child views.
  };

  // This specifies where along the cross axis the children should be laid out.
  // e.g. a horizontal layout of kEnd will result in the child views being
  // bottom-aligned.
  enum class CrossAxisAlignment {
    // This causes the child view to stretch to fit the host in the cross axis.
    kStretch,
    kStart,
    kCenter,
    kEnd,
  };

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
  explicit BoxLayout(Orientation orientation,
                     const gfx::Insets& inside_border_insets = gfx::Insets(),
                     int between_child_spacing = 0,
                     bool collapse_margins_spacing = false);
  ~BoxLayout() override;

  void set_main_axis_alignment(MainAxisAlignment main_axis_alignment) {
    main_axis_alignment_ = main_axis_alignment;
  }

  void set_cross_axis_alignment(CrossAxisAlignment cross_axis_alignment) {
    cross_axis_alignment_ = cross_axis_alignment;
  }

  void set_inside_border_insets(const gfx::Insets& insets) {
    inside_border_insets_ = insets;
  }

  void set_minimum_cross_axis_size(int size) {
    minimum_cross_axis_size_ = size;
  }

  void set_between_child_spacing(int spacing) {
    between_child_spacing_ = spacing;
  }

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

  // Overridden from views::LayoutManager:
  void Installed(View* host) override;
  void ViewRemoved(View* host, View* view) override;
  void Layout(View* host) override;
  gfx::Size GetPreferredSize(const View* host) const override;
  int GetPreferredHeightForWidth(const View* host, int width) const override;

 private:
  // This struct is used internally to "wrap" a child view in order to obviate
  // the need for the main layout logic to be fully aware of the per-view
  // margins when |collapse_margin_spacing_| is false. Since each view is a
  // rectangle of a certain size, this wrapper, coupled with any margins set
  // will increase the apparent size of the view along the main axis. All
  // necessary view size/position methods required for the layout logic add or
  // subtract the margins where appropriate to ensure the actual visible size of
  // the view doesn't include the margins. For the cross axis, the margins are
  // NOT included in the size/position calculations. BoxLayout will adjust the
  // bounding rectangle of the space used for layout using the maximum margin
  // for all views along the appropriate edge.
  // When |collapse_margin_spacing_| is true, this wrapper provides quick access
  // to the view's margins for use by the layout to collapse adjacent spacing
  // to the largest of the several values.
  class ViewWrapper {
   public:
    ViewWrapper();
    ViewWrapper(const BoxLayout* layout, View* view);
    ~ViewWrapper();

    int GetHeightForWidth(int width) const;
    const gfx::Insets& margins() const { return margins_; }
    gfx::Size GetPreferredSize() const;
    void SetBoundsRect(const gfx::Rect& bounds);
    View* view() const { return view_; }
    bool visible() const;

   private:
    View* view_ = nullptr;
    const BoxLayout* layout_ = nullptr;
    gfx::Insets margins_;

    DISALLOW_COPY_AND_ASSIGN(ViewWrapper);
  };

  struct Flex {
    int flex_weight;
    bool use_min_size;
  };

  using FlexMap = std::map<const View*, Flex>;

  // Returns the flex for the specified |view|.
  int GetFlexForView(const View* view) const;

  // Returns the minimum size for the specified |view|.
  int GetMinimumSizeForView(const View* view) const;

  // Returns the size and position along the main axis of |rect|.
  int MainAxisSize(const gfx::Rect& rect) const;
  int MainAxisPosition(const gfx::Rect& rect) const;

  // Sets the size and position along the main axis of |rect|.
  void SetMainAxisSize(int size, gfx::Rect* rect) const;
  void SetMainAxisPosition(int position, gfx::Rect* rect) const;

  // Returns the size and position along the cross axis of |rect|.
  int CrossAxisSize(const gfx::Rect& rect) const;
  int CrossAxisPosition(const gfx::Rect& rect) const;

  // Sets the size and position along the cross axis of |rect|.
  void SetCrossAxisSize(int size, gfx::Rect* rect) const;
  void SetCrossAxisPosition(int size, gfx::Rect* rect) const;

  // Returns the main axis size for the given view. |child_area_width| is needed
  // to calculate the height of the view when the orientation is vertical.
  int MainAxisSizeForView(const ViewWrapper& view, int child_area_width) const;

  // Returns the |left| or |top| edge of the given inset based on the value of
  // |orientation_|.
  int MainAxisLeadingInset(const gfx::Insets& insets) const;

  // Returns the |right| or |bottom| edge of the given inset based on the value
  // of |orientation_|.
  int MainAxisTrailingInset(const gfx::Insets& insets) const;

  // Returns the left (|x|) or top (|y|) edge of the given rect based on the
  // value of |orientation_|.
  int CrossAxisLeadingEdge(const gfx::Rect& rect) const;

  // Returns the |left| or |top| edge of the given inset based on the value of
  // |orientation_|.
  int CrossAxisLeadingInset(const gfx::Insets& insets) const;

  // Returns the |right| or |bottom| edge of the given inset based on the value
  // of |orientation_|.
  int CrossAxisTrailingInset(const gfx::Insets& insets) const;

  // Returns the main axis margin spacing between the two views which is the max
  // of the right margin from the |left| view, the left margin of the |right|
  // view and |between_child_spacing_|.
  int MainAxisMarginBetweenViews(const ViewWrapper& left,
                                 const ViewWrapper& right) const;

  // Returns the outer margin along the main axis as insets.
  gfx::Insets MainAxisOuterMargin() const;

  // Returns the maximum margin along the cross axis from all views as insets.
  gfx::Insets CrossAxisMaxViewMargin() const;

  // Adjusts the main axis for |rect| by collapsing the left or top margin of
  // the first view with corresponding side of |inside_border_insets_| and the
  // right or bottom margin of the last view with the corresponding side of
  // |inside_border_insets_|.
  void AdjustMainAxisForMargin(gfx::Rect* rect) const;

  // Adjust the cross axis for |rect| using the appropriate sides of
  // |inside_border_insets_|.
  void AdjustCrossAxisForInsets(gfx::Rect* rect) const;

  // Returns the cross axis size for the given view.
  int CrossAxisSizeForView(const ViewWrapper& view) const;

  // Returns the total margin width for the given view or 0 when
  // collapse_margins_spacing_ is true.
  int CrossAxisMarginSizeForView(const ViewWrapper& view) const;

  // Returns the Top or Left size of the margin for the given view or 0 when
  // collapse_margins_spacing_ is true.
  int CrossAxisLeadingMarginForView(const ViewWrapper& view) const;

  // Adjust the cross axis for |rect| using the given leading and trailing
  // values.
  void InsetCrossAxis(gfx::Rect* rect, int leading, int trailing) const;

  // The preferred size for the dialog given the width of the child area.
  gfx::Size GetPreferredSizeForChildWidth(const View* host,
                                          int child_area_width) const;

  // The amount of space the layout requires in addition to any space for the
  // child views.
  gfx::Size NonChildSize(const View* host) const;

  // The next visible view at or after pos. If no other views are visible,
  // returns null.
  View* NextVisibleView(View::Views::const_iterator pos) const;

  // Return the first visible view in the host or nullptr if none are visible.
  View* FirstVisibleView() const;

  // Return the last visible view in the host or nullptr if none are visible.
  View* LastVisibleView() const;

  const Orientation orientation_;

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

  // A map of views to their flex weights.
  FlexMap flex_map_;

  // The flex weight for views if none is set. Defaults to 0.
  int default_flex_ = 0;

  // The minimum cross axis size for the layout.
  int minimum_cross_axis_size_ = 0;

  // Adjacent view margins and spacing should be collapsed.
  const bool collapse_margins_spacing_;

  // The view that this BoxLayout is managing the layout for.
  views::View* host_ = nullptr;

  DISALLOW_IMPLICIT_CONSTRUCTORS(BoxLayout);
};

}  // namespace views

#endif  // UI_VIEWS_LAYOUT_BOX_LAYOUT_H_
