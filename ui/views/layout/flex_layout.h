// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_FLEX_LAYOUT_H_
#define UI_VIEWS_LAYOUT_FLEX_LAYOUT_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/optional.h"
#include "ui/base/class_property.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/views_export.h"

namespace views {

class NormalizedSizeBounds;
class View;

// Provides CSS-like layout for a one-dimensional (vertical or horizontal)
// arrangement of child views. Independent alignment can be specified for the
// main and cross axes.
//
// Per-View margins (provided by view property kMarginsKey) specify how much
// space to leave around each child view. The |interior_margin| says how much
// empty space to leave at the edges of the parent view. If |collapse_margins|
// is false, these values are additive; if true, the greater of the two values
// is used. The |default_child_margins| provides a fallback for views without
// kMarginsKey set.
//
// collapse_margins = false:
//
// | interior margin>                      <margin [view]...
// |                 <margin [view] margin>
//
// collapse_margins = true:
//
// | interior margin>      <margin [view]
// |         <margin [view] margin>       ...
//
// Views can have their own internal padding, using the kInternalPaddingKey
// property, which is subtracted from the margin space between child views.
//
// Calling SetVisible(false) on a child view outside of the FlexLayout will
// result in the child view being hidden until SetVisible(true) is called. This
// is irrespective of whether the FlexLayout has set the child view to be
// visible or not based on, for example, flex rules.
//
// If you want the host view to maintain control over a child view, you can
// exclude it from the layout. Excluded views are completely ignored during
// layout and do not have their properties modified.
//
// FlexSpecification objects determine how child views are sized. You can set
// individual flex rules for each child view, or a default for any child views
// without individual flex rules set. If you don't set anything, each view will
// take up its preferred size in the layout.
//
// The core function of this class is contained in
// GetPreferredSize(maximum_size) and Layout(). In both cases, a layout will
// be cached and typically not recalculated as long as none of the layout's
// properties or the preferred size or visibility of any of its children has
// changed.
class VIEWS_EXPORT FlexLayout : public LayoutManagerBase {
 public:
  FlexLayout();
  ~FlexLayout() override;

  // Note: setters provide a Builder-style interface, so you can type:
  // layout.SetMainAxisAlignment()
  //       .SetCrossAxisAlignment()
  //       .SetDefaultFlex(...);
  FlexLayout& SetOrientation(LayoutOrientation orientation);
  FlexLayout& SetMainAxisAlignment(LayoutAlignment main_axis_alignment);
  FlexLayout& SetCrossAxisAlignment(LayoutAlignment cross_axis_alignment);
  FlexLayout& SetInteriorMargin(const gfx::Insets& interior_margin);
  FlexLayout& SetMinimumCrossAxisSize(int size);
  FlexLayout& SetCollapseMargins(bool collapse_margins);
  FlexLayout& SetIncludeHostInsetsInLayout(bool include_host_insets_in_layout);
  FlexLayout& SetIgnoreDefaultMainAxisMargins(
      bool ignore_default_main_axis_margins);
  FlexLayout& SetBetweenChildSpacing(int between_child_spacing);

  LayoutOrientation orientation() const { return orientation_; }
  bool collapse_margins() const { return collapse_margins_; }
  LayoutAlignment main_axis_alignment() const { return main_axis_alignment_; }
  LayoutAlignment cross_axis_alignment() const { return cross_axis_alignment_; }
  const gfx::Insets& interior_margin() const { return interior_margin_; }
  int minimum_cross_axis_size() const { return minimum_cross_axis_size_; }
  bool include_host_insets_in_layout() const {
    return include_host_insets_in_layout_;
  }
  bool ignore_default_main_axis_margins() const {
    return ignore_default_main_axis_margins_;
  }
  int between_child_spacing() const { return between_child_spacing_; }

  // Moves and uses |value| as the default value for layout property |key|.
  template <class T, class U>
  FlexLayout& SetDefault(const ui::ClassProperty<T>* key, U&& value) {
    layout_defaults_.SetProperty(key, std::forward<U>(value));
    return *this;
  }

  // Copies and uses |value| as the default value for layout property |key|.
  template <class T, class U>
  FlexLayout& SetDefault(const ui::ClassProperty<T>* key, const U& value) {
    layout_defaults_.SetProperty(key, value);
    return *this;
  }

 protected:
  // LayoutManagerBase:
  ProposedLayout CalculateProposedLayout(
      const SizeBounds& size_bounds) const override;

 private:
  struct ChildLayoutParams;
  class ChildViewSpacing;
  struct FlexLayoutData;

  class PropertyHandler : public ui::PropertyHandler {
   public:
    explicit PropertyHandler(FlexLayout* layout);

   protected:
    // ui::PropertyHandler:
    void AfterPropertyChange(const void* key, int64_t old_value) override;

   private:
    FlexLayout* const layout_;
  };

  // Maps a flex order (lower = allocated first, and therefore higher priority)
  // to the indices of child views within that order that can flex.
  // See FlexSpecification::order().
  using FlexOrderToViewIndexMap = std::map<int, std::vector<size_t>>;

  // Returns the combined margins across the cross axis of the host view, for a
  // particular child view.
  Inset1D GetCrossAxisMargins(const FlexLayoutData& layout,
                              size_t child_index) const;

  // Calculates a margin between two child views based on each's margin,
  // inter-child spacing, and any internal padding present in one or both
  // elements. Uses properties of the layout, like whether adjacent margins
  // should be collapsed.
  int CalculateMargin(int margin1,
                      int margin2,
                      int internal_padding,
                      int spacing = 0) const;

  // Calculates the cross-layout space available to a view based on the
  // available space and margins.
  base::Optional<int> GetAvailableCrossAxisSize(
      const FlexLayoutData& layout,
      size_t child_index,
      const NormalizedSizeBounds& bounds) const;

  // Calculates the preferred spacing between two child views, or between a
  // view edge and the first or last visible child views.
  int CalculateChildSpacing(const FlexLayoutData& layout,
                            base::Optional<size_t> child1_index,
                            base::Optional<size_t> child2_index) const;

  // Calculates the position of each child view and the size of the overall
  // layout based on tentative visibilities and sizes for each child.
  void UpdateLayoutFromChildren(const NormalizedSizeBounds& bounds,
                                FlexLayoutData* data,
                                ChildViewSpacing* child_spacing) const;

  // Applies flex rules to each view in a layout, updating |data| and
  // |child_spacing|.
  //
  // If |expandable_views| is specified, any view requesting more than its
  // preferred size will be clamped to its preferred size and be added to
  // |expandable_views| for later processing after all other flex space has been
  // allocated.
  //
  // Typically, this method will be called once with |expandable_views| set and
  // then again with it null to allocate the remaining space.
  void AllocateFlexSpace(
      const NormalizedSizeBounds& bounds,
      const FlexOrderToViewIndexMap& order_to_index,
      FlexLayoutData* data,
      ChildViewSpacing* child_spacing,
      FlexOrderToViewIndexMap* expandable_views = nullptr) const;

  // Fills out the child entries for |data| and generates some initial size
  // and visibility data, and stores off information about which views can
  // expand in |flex_order_to_index|.
  void InitializeChildData(const NormalizedSizeBounds& bounds,
                           FlexLayoutData* data,
                           FlexOrderToViewIndexMap* flex_order_to_index) const;

  // Caclulates the child bounds (in screen coordinates) for each visible child
  // in the layout.
  void CalculateChildBounds(const SizeBounds& size_bounds,
                            FlexLayoutData* data) const;

  // Gets the default value for a particular layout property, which will be used
  // if the property is not set on a child view being laid out (e.g.
  // kMarginsKey).
  template <class T>
  T* GetDefault(const ui::ClassProperty<T>* key) const {
    return layout_defaults_.GetProperty(key);
  }

  // Clears the default value for a particular layout property, which will be
  // used if the property is not set on a child view being laid out (e.g.
  // kMarginsKey).
  template <class T>
  FlexLayout& ClearDefault(const ui::ClassProperty<T>* key) {
    layout_defaults_.ClearProperty(key);
    return *this;
  }

  LayoutOrientation orientation_ = LayoutOrientation::kHorizontal;

  // Adjacent view margins should be collapsed.
  bool collapse_margins_ = false;

  // Spacing between child views and host view border.
  gfx::Insets interior_margin_;

  // The alignment of children in the main axis. This is start by default.
  LayoutAlignment main_axis_alignment_ = LayoutAlignment::kStart;

  // The alignment of children in the cross axis. This is stretch by default.
  LayoutAlignment cross_axis_alignment_ = LayoutAlignment::kStretch;

  // The minimum cross axis size for the layout.
  int minimum_cross_axis_size_ = 0;

  // Whether to include host insets in the layout. Use when e.g. the host has an
  // empty border and you want to treat that empty space as part of the interior
  // margin of the host view.
  //
  // Most useful in conjunction with |collapse_margins| so child margins can
  // overlap with the host's insets.
  //
  // In the future, we might consider putting this as metadata on the host's
  // border - e.g. an EmptyBorder would be included in host insets but a thick
  // frame would not be.
  bool include_host_insets_in_layout_ = false;

  // Whether host |interior_margin| overrides default child margins at the
  // leading and trailing edge of the host view.
  //
  // Example:
  // layout->SetIgnoreDefaultMainAxisMargins(true)
  //        .SetCollapseMargins(true)
  //        .SetDefault(kMarginsKey, {5, 10})
  //        .SetInteriorMargin({5, 5});
  //
  // This produces a margin of 5 DIP on all edges of the host view, with 10 DIP
  // between child views. If SetIgnoreDefaultMainAxisMargins(true) was not
  // called, the default child margin of 10 would also apply on the leading and
  // trailing edge of the host view.
  bool ignore_default_main_axis_margins_ = false;

  // The spacing between the children along the main axis. This is irrespective
  // of any margins which are set. If |collapse_margins_| is true, then the max
  // between this value and the margins is used.
  int between_child_spacing_ = 0;

  // Default properties for any views that don't have them explicitly set for
  // this layout.
  PropertyHandler layout_defaults_{this};

  DISALLOW_COPY_AND_ASSIGN(FlexLayout);
};

}  // namespace views

#endif  // UI_VIEWS_LAYOUT_FLEX_LAYOUT_H_
