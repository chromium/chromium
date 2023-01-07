// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/flex_layout.h"

#include <algorithm>
#include <functional>
#include <numeric>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "ui/base/class_property.h"
#include "ui/events/event_target.h"
#include "ui/events/event_target_iterator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/normalized_geometry.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

// Module-private declarations -------------------------------------------------

namespace views {

namespace {

// Layout information for a specific child view in a proposed layout.
struct FlexChildData {
  explicit FlexChildData(const FlexSpecification& flex) : flex(flex) {}

  // Copying this struct would be expensive and they only ever live in a vector
  // in Layout (see below) so we'll only allow move semantics.
  FlexChildData(const FlexChildData&) = delete;
  FlexChildData& operator=(const FlexChildData&) = delete;

  FlexChildData(FlexChildData&& other) = default;

  std::string ToString() const {
    std::ostringstream oss;
    oss << "{ preferred " << preferred_size.ToString() << " current "
        << current_size.ToString() << " margins " << margins.ToString()
        << (using_default_margins ? " (using default)" : "") << " padding "
        << internal_padding.ToString() << " bounds " << actual_bounds.ToString()
        << " }";
    return oss.str();
  }

  NormalizedSize preferred_size;
  NormalizedSize current_size;
  NormalizedInsets margins;
  bool using_default_margins = true;
  NormalizedInsets internal_padding;
  NormalizedRect actual_bounds;
  FlexSpecification flex;
};

template <typename T>
T GetViewProperty(const View* view,
                  const ui::PropertyHandler& defaults,
                  const ui::ClassProperty<T*>* property,
                  bool* is_default = nullptr) {
  T* found_value = view->GetProperty(property);
  if (found_value) {
    if (is_default)
      *is_default = false;
    return *found_value;
  }
  if (is_default)
    *is_default = true;
  found_value = defaults.GetProperty(property);
  if (found_value)
    return *found_value;
  return T();
}

template <typename T>
T MaybeReverse(const T& list, FlexAllocationOrder order) {
  return order == FlexAllocationOrder::kReverse ? T(list.rbegin(), list.rend())
                                                : list;
}

}  // anonymous namespace

// Private implementation ------------------------------------------------------

// These definitions are required due to the C++ spec.
constexpr LayoutAlignment FlexLayout::kDefaultMainAxisAlignment;
constexpr LayoutAlignment FlexLayout::kDefaultCrossAxisAlignment;

// Calculates and maintains 1D spacing between a sequence of child views.
class FlexLayout::ChildViewSpacing {
 public:
  // Given the indices of two child views, returns the amount of space that
  // should be placed between them if they were adjacent. If the first index is
  // absent, uses the left edge of the parent container. If the second index is
  // absent, uses the right edge of the parent container.
  using GetViewSpacingCallback =
      base::RepeatingCallback<int(absl::optional<size_t>,
                                  absl::optional<size_t>)>;

  explicit ChildViewSpacing(GetViewSpacingCallback get_view_spacing);
  ChildViewSpacing(const ChildViewSpacing& other) = default;
  ChildViewSpacing& operator=(const ChildViewSpacing& other) = default;

  bool HasViewIndex(size_t view_index) const;
  int GetLeadingInset() const;
  int GetTrailingInset() const;
  int GetLeadingSpace(size_t view_index) const;
  int GetTotalSpace() const;

  // Returns the maximum size for the child at |view_index|, given its
  // |current_size| and the amount of |available_space| for flex allocation.
  SizeBound GetMaxSize(size_t view_index,
                       int current_size,
                       const SizeBound& available_space) const;

  // Returns the change in total allocated size if the child at |view_index| is
  // resized from |current_size| to |new_size|.
  int GetTotalSizeChangeForNewSize(size_t view_index,
                                   int current_size,
                                   int new_size) const;

  // Add the view at the specified index.
  //
  // If |new_leading| or |new_trailing| is specified, it will be set to the new
  // leading/trailing space for the view at the index that was added.
  void AddViewIndex(size_t view_index,
                    int* new_leading = nullptr,
                    int* new_trailing = nullptr);

 private:
  absl::optional<size_t> GetPreviousViewIndex(size_t view_index) const;
  absl::optional<size_t> GetNextViewIndex(size_t view_index) const;

  // Returns the change in space required if the specified view index were
  // added. The view must not already be present.
  int GetAddDelta(size_t view_index) const;

  GetViewSpacingCallback get_view_spacing_;
  // Maps from view index to the leading spacing for that index.
  std::map<size_t, int> leading_spacings_;
  // The trailing space (space preceding the trailing margin).
  int trailing_space_;
};

FlexLayout::ChildViewSpacing::ChildViewSpacing(
    GetViewSpacingCallback get_view_spacing)
    : get_view_spacing_(std::move(get_view_spacing)),
      trailing_space_(get_view_spacing_.Run(absl::nullopt, absl::nullopt)) {}

bool FlexLayout::ChildViewSpacing::HasViewIndex(size_t view_index) const {
  return leading_spacings_.find(view_index) != leading_spacings_.end();
}

int FlexLayout::ChildViewSpacing::GetLeadingInset() const {
  if (leading_spacings_.empty())
    return 0;
  return leading_spacings_.begin()->second;
}

int FlexLayout::ChildViewSpacing::GetTrailingInset() const {
  return trailing_space_;
}

int FlexLayout::ChildViewSpacing::GetLeadingSpace(size_t view_index) const {
  auto it = leading_spacings_.find(view_index);
  DCHECK(it != leading_spacings_.end());
  return it->second;
}

int FlexLayout::ChildViewSpacing::GetTotalSpace() const {
  return std::accumulate(
      leading_spacings_.cbegin(), leading_spacings_.cend(), trailing_space_,
      [](int total, const auto& value) { return total + value.second; });
}

SizeBound FlexLayout::ChildViewSpacing::GetMaxSize(
    size_t view_index,
    int current_size,
    const SizeBound& available_space) const {
  DCHECK_GE(available_space, 0);

  if (HasViewIndex(view_index))
    return current_size + available_space;

  DCHECK_EQ(0, current_size);
  // Making the child visible may result in the addition of margin space, which
  // counts against the child view's flex space allocation.
  //
  // Note: In cases where the layout's internal margins and/or the child views'
  // margins are wildly different sizes, subtracting the full delta out of the
  // available space can cause the first view to be smaller than we would expect
  // (see TODOs in unit tests for examples). We should look into ways to make
  // this "feel" better (but in the meantime, specify reasonable margins).
  return std::max<SizeBound>(available_space - GetAddDelta(view_index), 0);
}

int FlexLayout::ChildViewSpacing::GetTotalSizeChangeForNewSize(
    size_t view_index,
    int current_size,
    int new_size) const {
  return HasViewIndex(view_index) ? new_size - current_size
                                  : new_size + GetAddDelta(view_index);
}

void FlexLayout::ChildViewSpacing::AddViewIndex(size_t view_index,
                                                int* new_leading,
                                                int* new_trailing) {
  DCHECK(!HasViewIndex(view_index));
  absl::optional<size_t> prev = GetPreviousViewIndex(view_index);
  absl::optional<size_t> next = GetNextViewIndex(view_index);

  const int leading_space = get_view_spacing_.Run(prev, view_index);
  const int trailing_space = get_view_spacing_.Run(view_index, next);
  leading_spacings_[view_index] = leading_space;
  if (next)
    leading_spacings_[*next] = trailing_space;
  else
    trailing_space_ = trailing_space;

  if (new_leading)
    *new_leading = leading_space;
  if (new_trailing)
    *new_trailing = trailing_space;
}

absl::optional<size_t> FlexLayout::ChildViewSpacing::GetPreviousViewIndex(
    size_t view_index) const {
  const auto it = leading_spacings_.lower_bound(view_index);
  if (it == leading_spacings_.begin())
    return absl::nullopt;
  return std::prev(it)->first;
}

absl::optional<size_t> FlexLayout::ChildViewSpacing::GetNextViewIndex(
    size_t view_index) const {
  const auto it = leading_spacings_.upper_bound(view_index);
  if (it == leading_spacings_.end())
    return absl::nullopt;
  return it->first;
}

int FlexLayout::ChildViewSpacing::GetAddDelta(size_t view_index) const {
  DCHECK(!HasViewIndex(view_index));
  absl::optional<size_t> prev = GetPreviousViewIndex(view_index);
  absl::optional<size_t> next = GetNextViewIndex(view_index);
  const int old_spacing = next ? GetLeadingSpace(*next) : GetTrailingInset();
  const int new_spacing = get_view_spacing_.Run(prev, view_index) +
                          get_view_spacing_.Run(view_index, next);
  return new_spacing - old_spacing;
}

// Represents a specific stored layout given a set of size bounds.
struct FlexLayout::FlexLayoutData {
  FlexLayoutData() = default;

  FlexLayoutData(const FlexLayoutData&) = delete;
  FlexLayoutData& operator=(const FlexLayoutData&) = delete;

  ~FlexLayoutData() = default;

  size_t num_children() const { return child_data.size(); }

  std::string ToString() const {
    std::ostringstream oss;
    oss << "{ " << total_size.ToString() << " " << layout.ToString() << " {";
    bool first = true;
    for (const FlexChildData& flex_child : child_data) {
      if (first)
        first = false;
      else
        oss << ", ";
      oss << flex_child.ToString();
    }
    oss << "} margin " << interior_margin.ToString() << " insets "
        << host_insets.ToString() << "}";
    return oss.str();
  }

  ProposedLayout layout;

  // Holds additional information about the child views of this layout.
  std::vector<FlexChildData> child_data;

  // The total size of the layout (minus parent insets).
  NormalizedSize total_size;
  NormalizedInsets interior_margin;
  NormalizedInsets host_insets;
};

FlexLayout::PropertyHandler::PropertyHandler(FlexLayout* layout)
    : layout_(layout) {}

void FlexLayout::PropertyHandler::AfterPropertyChange(const void* key,
                                                      int64_t old_value) {
  layout_->InvalidateHost(true);
}

// FlexLayout
// -------------------------------------------------------------------

FlexLayout::FlexLayout() {
  // Ensure this property is always set and is never null.
  SetDefault(kCrossAxisAlignmentKey, kDefaultCrossAxisAlignment);
}

FlexLayout::~FlexLayout() = default;

FlexLayout& FlexLayout::SetOrientation(LayoutOrientation orientation) {
  if (orientation != orientation_) {
    orientation_ = orientation;
    InvalidateHost(true);
  }
  return *this;
}

FlexLayout& FlexLayout::SetIncludeHostInsetsInLayout(
    bool include_host_insets_in_layout) {
  if (include_host_insets_in_layout != include_host_insets_in_layout_) {
    include_host_insets_in_layout_ = include_host_insets_in_layout;
    InvalidateHost(true);
  }
  return *this;
}

FlexLayout& FlexLayout::SetCollapseMargins(bool collapse_margins) {
  if (collapse_margins != collapse_margins_) {
    collapse_margins_ = collapse_margins;
    InvalidateHost(true);
  }
  return *this;
}

FlexLayout& FlexLayout::SetMainAxisAlignment(
    LayoutAlignment main_axis_alignment) {
  DCHECK_NE(main_axis_alignment, LayoutAlignment::kStretch)
      << "Main axis stretch/justify is not yet supported.";
  if (main_axis_alignment_ != main_axis_alignment) {
    main_axis_alignment_ = main_axis_alignment;
    InvalidateHost(true);
  }
  return *this;
}

FlexLayout& FlexLayout::SetCrossAxisAlignment(
    LayoutAlignment cross_axis_alignment) {
  return SetDefault(kCrossAxisAlignmentKey, cross_axis_alignment);
}

FlexLayout& FlexLayout::SetInteriorMargin(const gfx::Insets& interior_margin) {
  if (interior_margin_ != interior_margin) {
    interior_margin_ = interior_margin;
    InvalidateHost(true);
  }
  return *this;
}

FlexLayout& FlexLayout::SetIgnoreDefaultMainAxisMargins(
    bool ignore_default_main_axis_margins) {
  if (ignore_default_main_axis_margins_ != ignore_default_main_axis_margins) {
    ignore_default_main_axis_margins_ = ignore_default_main_axis_margins;
    InvalidateHost(true);
  }
  return *this;
}

FlexLayout& FlexLayout::SetMinimumCrossAxisSize(int size) {
  if (minimum_cross_axis_size_ != size) {
    minimum_cross_axis_size_ = size;
    InvalidateHost(true);
  }
  return *this;
}

FlexLayout& FlexLayout::SetFlexAllocationOrder(
    FlexAllocationOrder flex_allocation_order) {
  if (flex_allocation_order_ != flex_allocation_order) {
    flex_allocation_order_ = flex_allocation_order;
    InvalidateHost(true);
  }
  return *this;
}

FlexRule FlexLayout::GetDefaultFlexRule() const {
  return base::BindRepeating(&FlexLayout::DefaultFlexRuleImpl,
                             base::Unretained(this));
}

ProposedLayout FlexLayout::CalculateProposedLayout(
    const SizeBounds& size_bounds) const {
  FlexLayoutData data;

  if (include_host_insets_in_layout()) {
    // Combining the interior margin and host insets means we only have to set
    // the margin value; we'll leave the insets at zero.
    data.interior_margin =
        Normalize(orientation(), interior_margin() + host_view()->GetInsets());
  } else {
    data.host_insets = Normalize(orientation(), host_view()->GetInsets());
    data.interior_margin = Normalize(orientation(), interior_margin());
  }
  NormalizedSizeBounds bounds = Normalize(orientation(), size_bounds);
  bounds.Inset(data.host_insets);
  bounds.set_cross(
      std::max<SizeBound>(bounds.cross(), minimum_cross_axis_size()));

  // Populate the child layout data vectors and the order-to-index map.
  FlexOrderToViewIndexMap order_to_view_index;
  InitializeChildData(bounds, data, order_to_view_index);

  // Do the initial layout update, calculating spacing between children.
  ChildViewSpacing child_spacing(
      base::BindRepeating(&FlexLayout::CalculateChildSpacing,
                          base::Unretained(this), std::cref(data)));
  UpdateLayoutFromChildren(bounds, data, child_spacing);

  // We now have a layout with all views at the absolute minimum size and with
  // those able to drop out dropped out. Now apply flex rules.
  //
  // This is done in two primary phases:
  // 1. If there is insufficient space to provide each view with its preferred
  //    size, the deficit will be spread across the views that can flex, with
  //    any views that bottom out getting their minimum and dropping out of the
  //    calculation.
  // 2. If there is excess space after the first phase, it is spread across all
  //    of the remaining flex views that haven't dropped out.
  //
  // The result of this calculation is extremely *correct* but it is possible
  // there are some pathological cases where the cost of one of the steps is
  // quadratic in the number of views. Again, this is unlikely and numbers of
  // child views tend to be small enough that it won't matter.

  CalculateNonFlexAvailableSpace(
      std::max<SizeBound>(0, bounds.main() - data.total_size.main()),
      order_to_view_index, child_spacing, data);

  // Flex up to preferred size. This will be a no-op if |order_to_view_index|
  // is empty.
  FlexOrderToViewIndexMap expandable_views;
  AllocateFlexShortage(bounds, order_to_view_index, data, child_spacing,
                       expandable_views);

  // Flex views that can exceed their preferred size. This will be a no-op if
  // |expandable_views| is empty.
  AllocateFlexExcess(bounds, expandable_views, data, child_spacing);

  // Calculate the size of the host view.
  NormalizedSize host_size = data.total_size;
  host_size.Enlarge(data.host_insets.main_size(),
                    data.host_insets.cross_size());
  data.layout.host_size = Denormalize(orientation(), host_size);

  // Size and position the children in screen space.
  CalculateChildBounds(size_bounds, data);

  return data.layout;
}

NormalizedSize FlexLayout::GetPreferredSizeForRule(
    const FlexRule& rule,
    const View* child,
    const SizeBound& available_cross) const {
  const NormalizedSize default_size =
      Normalize(orientation(), rule.Run(child, SizeBounds()));
  if (!available_cross.is_bounded())
    return default_size;

  // Do the height-for-width calculation.
  const NormalizedSize stretch_size = Normalize(
      orientation(),
      rule.Run(child,
               Denormalize(orientation(), NormalizedSizeBounds(
                                              SizeBound(), available_cross))));

  NormalizedSize size = default_size;

  // For vertical layouts, allow changing the cross-axis to cause the main axis
  // to grow - or in the case of "stretch" alignment where we can potentially
  // force the cross-axis to be larger than the preferred size, allow the main
  // axis to shrink. This best handles labels and other text controls in
  // vertical layouts. (We don't do this in horizontal layouts for aesthetic
  // reasons.)
  if (orientation() == LayoutOrientation::kVertical) {
    const LayoutAlignment cross_align =
        GetViewProperty(child, layout_defaults_, kCrossAxisAlignmentKey);
    if (cross_align == LayoutAlignment::kStretch)
      return stretch_size;
    size.set_main(std::max(size.main(), stretch_size.main()));
  }

  // Always allow the cross axis to adjust to the available space if it's less
  // than the preferred size in order to prevent unnecessary overhang.
  size.set_cross(std::min(size.cross(), stretch_size.cross()));
  return size;
}

NormalizedSize FlexLayout::GetCurrentSizeForRule(
    const FlexRule& rule,
    const View* child,
    const NormalizedSizeBounds& available) const {
  return Normalize(orientation(),
                   rule.Run(child, Denormalize(orientation(), available)));
}

void FlexLayout::InitializeChildData(
    const NormalizedSizeBounds& bounds,
    FlexLayoutData& data,
    FlexOrderToViewIndexMap& flex_order_to_index) const {
  // Step through the children, creating placeholder layout view elements
  // and setting up initial minimal visibility.
  const bool main_axis_bounded = bounds.main().is_bounded();
  for (View* child : host_view()->children()) {
    if (!IsChildIncludedInLayout(child))
      continue;

    const size_t view_index = data.num_children();
    data.layout.child_layouts.emplace_back(ChildLayout{child});
    ChildLayout& child_layout = data.layout.child_layouts.back();
    data.child_data.emplace_back(
        GetViewProperty(child, layout_defaults_, views::kFlexBehaviorKey));
    FlexChildData& flex_child = data.child_data.back();

    flex_child.margins =
        Normalize(orientation(),
                  GetViewProperty(child, layout_defaults_, views::kMarginsKey,
                                  &flex_child.using_default_margins));
    flex_child.internal_padding = Normalize(
        orientation(),
        GetViewProperty(child, layout_defaults_, views::kInternalPaddingKey));

    const SizeBound available_cross =
        GetAvailableCrossAxisSize(data, view_index, bounds);
    SetCrossAxis(&child_layout.available_size, orientation(), available_cross);

    flex_child.preferred_size =
        GetPreferredSizeForRule(flex_child.flex.rule(), child, available_cross);

    // gfx::Size calculation depends on whether flex is allowed.
    if (main_axis_bounded) {
      flex_child.current_size =
          GetCurrentSizeForRule(flex_child.flex.rule(), child,
                                NormalizedSizeBounds(0, available_cross));

      DCHECK_GE(flex_child.preferred_size.main(),
                flex_child.current_size.main())
          << " in " << child->GetClassName();
    } else {
      // All non-flex or unbounded controls get preferred size.
      flex_child.current_size = flex_child.preferred_size;
    }

    // Keep track of non-hidden/ignored child views that can flex. We assume any
    // view with a non-zero weight can flex, as can views with zero weight that
    // have a minimum size smaller than their preferred size.
    const int weight = flex_child.flex.weight();
    bool can_flex = weight > 0 || flex_child.current_size.main() <
                                      flex_child.preferred_size.main();

    // Do a spot check to see if a zero-weight view could expand in the space
    // provided. Note that we can get some false positives here but they will
    // invariably shake out in subsequent steps.
    if (!can_flex && weight == 0) {
      const NormalizedSize estimate = GetCurrentSizeForRule(
          flex_child.flex.rule(), child,
          NormalizedSizeBounds(bounds.main(), available_cross));
      can_flex = estimate.main() > flex_child.preferred_size.main();
    }

    // Add views that have the potential to flex to the appropriate order list.
    if (can_flex)
      flex_order_to_index[flex_child.flex.order()].push_back(view_index);

    child_layout.visible = flex_child.current_size.main() > 0;
  }
}

void FlexLayout::CalculateChildBounds(const SizeBounds& size_bounds,
                                      FlexLayoutData& data) const {
  // Apply main axis alignment (we've already done cross-axis alignment above).
  const NormalizedSizeBounds normalized_bounds =
      Normalize(orientation(), size_bounds);
  const NormalizedSize normalized_host_size =
      Normalize(orientation(), data.layout.host_size);
  int available_main = normalized_bounds.main().is_bounded()
                           ? normalized_bounds.main().value()
                           : normalized_host_size.main();
  available_main = std::max(0, available_main - data.host_insets.main_size());
  const int excess_main = available_main - data.total_size.main();
  NormalizedPoint start(data.host_insets.main_leading(),
                        data.host_insets.cross_leading());
  switch (main_axis_alignment()) {
    case LayoutAlignment::kStart:
      break;
    case LayoutAlignment::kCenter:
      start.set_main(start.main() + excess_main / 2);
      break;
    case LayoutAlignment::kEnd:
      start.set_main(start.main() + excess_main);
      break;
    case LayoutAlignment::kStretch:
    case LayoutAlignment::kBaseline:
      NOTIMPLEMENTED();
      break;
  }

  // Calculate the actual child bounds.
  for (size_t i = 0; i < data.num_children(); ++i) {
    ChildLayout& child_layout = data.layout.child_layouts[i];
    if (child_layout.visible) {
      FlexChildData& flex_child = data.child_data[i];
      NormalizedRect actual = flex_child.actual_bounds;
      actual.Offset(start.main(), start.cross());
      if (actual.size_main() > flex_child.preferred_size.main() &&
          flex_child.flex.alignment() != LayoutAlignment::kStretch) {
        Span container(actual.origin_main(), actual.size_main());
        Span new_main(0, flex_child.preferred_size.main());
        new_main.Align(container, flex_child.flex.alignment());
        actual.set_origin_main(new_main.start());
        actual.set_size_main(new_main.length());
      }
      child_layout.bounds = Denormalize(orientation(), actual);
    }
  }
}

void FlexLayout::CalculateNonFlexAvailableSpace(
    const SizeBound& available_space,
    const FlexOrderToViewIndexMap& flex_views,
    const ChildViewSpacing& child_spacing,
    FlexLayoutData& data) const {
  // Add all views which are participating in flex (and will have their
  // available space set later) to a lookup so we can skip them now.
  std::set<size_t> all_flex_indices;
  for (const auto& order_to_indices : flex_views) {
    all_flex_indices.insert(order_to_indices.second.begin(),
                            order_to_indices.second.end());
  }

  // Work through the remaining views and set their available space. Since
  // non-flex views get their space first, these views will have access to the
  // entire budget of remaining space in the layout.
  for (size_t index = 0; index < data.child_data.size(); ++index) {
    if (base::Contains(all_flex_indices, index))
      continue;

    // Cross-axis available size is already set in InitializeChildData(), so
    // just set the main axis here.
    const SizeBound max_size = child_spacing.GetMaxSize(
        index, data.child_data[index].current_size.main(), available_space);
    SetMainAxis(&data.layout.child_layouts[index].available_size, orientation(),
                max_size);
  }
}

Inset1D FlexLayout::GetCrossAxisMargins(const FlexLayoutData& layout,
                                        size_t child_index) const {
  const FlexChildData& child_data = layout.child_data[child_index];
  const int leading_margin =
      CalculateMargin(layout.interior_margin.cross_leading(),
                      child_data.margins.cross_leading(),
                      child_data.internal_padding.cross_leading());
  const int trailing_margin =
      CalculateMargin(layout.interior_margin.cross_trailing(),
                      child_data.margins.cross_trailing(),
                      child_data.internal_padding.cross_trailing());
  return Inset1D(leading_margin, trailing_margin);
}

int FlexLayout::CalculateMargin(int margin1,
                                int margin2,
                                int internal_padding) const {
  const int result =
      collapse_margins() ? std::max(margin1, margin2) : margin1 + margin2;
  return std::max(0, result - internal_padding);
}

SizeBound FlexLayout::GetAvailableCrossAxisSize(
    const FlexLayoutData& layout,
    size_t child_index,
    const NormalizedSizeBounds& bounds) const {
  const Inset1D cross_margins = GetCrossAxisMargins(layout, child_index);
  return std::max<SizeBound>(0, bounds.cross() - cross_margins.size());
}

int FlexLayout::CalculateChildSpacing(
    const FlexLayoutData& layout,
    absl::optional<size_t> child1_index,
    absl::optional<size_t> child2_index) const {
  const FlexChildData* const child1 =
      child1_index ? &layout.child_data[*child1_index] : nullptr;
  const FlexChildData* const child2 =
      child2_index ? &layout.child_data[*child2_index] : nullptr;

  const int child1_trailing =
      child1 && (child2 || !ignore_default_main_axis_margins() ||
                 !child1->using_default_margins)
          ? child1->margins.main_trailing()
          : 0;
  const int child2_leading =
      child2 && (child1 || !ignore_default_main_axis_margins() ||
                 !child2->using_default_margins)
          ? child2->margins.main_leading()
          : 0;

  const int left_margin =
      child1 ? child1_trailing : layout.interior_margin.main_leading();
  const int right_margin =
      child2 ? child2_leading : layout.interior_margin.main_trailing();

  const int left_padding =
      child1 ? child1->internal_padding.main_trailing() : 0;
  const int right_padding =
      child2 ? child2->internal_padding.main_leading() : 0;

  return CalculateMargin(left_margin, right_margin,
                         left_padding + right_padding);
}

void FlexLayout::UpdateLayoutFromChildren(
    const NormalizedSizeBounds& bounds,
    FlexLayoutData& data,
    ChildViewSpacing& child_spacing) const {
  // Calculate starting minimum for cross-axis size.
  int min_cross_size =
      std::max(minimum_cross_axis_size(),
               CalculateMargin(data.interior_margin.cross_leading(),
                               data.interior_margin.cross_trailing(), 0));
  data.total_size = NormalizedSize(0, min_cross_size);

  // For cases with a non-zero cross-axis bound, the objective is to fit the
  // layout into that precise size, not to determine what size we need.
  bool force_cross_size = false;
  if (bounds.cross().is_bounded() && bounds.cross() > 0) {
    data.total_size.SetToMax(0, bounds.cross().value());
    force_cross_size = true;
  }

  std::vector<Inset1D> cross_spacings(data.num_children());
  for (size_t i = 0; i < data.num_children(); ++i) {
    FlexChildData& flex_child = data.child_data[i];

    const bool is_visible = data.layout.child_layouts[i].visible;

    // Update the cross-axis margins and if necessary, the size.
    cross_spacings[i] = GetCrossAxisMargins(data, i);
    if (!force_cross_size &&
        (is_visible || flex_child.preferred_size.main() == 0)) {
      data.total_size.SetToMax(
          0, cross_spacings[i].size() + flex_child.current_size.cross());
    }

    // We don't have to deal with invisible children any further than this.
    if (!is_visible)
      continue;

    // Calculate main-axis size and upper-left main axis coordinate.
    int leading_space;
    if (child_spacing.HasViewIndex(i))
      leading_space = child_spacing.GetLeadingSpace(i);
    else
      child_spacing.AddViewIndex(i, &leading_space);
    data.total_size.Enlarge(leading_space, 0);

    const int size_main = flex_child.current_size.main();
    flex_child.actual_bounds.set_origin_main(data.total_size.main());
    flex_child.actual_bounds.set_size_main(size_main);
    data.total_size.Enlarge(size_main, 0);
  }

  // Add the end margin.
  data.total_size.Enlarge(child_spacing.GetTrailingInset(), 0);

  // Calculate cross-axis positioning based on the cross margins and size that
  // were calculated above.
  const Span cross_span(0, data.total_size.cross());
  for (size_t i = 0; i < data.num_children(); ++i) {
    FlexChildData& flex_child = data.child_data[i];
    flex_child.actual_bounds.set_size_cross(flex_child.current_size.cross());
    const LayoutAlignment cross_align =
        GetViewProperty(data.layout.child_layouts[i].child_view,
                        layout_defaults_, kCrossAxisAlignmentKey);
    flex_child.actual_bounds.AlignCross(cross_span, cross_align,
                                        cross_spacings[i]);
  }
}

void FlexLayout::AllocateFlexShortage(
    const NormalizedSizeBounds& bounds,
    const FlexOrderToViewIndexMap& order_to_index,
    FlexLayoutData& data,
    ChildViewSpacing& child_spacing,
    FlexOrderToViewIndexMap& expandable_views) const {
  // Step through each flex priority allocating shortage across child views that
  // can flex.
  for (const auto& flex_elem : order_to_index) {
    const int order = flex_elem.first;

    // Record available space for each view at this flex order.
    CalculateFlexAvailableSpace(bounds, flex_elem.second, child_spacing, data);

    // Get the list of views to process at this flex priority, in the desired
    // order. Zero-preferred-size views are sorted directly onto the list of
    // expandable views, because they're already at their preferred size.
    ChildIndices view_indices;
    for (size_t child_index :
         MaybeReverse(flex_elem.second, flex_allocation_order())) {
      const int size = data.child_data[child_index].preferred_size.main();
      auto& indices = (size == 0) ? expandable_views[order] : view_indices;
      indices.push_back(child_index);
    }

    // Allocate zero-weight child views at this order first. This removes them
    // from |view_indices|.
    AllocateZeroWeightFlex(bounds, order, view_indices, data, child_spacing,
                           &expandable_views);

    // Iterate until all views can be allocated or are dropped out.
    for (SizeBound deficit;
         !view_indices.empty() &&
         (deficit = TryAllocateAll(bounds, order, view_indices, data,
                                   child_spacing, expandable_views)) > 0;) {
      // Process flex views with weight, allocating any shortage of flex space
      // below the views' minimum size based on weight, and dropping out any
      // views that fall to zero size.
      AllocateFlexShortageAtOrder(bounds, deficit, view_indices, data,
                                  child_spacing);
    }

    UpdateLayoutFromChildren(bounds, data, child_spacing);
  }
}

void FlexLayout::AllocateFlexExcess(
    const NormalizedSizeBounds& bounds,
    const FlexOrderToViewIndexMap& order_to_index,
    FlexLayoutData& data,
    ChildViewSpacing& child_spacing) const {
  // Step through each flex priority allocating as much remaining space as
  // possible to each remaining flex view.
  for (const auto& flex_elem : order_to_index) {
    const int order = flex_elem.first;

    // No need to reverse here because if we are reversed, then these values
    // were added in reverse order.
    ChildIndices view_indices = flex_elem.second;

    AllocateZeroWeightFlex(bounds, order, view_indices, data, child_spacing,
                           nullptr);

    // Allocate space to available children until all possible space is used up.
    for (SizeBound remaining =
             std::max<SizeBound>(0, bounds.main() - data.total_size.main());
         !view_indices.empty();) {
      AllocateFlexExcessAtOrder(bounds, remaining, view_indices, data,
                                child_spacing);
    }

    UpdateLayoutFromChildren(bounds, data, child_spacing);
  }
}

void FlexLayout::AllocateFlexShortageAtOrder(
    const NormalizedSizeBounds& bounds,
    SizeBound deficit,
    ChildIndices& child_list,
    FlexLayoutData& data,
    ChildViewSpacing& child_spacing) const {
  int flex_total = CalculateFlexTotal(data, child_list);

  // We'll process the views in reverse order so that views later in the order
  // are more likely to drop out/be shorted, which is consistent with the zero
  // weight behavior. That is, if the FlexAllocationOrder associated with this
  // layout is kNormal, views will drop from the end; while if it's kReverse,
  // views will drop from the beginning.
  std::map<size_t, NormalizedSize> pending_updates;
  for (auto it = child_list.rbegin(); it != child_list.rend(); ++it) {
    const size_t view_index = *it;
    FlexChildData& flex_child = data.child_data[view_index];
    ChildLayout& child_layout = data.layout.child_layouts[view_index];

    const int weight = flex_child.flex.weight();
    DCHECK_GT(weight, 0);
    DCHECK(deficit.is_bounded());
    const SizeBound to_deduct = base::ClampRound(
        deficit.value() * weight / static_cast<float>(flex_total));
    const SizeBound new_main = flex_child.preferred_size.main() - to_deduct;

    // If a view would shrink smaller than its current size, go with that and
    // eliminate it from the flex calculation.
    if (new_main <= flex_child.current_size.main()) {
      // Note that the iterator math ensures that the resulting forward iterator
      // actually points to the element being removed.
      child_list.erase(--it.base());
      return;
    }

    // See how much space the child view wants within the reduced space
    // remaining for it.
    const NormalizedSizeBounds available(
        new_main, GetCrossAxis(orientation(), child_layout.available_size));
    const NormalizedSize new_size = GetCurrentSizeForRule(
        flex_child.flex.rule(), child_layout.child_view, available);

    if (new_size.main() < new_main) {
      // Views that cap out below the allotted space can get their size set
      // immediately and they will drop out of subsequent passes.
      if (!new_size.is_empty() &&
          new_size.main() >= flex_child.current_size.main()) {
        flex_child.current_size = new_size;
        child_layout.visible = true;
        if (!child_spacing.HasViewIndex(view_index))
          child_spacing.AddViewIndex(view_index);
      }

      // Since the view has already been allocated, remove it from the
      // candidates list. The iterator math ensures that the resulting forward
      // iterator corresponds to the element being removed from the list.
      child_list.erase(--it.base());
      return;
    }

    // Changes to views that can take up the entire allotted space are held in
    // case we need to do them on another pass (since they might get additional
    // leftover space).
    pending_updates.emplace(view_index, new_size);

    // These numbers are based on ideal and not actual values we'll calculate
    // below, because we want views which cannot use all of their adjusted space
    // to drop out together rather than be order-dependent.
    flex_total -= weight;
    deficit -= to_deduct;
  }

  // We have successfully allocated all of the remaining space. Apply the
  // pending updates and we're done.
  for (size_t pending_index : child_list) {
    FlexChildData& flex_child = data.child_data[pending_index];
    ChildLayout& child_layout = data.layout.child_layouts[pending_index];
    flex_child.current_size = pending_updates[pending_index];
    child_layout.visible = true;
    if (!child_spacing.HasViewIndex(pending_index))
      child_spacing.AddViewIndex(pending_index);
  }
  child_list.clear();
}

void FlexLayout::AllocateFlexExcessAtOrder(
    const NormalizedSizeBounds& bounds,
    SizeBound& to_allocate,
    ChildIndices& child_list,
    FlexLayoutData& data,
    ChildViewSpacing& child_spacing) const {
  int flex_total = CalculateFlexTotal(data, child_list);

  // Collect views that have preferred size zero (and are therefore still not
  // visible) and see if we can allocate the additional required margins for
  // them. If we can, make them all visible. If not, none are visible.
  ChildIndices zero_size_children;
  ChildViewSpacing temp_spacing(child_spacing);
  const int old_spacing = temp_spacing.GetTotalSpace();
  base::ranges::copy_if(child_list, std::back_inserter(zero_size_children),
                        [&child_spacing](auto index) {
                          return !child_spacing.HasViewIndex(index);
                        });
  for (auto index : zero_size_children)
    temp_spacing.AddViewIndex(index);

  if (!zero_size_children.empty()) {
    // Make sure there is enough space to show each of the affected views. If
    // there is not, none of them appear, so remove them and bail out.
    const int new_spacing = temp_spacing.GetTotalSpace();
    const int delta = new_spacing - old_spacing;
    // We'll factor in |flex_total| so that each child view should be allocated
    // at least 1dp of space. That doesn't mean the child's flex rule will allow
    // it to take up that space (see note below).
    if (delta + flex_total > to_allocate) {
      child_list.remove_if([child_spacing](size_t index) {
        return !child_spacing.HasViewIndex(index);
      });
      return;
    }

    // Make all of the views visible, though note that at this point they are
    // still zero-size, which typically does not happen elsewhere in FlexLayout.
    // TODO(dfried): We could add a second boolean that would allow these views
    // to be set to not visible but still "take up space" in the layout, or
    // do some kind of post-processing pass to change the visibility flag to
    // false once all of the other computations are complete, but I don't think
    // it's worth the extra complexity until we have an actual use case or bug.
    to_allocate -= delta;
    child_spacing = temp_spacing;
    for (size_t view_index : zero_size_children)
      data.layout.child_layouts[view_index].visible = true;
  }

  // See if we can't get through the remaining views, allocating size for each.
  std::map<size_t, NormalizedSize> pending_updates;
  SizeBound remaining = to_allocate;
  for (auto it = child_list.begin(); remaining > 0 && it != child_list.end();
       ++it) {
    const size_t view_index = *it;
    FlexChildData& flex_child = data.child_data[view_index];
    ChildLayout& child_layout = data.layout.child_layouts[view_index];
    // On the excess pass, all of the views we're considering should be visible
    // (at least once we've cleared the bit above). We should have also handled
    // flex weight zero views earlier.
    DCHECK(child_layout.visible);

    const int weight = flex_child.flex.weight();
    DCHECK_GT(weight, 0);
    // Round up so we give slightly greater weight to earlier views.
    SizeBound flex_amount = remaining;
    if (remaining.is_bounded()) {
      flex_amount = base::ClampCeil(remaining.value() * weight /
                                    static_cast<float>(flex_total));
    }
    const int old_size = flex_child.current_size.main();
    const SizeBound new_main = flex_amount + old_size;

    const NormalizedSizeBounds available(
        new_main, GetCrossAxis(orientation(), child_layout.available_size));
    const NormalizedSize new_size = GetCurrentSizeForRule(
        flex_child.flex.rule(), child_layout.child_view, available);

    // In cases where a view does not take up its entire available size, we
    // need to set aside the space it does want and bail out (if there are other
    // views we'll repeat the allocation at this priority).
    const int to_deduct = new_size.main() - old_size;
    if (new_size.main() < new_main) {
      flex_child.current_size = new_size;
      to_allocate -= to_deduct;

      child_list.erase(it);
      return;
    }

    DCHECK_GE(to_deduct, 0);
    DCHECK_LE(to_deduct, remaining);
    pending_updates.emplace(view_index, new_size);

    flex_total -= weight;
    remaining -= to_deduct;
  }

  // If we get here, we successfully allocated all of the space, so update
  // everything and we're done.
  to_allocate = remaining;
  for (const auto& update : pending_updates)
    data.child_data[update.first].current_size = update.second;
  child_list.clear();
}

void FlexLayout::CalculateFlexAvailableSpace(
    const NormalizedSizeBounds& bounds,
    const ChildIndices& child_indices,
    const ChildViewSpacing& child_spacing,
    FlexLayoutData& data) const {
  const SizeBound remaining_at_priority =
      std::max<SizeBound>(0, bounds.main() - data.total_size.main());
  for (size_t index : child_indices) {
    // We'll save the maximum amount of main axis size first offered to the
    // view so we can report the maximum available size later. We only need to
    // do this the first time because the available space decreases
    // monotonically as we allocate flex space.
    ChildLayout& child_layout = data.layout.child_layouts[index];
    if (!GetMainAxis(orientation(), child_layout.available_size).is_bounded()) {
      // Calculate how much space this child view could take based on the
      // total remaining flex space at this priority. Note that this is not
      // the actual remaining space at this step, which will be based on flex
      // used by previous children at the same priority.
      const FlexChildData& flex_child = data.child_data[index];
      const int old_size =
          child_layout.visible ? flex_child.current_size.main() : 0;
      const SizeBound available_size = std::max<SizeBound>(
          flex_child.current_size.main(),
          child_spacing.GetMaxSize(index, old_size, remaining_at_priority));
      SetMainAxis(&child_layout.available_size, orientation(), available_size);
    }
  }
}

void FlexLayout::AllocateZeroWeightFlex(
    const NormalizedSizeBounds& bounds,
    int flex_order,
    ChildIndices& child_list,
    FlexLayoutData& data,
    ChildViewSpacing& child_spacing,
    FlexOrderToViewIndexMap* expandable_views) const {
  SizeBound remaining =
      std::max<SizeBound>(0, bounds.main() - data.total_size.main());
  const bool is_first_pass = expandable_views != nullptr;
  bool need_to_update_layout = false;

  // Allocate space to views with zero flex weight. They get first priority at
  // this priority order.
  auto it = child_list.begin();
  while (it != child_list.end()) {
    const size_t child_index = *it;
    FlexChildData& flex_child = data.child_data[child_index];

    // We don't care about weighted flex in this step.
    if (flex_child.flex.weight() > 0) {
      ++it;
      continue;
    }

    ChildLayout& child_layout = data.layout.child_layouts[child_index];
    DCHECK(is_first_pass || child_layout.visible ||
           flex_child.preferred_size.main() == 0);

    const int old_size =
        child_layout.visible ? flex_child.current_size.main() : 0;
    const SizeBound available_cross =
        GetCrossAxis(orientation(), child_layout.available_size);
    const SizeBound available_main =
        child_spacing.GetMaxSize(child_index, old_size, remaining);
    const NormalizedSizeBounds available(available_main, available_cross);
    NormalizedSize new_size = GetCurrentSizeForRule(
        flex_child.flex.rule(), child_layout.child_view, available);

    if (is_first_pass && new_size.main() > flex_child.preferred_size.main()) {
      new_size.set_main(flex_child.preferred_size.main());
      (*expandable_views)[flex_order].push_back(child_index);
    }

    if (new_size.main() > old_size) {
      const int delta = child_spacing.GetTotalSizeChangeForNewSize(
          child_index, old_size, new_size.main());
      remaining -= delta;
      child_layout.visible = true;
      flex_child.current_size = new_size;
      if (!child_spacing.HasViewIndex(child_index))
        child_spacing.AddViewIndex(child_index);
      need_to_update_layout = true;
    }
    it = child_list.erase(it);
  }

  if (need_to_update_layout)
    UpdateLayoutFromChildren(bounds, data, child_spacing);
}

SizeBound FlexLayout::TryAllocateAll(
    const NormalizedSizeBounds& bounds,
    int flex_order,
    const ChildIndices& child_list,
    FlexLayoutData& data,
    ChildViewSpacing& child_spacing,
    FlexOrderToViewIndexMap& expandable_views) const {
  // Compute a new proposed spacing resulting from adding all the remaining
  // child views at this order at their preferred sizes.
  ChildViewSpacing proposed_spacing(child_spacing);
  int delta = 0;
  for (size_t child_index : child_list) {
    const FlexChildData& flex_child = data.child_data[child_index];
    delta += proposed_spacing.GetTotalSizeChangeForNewSize(
        child_index, flex_child.current_size.main(),
        flex_child.preferred_size.main());
    if (!proposed_spacing.HasViewIndex(child_index))
      proposed_spacing.AddViewIndex(child_index);
  }
  const int new_total_size = data.total_size.main() + delta;
  const SizeBound deficit =
      std::max<SizeBound>(0, new_total_size - bounds.main());

  if (deficit == 0) {
    // If there's enough space to add all of these views up to their preferred
    // size then add them all, and if there's excess space, add the children
    // to |expandable_views| as well.
    for (size_t child_index : child_list) {
      FlexChildData& flex_child = data.child_data[child_index];
      if (flex_child.current_size.main() != flex_child.preferred_size.main()) {
        // Need to recalculate the ideal size in the given bounds, which might
        // not always be the preferred size.
        const ChildLayout& child_layout =
            data.layout.child_layouts[child_index];
        const NormalizedSize new_size = GetCurrentSizeForRule(
            flex_child.flex.rule(), child_layout.child_view,
            NormalizedSizeBounds(
                flex_child.preferred_size.main(),
                GetCrossAxis(orientation(), child_layout.available_size)));
        flex_child.current_size =
            NormalizedSize(flex_child.preferred_size.main(), new_size.cross());
        data.layout.child_layouts[child_index].visible = true;
      }
    }
    if (new_total_size < bounds.main()) {
      base::ranges::copy(child_list,
                         std::back_inserter(expandable_views[flex_order]));
    }

    // All children have been allocated for this step at this point.
    child_spacing = proposed_spacing;
  }

  return deficit;
}

// static
int FlexLayout::CalculateFlexTotal(const FlexLayoutData& data,
                                   const ChildIndices& child_indices) {
  return std::accumulate(child_indices.begin(), child_indices.end(), 0,
                         [&data](int total, size_t index) {
                           return total + data.child_data[index].flex.weight();
                         });
}

// static
gfx::Size FlexLayout::DefaultFlexRuleImpl(const FlexLayout* flex_layout,
                                          const View* view,
                                          const SizeBounds& size_bounds) {
  if (size_bounds == SizeBounds())
    return flex_layout->GetPreferredSize(view);
  if (size_bounds == SizeBounds(0, 0))
    return flex_layout->GetMinimumSize(view);
  return flex_layout->CalculateProposedLayout(size_bounds).host_size;
}

}  // namespace views
