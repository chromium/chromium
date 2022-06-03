// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/fill_layout.h"

#include <algorithm>

namespace views {

FillLayout::FillLayout() = default;

FillLayout::~FillLayout() = default;

FillLayout& FillLayout::SetIncludeHiddenViews(bool include_hidden_views) {
  if (include_hidden_views != include_hidden_views_) {
    include_hidden_views_ = include_hidden_views;
    InvalidateHost(true);
  }
  return *this;
}

FillLayout& FillLayout::SetMinimumSizeEnabled(bool minimum_size_enabled) {
  if (minimum_size_enabled != minimum_size_enabled_) {
    minimum_size_enabled_ = minimum_size_enabled;
    InvalidateHost(true);
  }
  return *this;
}

ProposedLayout FillLayout::CalculateProposedLayout(
    const SizeBounds& size_bounds) const {
  // Because we explicitly override GetPreferredSize and
  // GetPreferredHeightForWidth(), we should always call this method with well-
  // defined bounds.
  DCHECK(size_bounds.is_fully_bounded());

  ProposedLayout layout;
  layout.host_size = host_view()->size();

  const gfx::Rect contents_bounds = host_view()->GetContentsBounds();
  for (View* child : host_view()->children()) {
    if (ShouldIncludeChild(child)) {
      layout.child_layouts.push_back(
          ChildLayout{child, child->GetVisible(), contents_bounds,
                      SizeBounds(contents_bounds.size())});
    }
  }

  return layout;
}

gfx::Size FillLayout::GetPreferredSize(const View* host) const {
  DCHECK_EQ(host_view(), host);

  gfx::Size result;

  bool has_child = false;
  for (const View* child : host->children()) {
    if (ShouldIncludeChild(child)) {
      has_child = true;
      result.SetToMax(child->GetPreferredSize());
    }
  }

  // For backwards compatibility, do not include insets if there are no
  // children.
  if (has_child) {
    const gfx::Insets insets = host->GetInsets();
    result.Enlarge(insets.width(), insets.height());
  }

  return result;
}

gfx::Size FillLayout::GetMinimumSize(const View* host) const {
  DCHECK_EQ(host_view(), host);

  if (!minimum_size_enabled_)
    return host->GetPreferredSize();

  gfx::Size result;

  bool has_child = false;
  for (const View* child : host->children()) {
    if (ShouldIncludeChild(child)) {
      has_child = true;
      result.SetToMax(child->GetMinimumSize());
    }
  }

  // For backwards compatibility, do not include insets if there are no
  // children.
  if (has_child) {
    const gfx::Insets insets = host->GetInsets();
    result.Enlarge(insets.width(), insets.height());
  }

  return result;
}

int FillLayout::GetPreferredHeightForWidth(const View* host, int width) const {
  DCHECK_EQ(host_view(), host);

  const gfx::Insets insets = host->GetInsets();
  width -= insets.width();
  int height = 0;
  for (const View* child : host->children()) {
    if (ShouldIncludeChild(child)) {
      height =
          std::max(height, insets.height() + child->GetHeightForWidth(width));
    }
  }

  return height;
}

bool FillLayout::ShouldIncludeChild(const View* view) const {
  return include_hidden_views_ ? !IsChildViewIgnoredByLayout(view)
                               : IsChildIncludedInLayout(view);
}

}  // namespace views
