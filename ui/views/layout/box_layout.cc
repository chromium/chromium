// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/box_layout.h"

#include <algorithm>
#include <numeric>
#include <string>
#include <vector>

#include "base/containers/adapters.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/layout/normalized_geometry.h"
#include "ui/views/view_class_properties.h"

namespace views {

namespace {

struct BoxChildData {
  BoxChildData() = default;

  // Copying this struct would be expensive and they only ever live in a vector
  // in Layout (see below) so we'll only allow move semantics.
  BoxChildData(const BoxChildData&) = delete;
  BoxChildData& operator=(const BoxChildData&) = delete;

  BoxChildData(BoxChildData&& other) = default;

  std::string ToString() const {
    return base::StrCat({"{ preferred ", preferred_size.ToString(), " margins ",
                         margins.ToString(), " bounds ",
                         actual_bounds.ToString(), " }"});
  }

  NormalizedSize preferred_size;
  NormalizedInsets margins;
  NormalizedRect actual_bounds;
  int flex;
};

NormalizedInsets MaxAxisInsets(const NormalizedInsets& leading1,
                               const NormalizedInsets& leading2,
                               const NormalizedInsets& trailing1,
                               const NormalizedInsets& trailing2) {
  return NormalizedInsets(
      std::max(leading1.main_leading(), leading2.main_leading()), 0,
      std::max(trailing1.main_trailing(), trailing2.main_trailing()), 0);
}

}  // namespace

struct BoxLayout::BoxLayoutData {
  BoxLayoutData() = default;

  BoxLayoutData(const BoxLayoutData&) = delete;
  BoxLayoutData& operator=(const BoxLayoutData&) = delete;

  ~BoxLayoutData() = default;

  size_t num_children() const { return child_data.size(); }

  std::string ToString() const {
    std::string result = base::StrCat({"{ host_size ", total_size.ToString(),
                                       ", cross_center_pos ",
                                       base::NumberToString(cross_center_pos),
                                       "\n", layout.ToString(), " {\n"});
    bool first = true;
    for (const BoxChildData& flex_child : child_data) {
      if (first) {
        first = false;
      } else {
        base::StrAppend(&result, {",\n"});
      }
      base::StrAppend(&result, {flex_child.ToString()});
    }
    base::StrAppend(&result,
                    {"}\nmargin ", interior_margin.ToString(), " insets ",
                     host_insets.ToString(), "\nmax_cross_margin ",
                     max_cross_margin.ToString()});
    return result;
  }

  ProposedLayout layout;

  // Holds additional information about the child views of this layout.
  std::vector<BoxChildData> child_data;

  NormalizedSize total_size;
  NormalizedInsets host_insets;
  NormalizedInsets interior_margin;
  Inset1D max_cross_margin;
  int cross_center_pos;
};

// BoxLayoutFlexSpecification --------------------------------------------------

BoxLayoutFlexSpecification::BoxLayoutFlexSpecification() = default;

BoxLayoutFlexSpecification BoxLayoutFlexSpecification::WithWeight(
    int weight) const {
  DCHECK_GE(weight, 0);
  BoxLayoutFlexSpecification spec = *this;
  spec.weight_ = weight;
  return spec;
}

BoxLayoutFlexSpecification BoxLayoutFlexSpecification::UseMinSize(
    bool use_min_size) const {
  BoxLayoutFlexSpecification spec = *this;
  spec.use_min_size_ = use_min_size;
  return spec;
}

BoxLayoutFlexSpecification::~BoxLayoutFlexSpecification() = default;

// BoxLayout --------------------------------------------------

BoxLayout::BoxLayout(BoxLayout::Orientation orientation,
                     const gfx::Insets& inside_border_insets,
                     int between_child_spacing,
                     bool collapse_margins_spacing)
    : orientation_(orientation),
      inside_border_insets_(inside_border_insets),
      between_child_spacing_(between_child_spacing),
      collapse_margins_spacing_(collapse_margins_spacing) {}

BoxLayout::~BoxLayout() = default;

void BoxLayout::SetOrientation(Orientation orientation) {
  if (orientation_ != orientation) {
    orientation_ = orientation;
    InvalidateHost(true);
  }
}

BoxLayout::Orientation BoxLayout::GetOrientation() const {
  return orientation_;
}

void BoxLayout::set_main_axis_alignment(MainAxisAlignment main_axis_alignment) {
  if (main_axis_alignment_ != main_axis_alignment) {
    main_axis_alignment_ = main_axis_alignment;
    InvalidateHost(true);
  }
}

void BoxLayout::set_cross_axis_alignment(
    CrossAxisAlignment cross_axis_alignment) {
  if (cross_axis_alignment_ != cross_axis_alignment) {
    cross_axis_alignment_ = cross_axis_alignment;
    InvalidateHost(true);
  }
}

void BoxLayout::set_inside_border_insets(const gfx::Insets& insets) {
  if (inside_border_insets_ != insets) {
    inside_border_insets_ = insets;
    InvalidateHost(true);
  }
}

void BoxLayout::SetCollapseMarginsSpacing(bool collapse_margins_spacing) {
  if (collapse_margins_spacing != collapse_margins_spacing_) {
    collapse_margins_spacing_ = collapse_margins_spacing;
    InvalidateHost(true);
  }
}

bool BoxLayout::GetCollapseMarginsSpacing() const {
  return collapse_margins_spacing_;
}

void BoxLayout::SetFlexForView(const View* view,
                               int flex_weight,
                               bool use_min_size) {
  DCHECK(view);
  DCHECK_EQ(host_view(), view->parent());
  DCHECK_GE(flex_weight, 0);
  const_cast<View*>(view)->SetProperty(kBoxLayoutFlexKey,
                                       BoxLayoutFlexSpecification()
                                           .WithWeight(flex_weight)
                                           .UseMinSize(use_min_size));
}

void BoxLayout::ClearFlexForView(const View* view) {
  DCHECK(view);
  const_cast<View*>(view)->ClearProperty(kBoxLayoutFlexKey);
}

void BoxLayout::SetDefaultFlex(int default_flex) {
  DCHECK_GE(default_flex, 0);
  default_flex_ = default_flex;
}

int BoxLayout::GetDefaultFlex() const {
  return default_flex_;
}

ProposedLayout BoxLayout::CalculateProposedLayout(
    const SizeBounds& size_bounds) const {
  BoxLayoutData data;
  InitializeChildData(data);

  gfx::Insets insets = host_view()->GetInsets();
  data.interior_margin = Normalize(orientation_, inside_border_insets_);

  // TODO(crbug.com/1346889): In a vertical layout, if the width is not
  // specified, we need to first calculate the maximum width of the view, which
  // makes it convenient for us to call GetHeightForWidth later. If all views
  // are modified to GetPreferredSize(const SizeBounds&), we might consider
  // removing this part.
  SizeBounds new_bounds(size_bounds);
  if (!new_bounds.width().is_bounded() &&
      orientation_ == Orientation::kVertical) {
    new_bounds.set_width(CalculateMaxChildWidth(data));
  }

  NormalizedSizeBounds bounds = Normalize(orientation_, new_bounds);

  // When |collapse_margins_spacing_ = true|, the host_insets include the
  // leading of the first element and the trailing of the last one. It's crucial
  // to keep this in mind while reading here. Conversely, they are not included.
  if (collapse_margins_spacing_) {
    NormalizedInsets main_axis_insets =
        MaxAxisInsets(data.interior_margin,
                      data.child_data.empty() ? NormalizedInsets()
                                              : data.child_data.front().margins,
                      data.interior_margin,
                      data.child_data.empty() ? NormalizedInsets()
                                              : data.child_data.back().margins);
    data.host_insets = Normalize(
        orientation_, insets + Denormalize(orientation_, main_axis_insets));
  } else {
    data.host_insets = Normalize(orientation_, insets + inside_border_insets_);
  }
  bounds.Inset(data.host_insets);

  CalculatePreferredSize(Denormalize(orientation_, bounds), data);

  // Calculate the size of the view without boundary constraints. This is the
  // default size of our view.
  CalculatePreferredTotalSize(data);

  // Update the position information of the child views.
  UpdateFlexLayout(bounds, data);

  NormalizedSize host_size = data.total_size;
  host_size.Enlarge(data.host_insets.main_size(),
                    data.host_insets.cross_size());

  // TODO(weidongliu): see crbugs.com/1514004#c5, we handle compatibility here.
  // Maybe we can remove this in the future.
  if (collapse_margins_spacing_) {
    host_size.Enlarge(data.interior_margin.main_size(), 0);
  }
  data.layout.host_size = Denormalize(orientation_, host_size);

  CalculateChildBounds(size_bounds, data);

  return data.layout;
}

int BoxLayout::GetFlexForView(const View* view) const {
  // Respect flex provided via the `kBoxLayoutFlexKey`.
  if (auto* flex_behavior_key = view->GetProperty(kBoxLayoutFlexKey)) {
    return flex_behavior_key->weight();
  }
  // Fall back to default.
  return default_flex_;
}

int BoxLayout::GetMinimumSizeForView(const View* view) const {
  auto* flex_behavior_key = view->GetProperty(kBoxLayoutFlexKey);
  if (!flex_behavior_key || !flex_behavior_key->use_min_size()) {
    return 0;
  }

  return (orientation_ == Orientation::kHorizontal)
             ? view->GetMinimumSize().width()
             : view->GetMinimumSize().height();
}

gfx::Size BoxLayout::GetPreferredSizeForView(
    const View* view,
    const NormalizedSizeBounds& size_bounds) const {
  // TODO(crbug.com/1346889): If all views are migrated to
  // GetPreferredSize(const SizeBounds&), simplify the processing here.
  if (orientation_ == Orientation::kVertical &&
      cross_axis_alignment_ == CrossAxisAlignment::kStretch) {
    int width = size_bounds.cross().value();
    return gfx::Size(width, view->GetHeightForWidth(width));
  } else {
    gfx::Size bounded_preferred_size =
        view->GetPreferredSize(Denormalize(orientation_, size_bounds));
    if (orientation_ == Orientation::kHorizontal) {
      return bounded_preferred_size;
    } else {
      int width = size_bounds.cross().min_of(bounded_preferred_size.width());
      return gfx::Size(width, view->GetHeightForWidth(width));
    }
  }
}

void BoxLayout::EnsureCrossSize(BoxLayoutData& data) const {
  if (minimum_cross_axis_size_ > data.total_size.cross()) {
    if (cross_axis_alignment_ == CrossAxisAlignment::kCenter) {
      data.cross_center_pos +=
          (minimum_cross_axis_size_ - data.total_size.cross()) / 2;
    }
    data.total_size.set_cross(minimum_cross_axis_size_);
  }
}

void BoxLayout::InitializeChildData(BoxLayoutData& data) const {
  int leading = 0;
  int trailing = 0;

  for (View* child : host_view()->children()) {
    // If we calculate here in View::ChildVisibilityChanged, LayoutManagerBase
    // will not modify the visibility property of the view in time, so we need
    // to explicitly judge here.
    if (!IsChildIncludedInLayout(child, true) || !child->GetVisible()) {
      continue;
    }

    data.child_data.emplace_back();
    BoxChildData& child_data = data.child_data.back();

    data.layout.child_layouts.emplace_back(child, true);

    gfx::Insets* margins = child ? child->GetProperty(kMarginsKey) : nullptr;
    if (margins) {
      child_data.margins = Normalize(orientation_, *margins);
    }

    child_data.flex = GetFlexForView(child);

    leading = std::max(leading, child_data.margins.cross_leading());
    trailing = std::max(trailing, child_data.margins.cross_trailing());
  }

  data.max_cross_margin.set_leading(leading);
  data.max_cross_margin.set_trailing(trailing);
  data.cross_center_pos = 0;
}

SizeBound BoxLayout::CalculateMaxChildWidth(BoxLayoutData& data) const {
  gfx::Rect child_view_area;
  SizeBound width = 0;

  for (size_t i = 0; i < data.num_children(); ++i) {
    ChildLayout& child_layout = data.layout.child_layouts[i];

    gfx::Size child_size =
        child_layout.child_view->GetPreferredSize({/* Unbounded */});
    NormalizedInsets child_margins = GetChildMargins(data, i);

    // The value of |cross_axis_alignment_| will determine how the view's
    // margins interact with each other or the |inside_border_insets_|.
    if (cross_axis_alignment_ == CrossAxisAlignment::kStart) {
      width = std::max<SizeBound>(
          width, child_size.width() + child_margins.cross_trailing());
    } else if (cross_axis_alignment_ == CrossAxisAlignment::kEnd) {
      width = std::max<SizeBound>(
          width, child_size.width() + child_margins.cross_leading());
    } else {
      // We don't have a rectangle which can be used to calculate a common
      // center-point, so a single known point (0) along the horizontal axis
      // is used. This is OK because we're only interested in the overall
      // width and not the position.
      gfx::Rect child_bounds =
          gfx::Rect(-(child_size.width() / 2), 0, child_size.width(),
                    child_size.height());
      child_bounds.Inset(gfx::Insets::TLBR(0, -child_margins.cross_leading(), 0,
                                           -child_margins.cross_trailing()));
      child_view_area.Union(child_bounds);
      width = std::max<SizeBound>(width, child_view_area.width());
    }
  }

  int extra_cross_margin = host_view()->GetInsets().width();
  if (cross_axis_alignment_ == CrossAxisAlignment::kStart) {
    extra_cross_margin = data.max_cross_margin.leading();
  } else if (cross_axis_alignment_ == CrossAxisAlignment::kEnd) {
    extra_cross_margin = data.max_cross_margin.trailing();
  }

  if (!collapse_margins_spacing_) {
    extra_cross_margin += data.interior_margin.cross_size();
  }

  return std::max<SizeBound>(width + extra_cross_margin,
                             minimum_cross_axis_size_);
}

void BoxLayout::CalculatePreferredSize(const SizeBounds& bounds,
                                       BoxLayoutData& data) const {
  if (orientation_ == Orientation::kVertical) {
    for (size_t i = 0; i < data.num_children(); ++i) {
      BoxChildData& box_child = data.child_data[i];
      ChildLayout& child_layout = data.layout.child_layouts[i];
      SizeBound available_width = std::max<SizeBound>(
          0, bounds.width() - box_child.margins.cross_size());

      // Use the child area width for getting the height if the child is
      // supposed to stretch. Use its preferred size otherwise.
      int actual_width =
          cross_axis_alignment_ == CrossAxisAlignment::kStretch
              ? available_width.value()
              : std::min(
                    available_width.value(),
                    child_layout.child_view->GetPreferredSize({/* Unbounded */})
                        .width());

      if (collapse_margins_spacing_) {
        int height = child_layout.child_view->GetHeightForWidth(actual_width);
        box_child.preferred_size = NormalizedSize(height, actual_width);
      } else {
        actual_width = std::max(0, actual_width);
        int height = child_layout.child_view->GetHeightForWidth(actual_width);
        box_child.preferred_size = NormalizedSize(height, actual_width);
      }
    }
  } else {
    for (size_t i = 0; i < data.num_children(); ++i) {
      BoxChildData& box_child = data.child_data[i];
      ChildLayout& child_layout = data.layout.child_layouts[i];

      box_child.preferred_size = Normalize(
          orientation_, child_layout.child_view->GetPreferredSize(bounds));
    }
  }
}

NormalizedInsets BoxLayout::GetChildMargins(BoxLayoutData& data,
                                            size_t index) const {
  BoxChildData& box_child = data.child_data[index];

  if (collapse_margins_spacing_) {
    const size_t num_child = data.num_children();
    NormalizedInsets leading =
        index == 0 ? data.interior_margin : data.child_data[index - 1].margins;
    NormalizedInsets trailing = index == num_child - 1
                                    ? data.interior_margin
                                    : data.child_data[index + 1].margins;

    return NormalizedInsets(
        std::max(box_child.margins.main_leading(), leading.main_trailing()),
        std::max(box_child.margins.cross_leading(),
                 data.interior_margin.cross_leading()),
        std::max(box_child.margins.main_trailing(), trailing.main_leading()),
        std::max(box_child.margins.cross_trailing(),
                 data.interior_margin.cross_trailing()));
  } else {
    return box_child.margins;
  }
}

void BoxLayout::CalculatePreferredTotalSize(BoxLayoutData& data) const {
  for (size_t i = 0; i < data.num_children(); ++i) {
    BoxChildData& box_child = data.child_data[i];

    int main_size = box_child.preferred_size.main();
    if (!collapse_margins_spacing_) {
      main_size += box_child.margins.main_size();
    }

    if (main_size == 0 && box_child.flex == 0) {
      continue;
    }

    NormalizedInsets child_margins = GetChildMargins(data, i);

    if (i < data.num_children() - 1) {
      if (collapse_margins_spacing_) {
        main_size +=
            std::max(between_child_spacing_, child_margins.main_trailing());
      } else {
        main_size += between_child_spacing_;
      }
    }

    int cross_size = box_child.preferred_size.cross();
    if (cross_axis_alignment_ == CrossAxisAlignment::kStart) {
      cross_size +=
          data.max_cross_margin.leading() + child_margins.cross_trailing();
    } else if (cross_axis_alignment_ == CrossAxisAlignment::kEnd) {
      cross_size +=
          data.max_cross_margin.trailing() + child_margins.cross_leading();
    } else {
      // We implement center alignment by moving the central axis.
      int view_center = box_child.preferred_size.cross() / 2;
      int old_cross_center_pos = data.cross_center_pos;
      data.cross_center_pos = std::max(
          data.cross_center_pos, child_margins.cross_leading() + view_center);
      cross_size = data.cross_center_pos + box_child.preferred_size.cross() -
                   view_center + child_margins.cross_trailing();
      // If the new center point has moved to the right relative to the original
      // center point, then we need to move all the views to the right, so the
      // original total size increases by |data.cross_center_pos -
      // old_cross_center_pos|.
      data.total_size.Enlarge(
          0, std::max(0, data.cross_center_pos - old_cross_center_pos));
    }
    data.total_size.SetSize(data.total_size.main() + main_size,
                            std::max(data.total_size.cross(), cross_size));
  }

  EnsureCrossSize(data);
}

void BoxLayout::UpdateFlexLayout(const NormalizedSizeBounds& bounds,
                                 BoxLayoutData& data) const {
  if (bounds.main() == 0 && bounds.cross() == 0) {
    return;
  }

  int total_main_axis_size = data.total_size.main();
  int flex_sum = std::accumulate(
      data.child_data.cbegin(), data.child_data.cend(), 0,
      [](int total, const BoxChildData& data) { return total + data.flex; });

  // `main_free_space` can be negative. The free space is distributed to each
  // view proportionally to their flex value.
  SizeBound main_free_space = bounds.main() - total_main_axis_size;
  int total_padding = 0;
  int current_flex = 0;
  const size_t num_child = data.num_children();
  const int preferred_cross = data.total_size.cross();
  data.total_size = NormalizedSize();
  data.cross_center_pos = 0;

  for (size_t i = 0; i < num_child; ++i) {
    BoxChildData& box_child = data.child_data[i];
    ChildLayout& child_layout = data.layout.child_layouts[i];

    NormalizedInsets child_margins = GetChildMargins(data, i);

    if (!collapse_margins_spacing_) {
      data.total_size.Enlarge(box_child.margins.main_leading(), 0);
    }

    box_child.actual_bounds.set_origin_main(data.total_size.main());
    SizeBound cross_axis_size =
        bounds.cross().is_bounded() && bounds.cross().value() > 0
            ? bounds.cross()
            : preferred_cross;
    if (cross_axis_alignment_ == CrossAxisAlignment::kStretch ||
        cross_axis_alignment_ == CrossAxisAlignment::kCenter) {
      cross_axis_size -= child_margins.cross_size();
    }

    // Calculate flex padding.
    int current_padding = 0;
    int child_flex = box_child.flex;
    if (main_free_space.is_bounded() && child_flex > 0) {
      current_flex += child_flex;
      int quot = (main_free_space.value() * current_flex) / flex_sum;
      int rem = (main_free_space.value() * current_flex) % flex_sum;
      current_padding = quot - total_padding;
      // Use the current remainder to round to the nearest pixel.
      if (std::abs(rem) * 2 >= flex_sum) {
        current_padding += main_free_space > 0 ? 1 : -1;
      }
      total_padding += current_padding;
    }

    // Set main axis size.
    box_child.preferred_size = Normalize(
        orientation_,
        GetPreferredSizeForView(
            child_layout.child_view,
            NormalizedSizeBounds(
                std::max<SizeBound>(0, bounds.main() - data.total_size.main()),
                cross_axis_size)));
    int child_main_axis_size = box_child.preferred_size.main();

    int child_min_size = GetMinimumSizeForView(child_layout.child_view);
    if (child_min_size > 0 && !collapse_margins_spacing_) {
      child_min_size += box_child.margins.main_leading();
    }

    box_child.actual_bounds.set_size_main(
        std::max(child_min_size, child_main_axis_size + current_padding));

    // The cross size could change if its main size is shrunk.
    if (box_child.actual_bounds.size_main() < box_child.preferred_size.main()) {
      // TODO(crbug.com/332745403): we could probably remove this recalculation
      // of preferred_size by providing GetPreferredSizeForView() with the
      // actually allocated main size earlier.
      box_child.preferred_size = Normalize(
          orientation_, GetPreferredSizeForView(
                            child_layout.child_view,
                            NormalizedSizeBounds(
                                SizeBound(box_child.actual_bounds.size_main()),
                                cross_axis_size)));
    }

    if (box_child.actual_bounds.size_main() > 0 || box_child.flex > 0) {
      data.total_size.set_main(box_child.actual_bounds.max_main());
      if (i < num_child - 1) {
        if (collapse_margins_spacing_) {
          data.total_size.Enlarge(
              std::max(between_child_spacing_, child_margins.main_trailing()),
              0);
        } else {
          data.total_size.Enlarge(between_child_spacing_, 0);
        }
      }

      if (!collapse_margins_spacing_) {
        data.total_size.Enlarge(child_margins.main_trailing(), 0);
      }
    } else if (!collapse_margins_spacing_) {
      // TODO(weidongliu): see crbugs.com/1514004#c4. If a view with a 0
      // preferred size has a margin, it will be considered for main_leading but
      // not for main_trailing.
      data.total_size.set_main(data.total_size.main() +
                               child_margins.main_leading());
    }

    int cross_size = box_child.preferred_size.cross();
    if (cross_axis_alignment_ == CrossAxisAlignment::kStart) {
      cross_size +=
          data.max_cross_margin.leading() + child_margins.cross_trailing();
    } else if (cross_axis_alignment_ == CrossAxisAlignment::kEnd) {
      cross_size +=
          data.max_cross_margin.trailing() + child_margins.cross_leading();
    } else {
      int view_center = box_child.preferred_size.cross() / 2;
      // When center aligning, if the size is an odd number, we want the view to
      // be to the left instead of to the right.
      if (cross_axis_alignment_ == CrossAxisAlignment::kCenter) {
        view_center += box_child.preferred_size.cross() & 1;
      }

      int old_cross_center_pos = data.cross_center_pos;
      data.cross_center_pos = std::max(
          data.cross_center_pos, child_margins.cross_leading() + view_center);
      cross_size = data.cross_center_pos + box_child.preferred_size.cross() -
                   view_center + child_margins.cross_trailing();

      // If the new center point has moved to the right relative to the original
      // center point, then we need to move all the views to the right, so the
      // original total size increases by |data.cross_center_pos -
      // old_cross_center_pos|.
      data.total_size.Enlarge(
          0, std::max(0, data.cross_center_pos - old_cross_center_pos));
    }
    data.total_size.set_cross(std::max(data.total_size.cross(), cross_size));
  }

  EnsureCrossSize(data);

  for (size_t i = 0; i < num_child; ++i) {
    BoxChildData& box_child = data.child_data[i];

    NormalizedInsets child_margins = GetChildMargins(data, i);
    SizeBound cross_axis_size =
        bounds.cross().is_bounded() && bounds.cross().value() > 0
            ? bounds.cross()
            : data.total_size.cross();
    cross_axis_size -= child_margins.cross_size();

    if (cross_axis_alignment_ == CrossAxisAlignment::kCenter) {
      box_child.actual_bounds.set_size_cross(
          std::min(cross_axis_size.value(), box_child.preferred_size.cross()));
      int view_center = box_child.actual_bounds.size_cross() / 2 +
                        (box_child.actual_bounds.size_cross() & 1);
      int available_cross = cross_axis_size.value() +
                            child_margins.cross_size() -
                            data.total_size.cross();
      box_child.actual_bounds.set_origin_cross(
          data.cross_center_pos - view_center + available_cross / 2);
    } else {
      Span container(child_margins.cross_leading(), cross_axis_size.value());
      Span new_cross(0, std::min(cross_axis_size.value(),
                                 box_child.preferred_size.cross()));
      new_cross.Align(container, cross_axis_alignment_);
      box_child.actual_bounds.set_origin_cross(new_cross.start());
      box_child.actual_bounds.set_size_cross(new_cross.length());
    }
  }

  // Flex views should have grown/shrunk to consume all free space.
  if (flex_sum && main_free_space.is_bounded()) {
    DCHECK_EQ(total_padding, main_free_space);
  }
}

void BoxLayout::CalculateChildBounds(const SizeBounds& size_bounds,
                                     BoxLayoutData& data) const {
  // Apply main axis alignment (we've already done cross-axis alignment above).
  const NormalizedSizeBounds normalized_bounds =
      Normalize(orientation_, size_bounds);
  const NormalizedSize normalized_host_size =
      Normalize(orientation_, data.layout.host_size);
  int available_main = normalized_bounds.main().is_bounded()
                           ? normalized_bounds.main().value()
                           : normalized_host_size.main();
  available_main = std::max(0, available_main - data.host_insets.main_size());
  const int excess_main = available_main - data.total_size.main();
  NormalizedPoint start(data.host_insets.main_leading(),
                        data.host_insets.cross_leading());

  int flex_sum = std::accumulate(
      data.child_data.cbegin(), data.child_data.cend(), 0,
      [](int total, const BoxChildData& data) { return total + data.flex; });
  // BoxLayoutTest.FlexShrinkHorizontal relies on this behavior, why is
  // alignment needed only when there is no flex?
  if (!flex_sum) {
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
  }

  // Calculate the actual child bounds.
  for (size_t i = 0; i < data.num_children(); ++i) {
    ChildLayout& child_layout = data.layout.child_layouts[i];
    BoxChildData& box_child = data.child_data[i];
    NormalizedRect actual = box_child.actual_bounds;
    actual.Offset(start.main(), start.cross());
    // If the view exceeds the space, truncate the view.
    if (actual.origin_main() < data.host_insets.main_leading()) {
      actual.SetByBounds(data.host_insets.main_leading(), actual.origin_cross(),
                         actual.max_main(), actual.max_cross());
    }

    if (actual.max_main() > data.host_insets.main_leading() + available_main) {
      actual.SetByBounds(actual.origin_main(), actual.origin_cross(),
                         data.host_insets.main_leading() + available_main,
                         actual.max_cross());
    }
    child_layout.bounds = Denormalize(orientation_, actual);
  }
}

}  // namespace views
