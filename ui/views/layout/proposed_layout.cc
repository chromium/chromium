// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/proposed_layout.h"

#include <map>

#include "ui/gfx/animation/tween.h"

namespace views {

bool ChildLayout::operator==(const ChildLayout& other) const {
  // Note: if the view is not visible, the bounds do not matter as they will not
  // be set.
  return child_view == other.child_view && visible == other.visible &&
         (!visible || bounds == other.bounds);
}

ProposedLayout::ProposedLayout() = default;
ProposedLayout::ProposedLayout(const ProposedLayout& other) = default;
ProposedLayout::ProposedLayout(ProposedLayout&& other) = default;
ProposedLayout::ProposedLayout(
    const gfx::Size& size,
    const std::initializer_list<ChildLayout>& children)
    : host_size(size), child_layouts(children) {}
ProposedLayout::~ProposedLayout() = default;
ProposedLayout& ProposedLayout::operator=(const ProposedLayout& other) =
    default;
ProposedLayout& ProposedLayout::operator=(ProposedLayout&& other) = default;

bool ProposedLayout::operator==(const ProposedLayout& other) const {
  return host_size == other.host_size && child_layouts == other.child_layouts;
}

ProposedLayout ProposedLayoutBetween(double value,
                                     const ProposedLayout& start,
                                     const ProposedLayout& target) {
  if (value >= 1.0)
    return target;

  ProposedLayout layout;

  // Interpolate the host size.
  layout.host_size =
      gfx::Tween::SizeValueBetween(value, start.host_size, target.host_size);

  // The views may not be listed in the same order and some views might be
  // omitted from either the |start| or |target| layout.
  std::map<const views::View*, size_t> start_view_to_index;
  for (size_t i = 0; i < start.child_layouts.size(); ++i)
    start_view_to_index.emplace(start.child_layouts[i].child_view, i);
  for (const ChildLayout& target_child : target.child_layouts) {
    // Try to match the view from the target with the view from the start.
    const auto start_match = start_view_to_index.find(target_child.child_view);
    if (start_match == start_view_to_index.end()) {
      // If there is no match, make the view present but invisible.
      layout.child_layouts.push_back({target_child.child_view, false});
    } else {
      // Tween the two layouts.
      const ChildLayout& start_child = start.child_layouts[start_match->second];
      layout.child_layouts.push_back(
          {target_child.child_view, start_child.visible && target_child.visible,
           gfx::Tween::RectValueBetween(value, start_child.bounds,
                                        target_child.bounds)});
    }
  }
  return layout;
}

}  // namespace views
