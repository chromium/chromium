// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/flex_layout.h"

#include <algorithm>
#include <functional>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "ui/events/event_target.h"
#include "ui/events/event_target_iterator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/normalized_geometry.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

// Module-private declarations -------------------------------------------------

namespace views {

namespace {

// Layout information for a specific child view in a proposed layout.
struct FlexChildData {
  explicit FlexChildData(const FlexSpecification& flex) : flex(flex) {}
  FlexChildData(FlexChildData&& other) = default;

  // Start with zero size rather than unspecified size bounds because we start
  // all layouts with controls at their minimum allowed sizes.
  NormalizedSizeBounds available_size{0, 0};
  NormalizedSize preferred_size;
  NormalizedSize current_size;
  NormalizedInsets margins;
  bool using_default_margins = true;
  NormalizedInsets internal_padding;
  NormalizedRect actual_bounds;
  FlexSpecification flex;

 private:
  // Copying this struct would be expensive and they only ever live in a vector
  // in Layout (see below) so we'll only allow move semantics.
  DISALLOW_COPY_AND_ASSIGN(FlexChildData);
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

}  // anonymous namespace

// Private implementation ------------------------------------------------------

// Calculates and maintains 1D spacing between a sequence of child views.
class FlexLayout::ChildViewSpacing {
 public:
  // Given the indices of two child views, returns the amount of space that
  // should be placed between them if they were adjacent. If the first index is
  // absent, uses the left edge of the parent container. If the second index is
  // absent, uses the right edge of the parent container.
  using GetViewSpacingCallback =
      base::RepeatingCallback<int(base::Optional<size_t>,
                                  base::Optional<size_t>)>;

  explicit ChildViewSpacing(GetViewSpacingCallback get_view_spacing);
  ChildViewSpacing(ChildViewSpacing&& other);
  ChildViewSpacing& operator=(ChildViewSpacing&& other);

  bool HasViewIndex(size_t view_index) const;
  int GetLeadingInset() const;
  int GetTrailingInset() const;
  int GetLeadingSpace(size_t view_index) const;
  int GetTotalSpace() const;

  // Returns the change in space required if the specified view index were
  // added.
  int GetAddDelta(size_t view_index) const;

  // Add the view at the specified index.
  //
  // If |new_leading| or |new_trailing| is specified, it will be set to the new
  // leading/trailing space for the view at the index that was added.
  void AddViewIndex(size_t view_index,
                    int* new_leading = nullptr,
                    int* new_trailing = nullptr);

 private:
  base::Optional<size_t> GetPreviousViewIndex(size_t view_index) const;
  base::Optional<size_t> GetNextViewIndex(size_t view_index) const;

  GetViewSpacingCallback get_view_spacing_;
  // Maps from view index to the leading spacing for that index.
  std::map<size_t, int> leading_spacings_;
  // The trailing space (space preceding the trailing margin).
  int trailing_space_;
};

FlexLayout::ChildViewSpacing::ChildViewSpacing(
    GetViewSpacingCallback get_view_spacing)
    : get_view_spacing_(std::move(get_view_spacing)),
      trailing_space_(get_view_spacing_.Run(base::nullopt, base::nullopt)) {}

FlexLayout::ChildViewSpacing::ChildViewSpacing(ChildViewSpacing&& other)
    : get_view_spacing_(std::move(other.get_view_spacing_)),
      leading_spacings_(std::move(other.leading_spacings_)),
      trailing_space_(other.trailing_space_) {}

FlexLayout::ChildViewSpacing& FlexLayout::ChildViewSpacing::operator=(
    ChildViewSpacing&& other) {
  if (this != &other) {
    get_view_spacing_ = std::move(other.get_view_spacing_);
    leading_spacings_ = std::move(other.leading_spacings_);
    trailing_space_ = other.trailing_space_;
  }
  return *this;
}

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

int FlexLayout::ChildViewSpacing::GetAddDelta(size_t view_index) const {
  DCHECK(!HasViewIndex(view_index));
  base::Optional<size_t> prev = GetPreviousViewIndex(view_index);
  base::Optional<size_t> next = GetNextViewIndex(view_index);
  const int old_spacing = next ? GetLeadingSpace(*next) : GetTrailingInset();
  const int new_spacing = get_view_spacing_.Run(prev, view_index) +
                          get_view_spacing_.Run(view_index, next);
  return new_spacing - old_spacing;
}

void FlexLayout::ChildViewSpacing::AddViewIndex(size_t view_index,
                                                int* new_leading,
                                                int* new_trailing) {
  DCHECK(!HasViewIndex(view_index));
  base::Optional<size_t> prev = GetPreviousViewIndex(view_index);
  base::Optional<size_t> next = GetNextViewIndex(view_index);

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

base::Optional<size_t> FlexLayout::ChildViewSpacing::GetPreviousViewIndex(
    size_t view_index) const {
  const auto it = leading_spacings_.lower_bound(view_index);
  if (it == leading_spacings_.begin())
    return base::nullopt;
  return std::prev(it)->first;
}

base::Optional<size_t> FlexLayout::ChildViewSpacing::GetNextViewIndex(
    size_t view_index) const {
  const auto it = leading_spacings_.upper_bound(view_index);
  if (it == leading_spacings_.end())
    return base::nullopt;
  return it->first;
}

// Represents a specific stored layout given a set of size bounds.
struct FlexLayout::FlexLayoutData {
  FlexLayoutData() {}
  ~FlexLayoutData() = default;

  size_t num_children() const { return child_data.size(); }

  ProposedLayout layout;

  // Holds additional information about the child views of this layout.
  std::vector<FlexChildData> child_data;

  // The total size of the layout (minus parent insets).
  NormalizedSize total_size;
  NormalizedInsets interior_margin;
  NormalizedInsets host_insets;

 private:
  DISALLOW_COPY_AND_ASSIGN(FlexLayoutData);
};

FlexLayout::PropertyHandler::PropertyHandler(FlexLayout* layout)
    : layout_(layout) {}

void FlexLayout::PropertyHandler::AfterPropertyChange(const void* key,
                                                      int64_t old_value) {
  layout_->InvalidateHost(true);
}

// FlexLayout
// -------------------------------------------------------------------

FlexLayout::FlexLayout() {}
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
  if (cross_axis_alignment_ != cross_axis_alignment) {
    cross_axis_alignment_ = cross_axis_alignment;
    InvalidateHost(true);
  }
  return *this;
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

FlexLayout& FlexLayout::SetBetweenChildSpacing(int between_child_spacing) {
  if (between_child_spacing_ != between_child_spacing) {
    between_child_spacing_ = between_child_spacing;
    InvalidateHost(true);
  }
  return *this;
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
  if (bounds.cross() && *bounds.cross() < minimum_cross_axis_size())
    bounds.set_cross(minimum_cross_axis_size());

  // Populate the child layout data vectors and the order-to-index map.
  FlexOrderToViewIndexMap order_to_view_index;
  InitializeChildData(bounds, &data, &order_to_view_index);

  // Do the initial layout update, calculating spacing between children.
  ChildViewSpacing child_spacing(
      base::BindRepeating(&FlexLayout::CalculateChildSpacing,
                          base::Unretained(this), std::cref(data)));
  UpdateLayoutFromChildren(bounds, &data, &child_spacing);

  if (bounds.main().has_value() && !order_to_view_index.empty()) {
    // Flex up to preferred size.
    FlexOrderToViewIndexMap expandable_views;
    AllocateFlexSpace(bounds, order_to_view_index, &data, &child_spacing,
                      &expandable_views);

    // Flex views that can exceed their preferred size.
    if (!expandable_views.empty())
      AllocateFlexSpace(bounds, expandable_views, &data, &child_spacing);
  }

  // Calculate the size of the host view.
  NormalizedSize host_size = data.total_size;
  host_size.Enlarge(data.host_insets.main_size(),
                    data.host_insets.cross_size());
  data.layout.host_size = Denormalize(orientation(), host_size);

  // Size and position the children in screen space.
  CalculateChildBounds(size_bounds, &data);

  return data.layout;
}

void FlexLayout::InitializeChildData(
    const NormalizedSizeBounds& bounds,
    FlexLayoutData* data,
    FlexOrderToViewIndexMap* flex_order_to_index) const {
  // Step through the children, creating placeholder layout view elements
  // and setting up initial minimal visibility.
  const bool main_axis_bounded = bounds.main().has_value();
  for (auto it = host_view()->children().begin();
       it != host_view()->children().end(); ++it) {
    View* const child = *it;
    if (!IsChildIncludedInLayout(child))
      continue;
    const size_t view_index = data->num_children();
    data->layout.child_layouts.emplace_back(ChildLayout{child});
    ChildLayout& child_layout = data->layout.child_layouts.back();
    data->child_data.emplace_back(
        GetViewProperty(child, layout_defaults_, views::kFlexBehaviorKey));
    FlexChildData& flex_child = data->child_data.back();

    flex_child.margins =
        Normalize(orientation(),
                  GetViewProperty(child, layout_defaults_, views::kMarginsKey,
                                  &flex_child.using_default_margins));
    flex_child.internal_padding = Normalize(
        orientation(),
        GetViewProperty(child, layout_defaults_, views::kInternalPaddingKey));
    flex_child.preferred_size =
        Normalize(orientation(), child->GetPreferredSize());

    // gfx::Size calculation depends on whether flex is allowed.
    if (main_axis_bounded) {
      flex_child.available_size = {
          0, GetAvailableCrossAxisSize(*data, view_index, bounds)};
      flex_child.current_size = Normalize(
          orientation(),
          flex_child.flex.rule().Run(
              child, Denormalize(orientation(), flex_child.available_size)));

      // We should revisit whether this is a valid assumption for text views
      // in vertical layouts.
      DCHECK_GE(flex_child.preferred_size.main(),
                flex_child.current_size.main())
          << " in " << child->GetClassName();

      // Keep track of non-hidden flex controls.
      const bool can_flex =
          flex_child.flex.weight() > 0 ||
          flex_child.current_size.main() < flex_child.preferred_size.main();
      if (can_flex)
        (*flex_order_to_index)[flex_child.flex.order()].push_back(view_index);

    } else {
      // All non-flex or unbounded controls get preferred size.
      flex_child.current_size = flex_child.preferred_size;
    }

    child_layout.visible = flex_child.current_size.main() > 0;
  }
}

void FlexLayout::CalculateChildBounds(const SizeBounds& size_bounds,
                                      FlexLayoutData* data) const {
  // Apply main axis alignment (we've already done cross-axis alignment above).
  const NormalizedSizeBounds normalized_bounds =
      Normalize(orientation(), size_bounds);
  const NormalizedSize normalized_host_size =
      Normalize(orientation(), data->layout.host_size);
  int available_main = (normalized_bounds.main() ? *normalized_bounds.main()
                                                 : normalized_host_size.main());
  available_main = std::max(0, available_main - data->host_insets.main_size());
  const int excess_main = available_main - data->total_size.main();
  NormalizedPoint start(data->host_insets.main_leading(),
                        data->host_insets.cross_leading());
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
      NOTIMPLEMENTED() << "Main axis stretch/justify is not yet supported.";
      break;
  }

  // Calculate the actual child bounds.
  for (size_t i = 0; i < data->num_children(); ++i) {
    ChildLayout& child_layout = data->layout.child_layouts[i];
    if (child_layout.visible) {
      FlexChildData& flex_child = data->child_data[i];
      NormalizedRect actual = flex_child.actual_bounds;
      actual.Offset(start.main(), start.cross());
      if (actual.size_main() > flex_child.preferred_size.main() &&
          flex_child.flex.alignment() != LayoutAlignment::kStretch) {
        Span container{actual.origin_main(), actual.size_main()};
        Span new_main{0, flex_child.preferred_size.main()};
        new_main.Align(container, flex_child.flex.alignment());
        actual.set_origin_main(new_main.start());
        actual.set_size_main(new_main.length());
      }
      child_layout.bounds = Denormalize(orientation(), actual);
    }
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
                                int internal_padding,
                                int spacing) const {
  const int result = collapse_margins() ? std::max({margin1, margin2, spacing})
                                        : margin1 + margin2 + spacing;
  return std::max(0, result - internal_padding);
}

base::Optional<int> FlexLayout::GetAvailableCrossAxisSize(
    const FlexLayoutData& layout,
    size_t child_index,
    const NormalizedSizeBounds& bounds) const {
  if (!bounds.cross())
    return base::nullopt;
  const Inset1D cross_margins = GetCrossAxisMargins(layout, child_index);
  return std::max(0, *bounds.cross() - cross_margins.size());
}

int FlexLayout::CalculateChildSpacing(
    const FlexLayoutData& layout,
    base::Optional<size_t> child1_index,
    base::Optional<size_t> child2_index) const {
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
                         left_padding + right_padding,
                         (child1 && child2) ? between_child_spacing() : 0);
}

void FlexLayout::UpdateLayoutFromChildren(
    const NormalizedSizeBounds& bounds,
    FlexLayoutData* data,
    ChildViewSpacing* child_spacing) const {
  // Calculate starting minimum for cross-axis size.
  int min_cross_size =
      std::max(minimum_cross_axis_size(),
               CalculateMargin(data->interior_margin.cross_leading(),
                               data->interior_margin.cross_trailing(), 0));
  data->total_size = NormalizedSize(0, min_cross_size);

  // For cases with a non-zero cross-axis bound, the objective is to fit the
  // layout into that precise size, not to determine what size we need.
  bool force_cross_size = false;
  if (bounds.cross() && *bounds.cross() > 0) {
    data->total_size.SetToMax(0, *bounds.cross());
    force_cross_size = true;
  }

  std::vector<Inset1D> cross_spacings(data->num_children());
  for (size_t i = 0; i < data->num_children(); ++i) {
    FlexChildData& flex_child = data->child_data[i];

    // We don't have to deal with invisible children.
    if (!data->layout.child_layouts[i].visible)
      continue;

    // Update the cross-axis margins and if necessary, the size.
    cross_spacings[i] = GetCrossAxisMargins(*data, i);

    if (!force_cross_size) {
      const int cross_size = std::min(flex_child.current_size.cross(),
                                      flex_child.preferred_size.cross());
      data->total_size.SetToMax(0, cross_spacings[i].size() + cross_size);
    }

    // Calculate main-axis size and upper-left main axis coordinate.
    int leading_space;
    if (child_spacing->HasViewIndex(i))
      leading_space = child_spacing->GetLeadingSpace(i);
    else
      child_spacing->AddViewIndex(i, &leading_space);
    data->total_size.Enlarge(leading_space, 0);

    const int size_main = flex_child.current_size.main();
    flex_child.actual_bounds.set_origin_main(data->total_size.main());
    flex_child.actual_bounds.set_size_main(size_main);
    data->total_size.Enlarge(size_main, 0);
  }

  // Add the end margin.
  data->total_size.Enlarge(child_spacing->GetTrailingInset(), 0);

  // Calculate cross-axis positioning based on the cross margins and size that
  // were calculated above.
  const Span cross_span(0, data->total_size.cross());
  for (size_t i = 0; i < data->num_children(); ++i) {
    FlexChildData& flex_child = data->child_data[i];

    // Start with a size appropriate for the child view. For child views which
    // can become larger than the preferred size, start with the preferred size
    // and let the alignment operation (specifically, if the alignment is set to
    // kStretch) grow the child view.
    const int starting_cross_size = std::min(flex_child.current_size.cross(),
                                             flex_child.preferred_size.cross());
    flex_child.actual_bounds.set_size_cross(starting_cross_size);
    flex_child.actual_bounds.AlignCross(cross_span, cross_axis_alignment(),
                                        cross_spacings[i]);
  }
}

void FlexLayout::AllocateFlexSpace(
    const NormalizedSizeBounds& bounds,
    const FlexOrderToViewIndexMap& order_to_index,
    FlexLayoutData* data,
    ChildViewSpacing* child_spacing,
    FlexOrderToViewIndexMap* expandable_views) const {
  // Step through each flex priority allocating as much remaining space as
  // possible to each flex view.
  for (const auto& flex_elem : order_to_index) {
    // Check to see we haven't filled available space.
    int remaining = *bounds.main() - data->total_size.main();
    if (remaining <= 0)
      break;
    const int flex_order = flex_elem.first;

    // The flex algorithm we're using works as follows:
    //  * For each child view at a particular flex order:
    //    - Calculate the percentage of the remaining flex space to allocate
    //      based on the ratio of its weight to the total unallocated weight
    //      at that order.
    //    - If the child view is already visible (it will be at its minimum
    //      size, which may or may not be zero), add the space the child is
    //      already taking up.
    //    - If the child view is not visible and adding it would introduce
    //      additional margin space between child views, subtract that
    //      additional space from the amount available.
    //    - Ask the child view's flex rule how large it would like to be
    //      within the space available.
    //    - If the child view would like to be larger, make it so, and
    //      subtract the additional space consumed by the child and its
    //      margins from the total remaining flex space.
    //
    // Note that this algorithm isn't *perfect* for specific cases, which are
    // noted below; namely when margins very asymmetrical the sizing of child
    // views can be slightly different from what would otherwise be expected.
    // We have a TODO to look at ways of making this algorithm more "fair" in
    // the future (but in the meantime most issues can be resolved by setting
    // reasonable margins and by using flex order).

    // Flex children at this priority order.
    int flex_total =
        std::accumulate(flex_elem.second.begin(), flex_elem.second.end(), 0,
                        [data](int total, size_t index) {
                          return total + data->child_data[index].flex.weight();
                        });

    // Note: because the child views are evaluated in order, if preferred
    // minimum sizes are not consistent across a single priority expanding
    // the parent control could result in children swapping visibility.
    // We currently consider this user error; if the behavior is not
    // desired, prioritize the child views' flex.
    bool dirty = false;
    for (auto index_it = flex_elem.second.begin();
         remaining >= 0 && index_it != flex_elem.second.end(); ++index_it) {
      const size_t view_index = *index_it;

      ChildLayout& child_layout = data->layout.child_layouts[view_index];
      FlexChildData& flex_child = data->child_data[view_index];

      // Offer a share of the remaining space to the view.
      int flex_amount;
      if (flex_child.flex.weight() > 0) {
        const int flex_weight = flex_child.flex.weight();
        // Round up so we give slightly greater weight to earlier views.
        flex_amount =
            int{std::ceil((float{remaining} * flex_weight) / flex_total)};
        flex_total -= flex_weight;
      } else {
        flex_amount = remaining;
      }

      // If the layout was previously invisible, then making it visible
      // may result in the addition of margin space, so we'll have to
      // recalculate the margins on either side of this view. The change in
      // margin space (if any) counts against the child view's flex space
      // allocation.
      //
      // Note: In cases where the layout's internal margins and/or the child
      // views' margins are wildly different sizes, subtracting the full delta
      // out of the available space can cause the first view to be smaller
      // than we would expect (see TODOs in unit tests for examples). We
      // should look into ways to make this "feel" better (but in the
      // meantime, please try to specify reasonable margins).
      const int margin_delta = child_spacing->HasViewIndex(view_index)
                                   ? 0
                                   : child_spacing->GetAddDelta(view_index);

      // This is the space on the main axis that was already allocated to the
      // child view; it will be added to the total flex space for the child
      // view since it is considered a fixed overhead of the layout if it is
      // nonzero.
      const int old_size =
          child_layout.visible ? flex_child.current_size.main() : 0;

      // Offer the modified flex space to the child view and see how large it
      // wants to be (or if it wants to be visible at that size at all).
      const NormalizedSizeBounds available(
          flex_amount + old_size - margin_delta,
          flex_child.available_size.cross());
      NormalizedSize new_size = Normalize(
          orientation(),
          flex_child.flex.rule().Run(child_layout.child_view,
                                     Denormalize(orientation(), available)));
      if (new_size.main() <= 0)
        continue;

      // Limit the expansion of views past their preferred size in the first
      // pass so that enough space is available for lower-priority views. Save
      // them to |expandable_views| so that the remaining space can be allocated
      // later.
      if (expandable_views &&
          new_size.main() > flex_child.preferred_size.main()) {
        (*expandable_views)[flex_order].push_back(view_index);
        new_size.set_main(flex_child.preferred_size.main());
      }

      // If the amount of space claimed increases (but is still within
      // bounds set by our flex rule) we can make the control visible and
      // claim the additional space.
      const int to_deduct = (new_size.main() - old_size) + margin_delta;
      DCHECK_GE(to_deduct, 0);
      if (to_deduct > 0 && to_deduct <= remaining) {
        flex_child.available_size = available;
        flex_child.current_size = new_size;
        child_layout.visible = true;
        remaining -= to_deduct;
        if (!child_spacing->HasViewIndex(view_index))
          child_spacing->AddViewIndex(view_index);
        dirty = true;
      }
    }

    // Reposition the child controls (taking margins into account) and
    // calculate remaining space.
    if (dirty)
      UpdateLayoutFromChildren(bounds, data, child_spacing);
  }
}

}  // namespace views
