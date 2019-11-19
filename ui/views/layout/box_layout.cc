// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/box_layout.h"

#include <algorithm>

#include "ui/gfx/geometry/rect.h"
#include "ui/views/view_class_properties.h"

namespace views {

namespace {

// Returns the maximum of the given insets along the given |axis|.
// NOTE: |axis| is different from |orientation_|; it specifies the actual
// desired axis.
enum Axis { HORIZONTAL_AXIS, VERTICAL_AXIS };

gfx::Insets MaxAxisInsets(Axis axis,
                          const gfx::Insets& leading1,
                          const gfx::Insets& leading2,
                          const gfx::Insets& trailing1,
                          const gfx::Insets& trailing2) {
  if (axis == HORIZONTAL_AXIS) {
    return gfx::Insets(0, std::max(leading1.left(), leading2.left()), 0,
                       std::max(trailing1.right(), trailing2.right()));
  }
  return gfx::Insets(std::max(leading1.top(), leading2.top()), 0,
                     std::max(trailing1.bottom(), trailing2.bottom()), 0);
}

}  // namespace

BoxLayout::ViewWrapper::ViewWrapper() = default;

BoxLayout::ViewWrapper::ViewWrapper(const BoxLayout* layout, View* view)
    : view_(view), layout_(layout) {
  gfx::Insets* margins = view_ ? view_->GetProperty(kMarginsKey) : nullptr;
  if (margins)
    margins_ = *margins;
}

BoxLayout::ViewWrapper::~ViewWrapper() = default;

int BoxLayout::ViewWrapper::GetHeightForWidth(int width) const {
  // When collapse_margins_spacing_ is true, the BoxLayout handles the margin
  // calculations because it has to compare and use only the largest of several
  // adjacent margins or border insets.
  if (layout_->collapse_margins_spacing_)
    return view_->GetHeightForWidth(width);
  // When collapse_margins_spacing_ is false, the view margins are included in
  // the "virtual" size of the view. The view itself is unaware of this, so this
  // information has to be excluded before the call to View::GetHeightForWidth()
  // and added back in to the result.
  // If the orientation_ is kVertical, the cross-axis is the actual view width.
  // This is because the cross-axis margins are always handled by the layout.
  if (layout_->orientation_ == Orientation::kHorizontal) {
    return view_->GetHeightForWidth(std::max(0, width - margins_.width())) +
           margins_.height();
  }
  return view_->GetHeightForWidth(width) + margins_.height();
}

gfx::Size BoxLayout::ViewWrapper::GetPreferredSize() const {
  gfx::Size preferred_size = view_->GetPreferredSize();
  if (!layout_->collapse_margins_spacing_)
    preferred_size.Enlarge(margins_.width(), margins_.height());
  return preferred_size;
}

void BoxLayout::ViewWrapper::SetBoundsRect(const gfx::Rect& bounds) {
  gfx::Rect new_bounds = bounds;
  if (!layout_->collapse_margins_spacing_) {
    if (layout_->orientation_ == Orientation::kHorizontal) {
      new_bounds.set_x(bounds.x() + margins_.left());
      new_bounds.set_width(std::max(0, bounds.width() - margins_.width()));
    } else {
      new_bounds.set_y(bounds.y() + margins_.top());
      new_bounds.set_height(std::max(0, bounds.height() - margins_.height()));
    }
  }
  view_->SetBoundsRect(new_bounds);
}

bool BoxLayout::ViewWrapper::visible() const {
  return view_->GetVisible();
}

BoxLayout::BoxLayout(BoxLayout::Orientation orientation,
                     const gfx::Insets& inside_border_insets,
                     int between_child_spacing,
                     bool collapse_margins_spacing)
    : orientation_(orientation),
      inside_border_insets_(inside_border_insets),
      between_child_spacing_(between_child_spacing),
      collapse_margins_spacing_(collapse_margins_spacing) {}

BoxLayout::~BoxLayout() = default;

void BoxLayout::SetFlexForView(const View* view,
                               int flex_weight,
                               bool use_min_size) {
  DCHECK(host_);
  DCHECK(view);
  DCHECK_EQ(host_, view->parent());
  DCHECK_GE(flex_weight, 0);
  flex_map_[view].flex_weight = flex_weight;
  flex_map_[view].use_min_size = use_min_size;
}

void BoxLayout::ClearFlexForView(const View* view) {
  DCHECK(view);
  flex_map_.erase(view);
}

void BoxLayout::SetDefaultFlex(int default_flex) {
  DCHECK_GE(default_flex, 0);
  default_flex_ = default_flex;
}

void BoxLayout::Layout(View* host) {
  DCHECK_EQ(host_, host);
  gfx::Rect child_area(host->GetContentsBounds());

  AdjustMainAxisForMargin(&child_area);
  gfx::Insets max_cross_axis_margin;
  if (!collapse_margins_spacing_) {
    AdjustCrossAxisForInsets(&child_area);
    max_cross_axis_margin = CrossAxisMaxViewMargin();
  }
  if (child_area.IsEmpty())
    return;

  int total_main_axis_size = 0;
  int num_visible = 0;
  int flex_sum = 0;
  // Calculate the total size of children in the main axis.
  for (auto i = host->children().cbegin(); i != host->children().cend(); ++i) {
    const ViewWrapper child(this, *i);
    if (!child.visible())
      continue;
    int flex = GetFlexForView(child.view());
    int child_main_axis_size = MainAxisSizeForView(child, child_area.width());
    if (child_main_axis_size == 0 && flex == 0)
      continue;
    total_main_axis_size +=
        child_main_axis_size +
        MainAxisMarginBetweenViews(
            child, ViewWrapper(this, NextVisibleView(std::next(i))));
    ++num_visible;
    flex_sum += flex;
  }

  if (!num_visible)
    return;

  total_main_axis_size -= between_child_spacing_;
  // Free space can be negative indicating that the views want to overflow.
  int main_free_space = MainAxisSize(child_area) - total_main_axis_size;
  int main_position = MainAxisPosition(child_area);
  {
    int size = MainAxisSize(child_area);
    if (!flex_sum) {
      switch (main_axis_alignment_) {
        case MainAxisAlignment::kStart:
          break;
        case MainAxisAlignment::kCenter:
          main_position += main_free_space / 2;
          size = total_main_axis_size;
          break;
        case MainAxisAlignment::kEnd:
          main_position += main_free_space;
          size = total_main_axis_size;
          break;
        default:
          NOTREACHED();
          break;
      }
    }
    gfx::Rect new_child_area(child_area);
    SetMainAxisPosition(main_position, &new_child_area);
    SetMainAxisSize(size, &new_child_area);
    child_area.Intersect(new_child_area);
  }

  int total_padding = 0;
  int current_flex = 0;
  for (auto i = host->children().cbegin(); i != host->children().cend(); ++i) {
    ViewWrapper child(this, *i);
    if (!child.visible())
      continue;

    // TODO(bruthig): Fix this. The main axis should be calculated before
    // the cross axis size because child Views may calculate their cross axis
    // size based on their main axis size. See https://crbug.com/682266.

    // Calculate cross axis size.
    gfx::Rect bounds(child_area);
    gfx::Rect min_child_area(child_area);
    gfx::Insets child_margins;
    if (collapse_margins_spacing_) {
      child_margins =
          MaxAxisInsets(orientation_ == Orientation::kVertical ? HORIZONTAL_AXIS
                                                               : VERTICAL_AXIS,
                        child.margins(), inside_border_insets_, child.margins(),
                        inside_border_insets_);
    } else {
      child_margins = child.margins();
    }

    if (cross_axis_alignment_ == CrossAxisAlignment::kStretch ||
        cross_axis_alignment_ == CrossAxisAlignment::kCenter) {
      InsetCrossAxis(&min_child_area, CrossAxisLeadingInset(child_margins),
                     CrossAxisTrailingInset(child_margins));
    }

    SetMainAxisPosition(main_position, &bounds);
    if (cross_axis_alignment_ != CrossAxisAlignment::kStretch) {
      int cross_axis_margin_size = CrossAxisMarginSizeForView(child);
      int view_cross_axis_size =
          CrossAxisSizeForView(child) - cross_axis_margin_size;
      int free_space = CrossAxisSize(bounds) - view_cross_axis_size;
      int position = CrossAxisPosition(bounds);
      if (cross_axis_alignment_ == CrossAxisAlignment::kCenter) {
        if (view_cross_axis_size > CrossAxisSize(min_child_area))
          view_cross_axis_size = CrossAxisSize(min_child_area);
        position += free_space / 2;
        position = std::max(position, CrossAxisLeadingEdge(min_child_area));
      } else if (cross_axis_alignment_ == CrossAxisAlignment::kEnd) {
        position += free_space - CrossAxisTrailingInset(max_cross_axis_margin);
        if (!collapse_margins_spacing_)
          InsetCrossAxis(&min_child_area,
                         CrossAxisLeadingInset(child.margins()),
                         CrossAxisTrailingInset(max_cross_axis_margin));
      } else {
        position += CrossAxisLeadingInset(max_cross_axis_margin);
        if (!collapse_margins_spacing_)
          InsetCrossAxis(&min_child_area,
                         CrossAxisLeadingInset(max_cross_axis_margin),
                         CrossAxisTrailingInset(child.margins()));
      }
      SetCrossAxisPosition(position, &bounds);
      SetCrossAxisSize(view_cross_axis_size, &bounds);
    }

    // Calculate flex padding.
    int current_padding = 0;
    int child_flex = GetFlexForView(child.view());
    if (child_flex > 0) {
      current_flex += child_flex;
      int quot = (main_free_space * current_flex) / flex_sum;
      int rem = (main_free_space * current_flex) % flex_sum;
      current_padding = quot - total_padding;
      // Use the current remainder to round to the nearest pixel.
      if (std::abs(rem) * 2 >= flex_sum)
        current_padding += main_free_space > 0 ? 1 : -1;
      total_padding += current_padding;
    }

    // Set main axis size.
    // TODO(bruthig): Use the allocated width to determine the cross axis size.
    // See https://crbug.com/682266.
    int child_main_axis_size = MainAxisSizeForView(child, child_area.width());
    int child_min_size = GetMinimumSizeForView(child.view());
    if (child_min_size > 0 && !collapse_margins_spacing_)
      child_min_size += child.margins().width();
    SetMainAxisSize(
        std::max(child_min_size, child_main_axis_size + current_padding),
        &bounds);
    if (MainAxisSize(bounds) > 0 || GetFlexForView(child.view()) > 0) {
      main_position +=
          MainAxisSize(bounds) +
          MainAxisMarginBetweenViews(
              child, ViewWrapper(this, NextVisibleView(std::next(i))));
    }

    // Clamp child view bounds to |child_area|.
    bounds.Intersect(min_child_area);
    child.SetBoundsRect(bounds);
  }

  // Flex views should have grown/shrunk to consume all free space.
  if (flex_sum)
    DCHECK_EQ(total_padding, main_free_space);
}

gfx::Size BoxLayout::GetPreferredSize(const View* host) const {
  DCHECK_EQ(host_, host);
  // Calculate the child views' preferred width.
  int width = 0;
  if (orientation_ == Orientation::kVertical) {
    // Calculating the child views' overall preferred width is a little involved
    // because of the way the margins interact with |cross_axis_alignment_|.
    int leading = 0;
    int trailing = 0;
    gfx::Rect child_view_area;
    for (View* view : host_->children()) {
      const ViewWrapper child(this, view);
      if (!child.visible())
        continue;

      // We need to bypass the ViewWrapper GetPreferredSize() to get the actual
      // raw view size because the margins along the cross axis are handled
      // below.
      gfx::Size child_size = child.view()->GetPreferredSize();
      gfx::Insets child_margins;
      if (collapse_margins_spacing_) {
        child_margins = MaxAxisInsets(HORIZONTAL_AXIS, child.margins(),
                                      inside_border_insets_, child.margins(),
                                      inside_border_insets_);
      } else {
        child_margins = child.margins();
      }

      // The value of |cross_axis_alignment_| will determine how the view's
      // margins interact with each other or the |inside_border_insets_|.
      if (cross_axis_alignment_ == CrossAxisAlignment::kStart) {
        leading = std::max(leading, CrossAxisLeadingInset(child_margins));
        width = std::max(
            width, child_size.width() + CrossAxisTrailingInset(child_margins));
      } else if (cross_axis_alignment_ == CrossAxisAlignment::kEnd) {
        trailing = std::max(trailing, CrossAxisTrailingInset(child_margins));
        width = std::max(
            width, child_size.width() + CrossAxisLeadingInset(child_margins));
      } else {
        // We don't have a rectangle which can be used to calculate a common
        // center-point, so a single known point (0) along the horizontal axis
        // is used. This is OK because we're only interested in the overall
        // width and not the position.
        gfx::Rect child_bounds =
            gfx::Rect(-(child_size.width() / 2), 0, child_size.width(),
                      child_size.height());
        child_bounds.Inset(-child.margins().left(), 0, -child.margins().right(),
                           0);
        child_view_area.Union(child_bounds);
        width = std::max(width, child_view_area.width());
      }
    }
    width = std::max(width + leading + trailing, minimum_cross_axis_size_);
  }

  return GetPreferredSizeForChildWidth(host, width);
}

int BoxLayout::GetPreferredHeightForWidth(const View* host, int width) const {
  DCHECK_EQ(host_, host);
  int child_width = width - NonChildSize(host).width();
  return GetPreferredSizeForChildWidth(host, child_width).height();
}

void BoxLayout::Installed(View* host) {
  DCHECK(!host_);
  host_ = host;
}

void BoxLayout::ViewRemoved(View* host, View* view) {
  ClearFlexForView(view);
}

int BoxLayout::GetFlexForView(const View* view) const {
  auto it = flex_map_.find(view);
  if (it == flex_map_.end())
    return default_flex_;

  return it->second.flex_weight;
}

int BoxLayout::GetMinimumSizeForView(const View* view) const {
  auto it = flex_map_.find(view);
  if (it == flex_map_.end() || !it->second.use_min_size)
    return 0;

  return (orientation_ == Orientation::kHorizontal)
             ? view->GetMinimumSize().width()
             : view->GetMinimumSize().height();
}

int BoxLayout::MainAxisSize(const gfx::Rect& rect) const {
  return orientation_ == Orientation::kHorizontal ? rect.width()
                                                  : rect.height();
}

int BoxLayout::MainAxisPosition(const gfx::Rect& rect) const {
  return orientation_ == Orientation::kHorizontal ? rect.x() : rect.y();
}

void BoxLayout::SetMainAxisSize(int size, gfx::Rect* rect) const {
  if (orientation_ == Orientation::kHorizontal)
    rect->set_width(size);
  else
    rect->set_height(size);
}

void BoxLayout::SetMainAxisPosition(int position, gfx::Rect* rect) const {
  if (orientation_ == Orientation::kHorizontal)
    rect->set_x(position);
  else
    rect->set_y(position);
}

int BoxLayout::CrossAxisSize(const gfx::Rect& rect) const {
  return orientation_ == Orientation::kVertical ? rect.width() : rect.height();
}

int BoxLayout::CrossAxisPosition(const gfx::Rect& rect) const {
  return orientation_ == Orientation::kVertical ? rect.x() : rect.y();
}

void BoxLayout::SetCrossAxisSize(int size, gfx::Rect* rect) const {
  if (orientation_ == Orientation::kVertical)
    rect->set_width(size);
  else
    rect->set_height(size);
}

void BoxLayout::SetCrossAxisPosition(int position, gfx::Rect* rect) const {
  if (orientation_ == Orientation::kVertical)
    rect->set_x(position);
  else
    rect->set_y(position);
}

int BoxLayout::MainAxisSizeForView(const ViewWrapper& view,
                                   int child_area_width) const {
  if (orientation_ == Orientation::kHorizontal) {
    return view.GetPreferredSize().width();
  } else {
    // To calculate the height we use the preferred width of the child
    // unless we're asked to stretch or the preferred width exceeds the
    // available width.
    return view.GetHeightForWidth(
        cross_axis_alignment_ == CrossAxisAlignment::kStretch
            ? child_area_width
            : std::min(child_area_width, view.GetPreferredSize().width()));
  }
}

int BoxLayout::MainAxisLeadingInset(const gfx::Insets& insets) const {
  return orientation_ == Orientation::kHorizontal ? insets.left()
                                                  : insets.top();
}

int BoxLayout::MainAxisTrailingInset(const gfx::Insets& insets) const {
  return orientation_ == Orientation::kHorizontal ? insets.right()
                                                  : insets.bottom();
}

int BoxLayout::CrossAxisLeadingEdge(const gfx::Rect& rect) const {
  return orientation_ == Orientation::kVertical ? rect.x() : rect.y();
}

int BoxLayout::CrossAxisLeadingInset(const gfx::Insets& insets) const {
  return orientation_ == Orientation::kVertical ? insets.left() : insets.top();
}

int BoxLayout::CrossAxisTrailingInset(const gfx::Insets& insets) const {
  return orientation_ == Orientation::kVertical ? insets.right()
                                                : insets.bottom();
}

int BoxLayout::MainAxisMarginBetweenViews(const ViewWrapper& leading,
                                          const ViewWrapper& trailing) const {
  if (!collapse_margins_spacing_ || !leading.view() || !trailing.view())
    return between_child_spacing_;
  return std::max(between_child_spacing_,
                  std::max(MainAxisTrailingInset(leading.margins()),
                           MainAxisLeadingInset(trailing.margins())));
}

gfx::Insets BoxLayout::MainAxisOuterMargin() const {
  if (collapse_margins_spacing_) {
    const ViewWrapper first(this, FirstVisibleView());
    const ViewWrapper last(this, LastVisibleView());
    return MaxAxisInsets(orientation_ == Orientation::kHorizontal
                             ? HORIZONTAL_AXIS
                             : VERTICAL_AXIS,
                         inside_border_insets_, first.margins(),
                         inside_border_insets_, last.margins());
  }
  return MaxAxisInsets(orientation_ == Orientation::kHorizontal
                           ? HORIZONTAL_AXIS
                           : VERTICAL_AXIS,
                       inside_border_insets_, gfx::Insets(),
                       inside_border_insets_, gfx::Insets());
}

gfx::Insets BoxLayout::CrossAxisMaxViewMargin() const {
  int leading = 0;
  int trailing = 0;
  for (View* view : host_->children()) {
    const ViewWrapper child(this, view);
    if (!child.visible())
      continue;
    leading = std::max(leading, CrossAxisLeadingInset(child.margins()));
    trailing = std::max(trailing, CrossAxisTrailingInset(child.margins()));
  }
  if (orientation_ == Orientation::kVertical)
    return gfx::Insets(0, leading, 0, trailing);
  return gfx::Insets(leading, 0, trailing, 0);
}

void BoxLayout::AdjustMainAxisForMargin(gfx::Rect* rect) const {
  rect->Inset(MainAxisOuterMargin());
}

void BoxLayout::AdjustCrossAxisForInsets(gfx::Rect* rect) const {
  rect->Inset(orientation_ == Orientation::kVertical
                  ? gfx::Insets(0, inside_border_insets_.left(), 0,
                                inside_border_insets_.right())
                  : gfx::Insets(inside_border_insets_.top(), 0,
                                inside_border_insets_.bottom(), 0));
}

int BoxLayout::CrossAxisSizeForView(const ViewWrapper& view) const {
  // TODO(bruthig): For horizontal case use the available width and not the
  // preferred width. See https://crbug.com/682266.
  return orientation_ == Orientation::kVertical
             ? view.GetPreferredSize().width()
             : view.GetHeightForWidth(view.GetPreferredSize().width());
}

int BoxLayout::CrossAxisMarginSizeForView(const ViewWrapper& view) const {
  return collapse_margins_spacing_ ? 0
                                   : (orientation_ == Orientation::kVertical
                                          ? view.margins().width()
                                          : view.margins().height());
}

int BoxLayout::CrossAxisLeadingMarginForView(const ViewWrapper& view) const {
  return collapse_margins_spacing_ ? 0 : CrossAxisLeadingInset(view.margins());
}

void BoxLayout::InsetCrossAxis(gfx::Rect* rect,
                               int leading,
                               int trailing) const {
  if (orientation_ == Orientation::kVertical)
    rect->Inset(leading, 0, trailing, 0);
  else
    rect->Inset(0, leading, 0, trailing);
}

gfx::Size BoxLayout::GetPreferredSizeForChildWidth(const View* host,
                                                   int child_area_width) const {
  DCHECK_EQ(host, host_);
  gfx::Rect child_area_bounds;

  if (orientation_ == Orientation::kHorizontal) {
    // Horizontal layouts ignore |child_area_width|, meaning they mimic the
    // default behavior of GridLayout::GetPreferredHeightForWidth().
    // TODO(estade|bruthig): Fix this See // https://crbug.com/682266.
    int position = 0;
    gfx::Insets max_margins = CrossAxisMaxViewMargin();
    for (auto i = host->children().cbegin(); i != host->children().cend();
         ++i) {
      const ViewWrapper child(this, *i);
      if (!child.visible())
        continue;

      gfx::Size size(child.view()->GetPreferredSize());
      if (size.IsEmpty())
        continue;

      gfx::Rect child_bounds = gfx::Rect(
          position, 0,
          size.width() +
              (!collapse_margins_spacing_ ? child.margins().width() : 0),
          size.height());
      gfx::Insets child_margins;
      if (collapse_margins_spacing_)
        child_margins =
            MaxAxisInsets(VERTICAL_AXIS, child.margins(), inside_border_insets_,
                          child.margins(), inside_border_insets_);
      else
        child_margins = child.margins();

      if (cross_axis_alignment_ == CrossAxisAlignment::kStart) {
        child_bounds.Inset(0, -CrossAxisLeadingInset(max_margins), 0,
                           -child_margins.bottom());
        child_bounds.set_origin(gfx::Point(position, 0));
      } else if (cross_axis_alignment_ == CrossAxisAlignment::kEnd) {
        child_bounds.Inset(0, -child_margins.top(), 0,
                           -CrossAxisTrailingInset(max_margins));
        child_bounds.set_origin(gfx::Point(position, 0));
      } else {
        child_bounds.set_origin(
            gfx::Point(position, -(child_bounds.height() / 2)));
        child_bounds.Inset(0, -child_margins.top(), 0, -child_margins.bottom());
      }

      child_area_bounds.Union(child_bounds);
      position += child_bounds.width() +
                  MainAxisMarginBetweenViews(
                      child, ViewWrapper(this, NextVisibleView(std::next(i))));
    }
    child_area_bounds.set_height(
        std::max(child_area_bounds.height(), minimum_cross_axis_size_));
  } else {
    int height = 0;
    for (auto i = host->children().cbegin(); i != host->children().cend();
         ++i) {
      const ViewWrapper child(this, *i);
      if (!child.visible())
        continue;

      const ViewWrapper next(this, NextVisibleView(std::next(i)));
      // Use the child area width for getting the height if the child is
      // supposed to stretch. Use its preferred size otherwise.
      int extra_height = MainAxisSizeForView(child, child_area_width);
      // Only add |between_child_spacing_| if this is not the only child.
      if (next.view() && extra_height > 0)
        height += MainAxisMarginBetweenViews(child, next);
      height += extra_height;
    }

    child_area_bounds.set_width(child_area_width);
    child_area_bounds.set_height(height);
  }

  gfx::Size non_child_size = NonChildSize(host_);
  return gfx::Size(child_area_bounds.width() + non_child_size.width(),
                   child_area_bounds.height() + non_child_size.height());
}

gfx::Size BoxLayout::NonChildSize(const View* host) const {
  gfx::Insets insets(host->GetInsets());
  if (!collapse_margins_spacing_)
    return gfx::Size(insets.width() + inside_border_insets_.width(),
                     insets.height() + inside_border_insets_.height());
  gfx::Insets main_axis = MainAxisOuterMargin();
  gfx::Insets cross_axis = inside_border_insets_;
  return gfx::Size(insets.width() + main_axis.width() + cross_axis.width(),
                   insets.height() + main_axis.height() + cross_axis.height());
}

View* BoxLayout::NextVisibleView(View::Views::const_iterator pos) const {
  const auto i = std::find_if(pos, host_->children().cend(),
                              [](const View* v) { return v->GetVisible(); });
  return (i == host_->children().cend()) ? nullptr : *i;
}

View* BoxLayout::FirstVisibleView() const {
  return NextVisibleView(host_->children().cbegin());
}

View* BoxLayout::LastVisibleView() const {
  const auto& children = host_->children();
  const auto i = std::find_if(children.crbegin(), children.crend(),
                              [](const View* v) { return v->GetVisible(); });
  return (i == children.crend()) ? nullptr : *i;
}

}  // namespace views
