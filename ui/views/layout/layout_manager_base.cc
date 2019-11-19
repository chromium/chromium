// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/layout_manager_base.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "ui/views/view.h"

namespace views {

LayoutManagerBase::~LayoutManagerBase() = default;

gfx::Size LayoutManagerBase::GetPreferredSize(const View* host) const {
  DCHECK_EQ(host_view_, host);
  if (!cached_preferred_size_)
    cached_preferred_size_ = CalculateProposedLayout(SizeBounds()).host_size;
  return *cached_preferred_size_;
}

gfx::Size LayoutManagerBase::GetMinimumSize(const View* host) const {
  DCHECK_EQ(host_view_, host);
  if (!cached_minimum_size_)
    cached_minimum_size_ = CalculateProposedLayout(SizeBounds(0, 0)).host_size;
  return *cached_minimum_size_;
}

int LayoutManagerBase::GetPreferredHeightForWidth(const View* host,
                                                  int width) const {
  if (!cached_height_for_width_ || cached_height_for_width_->width() != width) {
    const int height = CalculateProposedLayout(SizeBounds(width, base::nullopt))
                           .host_size.height();
    cached_height_for_width_ = gfx::Size(width, height);
  }

  return cached_height_for_width_->height();
}

void LayoutManagerBase::Layout(View* host) {
  DCHECK_EQ(host_view_, host);
  // A handful of views will cause invalidations while they are being
  // positioned, which can result in loops or loss of layout data during layout
  // application. Therefore we protect the layout manager from spurious
  // invalidations during the layout process.
  base::AutoReset<bool> setter(&suppress_invalidate_, true);
  LayoutImpl();
}

std::vector<View*> LayoutManagerBase::GetChildViewsInPaintOrder(
    const View* host) const {
  DCHECK_EQ(host_view_, host);
  return LayoutManager::GetChildViewsInPaintOrder(host);
}

ProposedLayout LayoutManagerBase::GetProposedLayout(
    const gfx::Size& host_size) const {
  if (cached_layout_size_ != host_size) {
    cached_layout_size_ = host_size;
    cached_layout_ = CalculateProposedLayout(SizeBounds(host_size));
  }
  return cached_layout_;
}

void LayoutManagerBase::SetChildViewIgnoredByLayout(View* child_view,
                                                    bool ignored) {
  auto it = child_infos_.find(child_view);
  DCHECK(it != child_infos_.end());
  if (it->second.ignored == ignored)
    return;

  base::AutoReset<bool> setter(&suppress_invalidate_, true);
  PropagateChildViewIgnoredByLayout(child_view, ignored);
  InvalidateHost(false);
}

bool LayoutManagerBase::IsChildViewIgnoredByLayout(
    const View* child_view) const {
  auto it = child_infos_.find(child_view);
  DCHECK(it != child_infos_.end());
  return it->second.ignored;
}

LayoutManagerBase::LayoutManagerBase() = default;

bool LayoutManagerBase::IsChildIncludedInLayout(const View* child,
                                                bool include_hidden) const {
  const auto it = child_infos_.find(child);

  // During callbacks when a child is removed we can get in a state where a view
  // in the child list of the host view is not in |child_infos_|. In that case,
  // the view is being removed and is not part of the layout.
  if (it == child_infos_.end())
    return false;

  return !it->second.ignored && (include_hidden || it->second.can_be_visible);
}

void LayoutManagerBase::LayoutImpl() {
  ApplyLayout(GetProposedLayout(host_view_->size()));
}

void LayoutManagerBase::ApplyLayout(const ProposedLayout& layout) {
  for (auto& child_layout : layout.child_layouts) {
    DCHECK_EQ(host_view_, child_layout.child_view->parent());

    // Since we have a non-const reference to the parent here, we can safely use
    // a non-const reference to the child.
    View* const child_view = child_layout.child_view;
    if (child_view->GetVisible() != child_layout.visible)
      SetViewVisibility(child_view, child_layout.visible);
    if (child_layout.visible)
      child_view->SetBoundsRect(child_layout.bounds);
  }
}

void LayoutManagerBase::InvalidateHost(bool mark_layouts_changed) {
  if (mark_layouts_changed) {
    DCHECK(!suppress_invalidate_);
    if (host_view()) {
      // This has the side-effect of also invalidating all of the layouts
      // associated with the host view, from the view's layout through its
      // owned layouts (and their owned layouts).
      host_view()->InvalidateLayout();
    } else {
      // Even without a host view, ensure that the layout tree is invalidated.
      GetRootLayoutManager()->InvalidateLayout();
    }
  } else if (host_view()) {
    // Suppress invalidation at the root layout, so host view layout does not
    // propagate down the layout manager tree.
    LayoutManagerBase* const root_layout = GetRootLayoutManager();
    base::AutoReset<bool> setter(&root_layout->suppress_invalidate_, true);
    host_view()->InvalidateLayout();
  }
}

bool LayoutManagerBase::OnChildViewIgnoredByLayout(View* child_view,
                                                   bool ignored) {
  OnLayoutChanged();
  return false;
}

bool LayoutManagerBase::OnViewAdded(View* host, View* view) {
  OnLayoutChanged();
  return false;
}

bool LayoutManagerBase::OnViewRemoved(View* host, View* view) {
  OnLayoutChanged();
  return false;
}

bool LayoutManagerBase::OnViewVisibilitySet(View* host,
                                            View* view,
                                            bool visible) {
  OnLayoutChanged();
  return false;
}

void LayoutManagerBase::OnInstalled(View* host) {}

void LayoutManagerBase::OnLayoutChanged() {
  cached_minimum_size_.reset();
  cached_preferred_size_.reset();
  cached_height_for_width_.reset();
  cached_layout_size_.reset();
}

void LayoutManagerBase::InvalidateLayout() {
  if (!suppress_invalidate_) {
    base::AutoReset<bool> setter(&suppress_invalidate_, true);
    PropagateInvalidateLayout();
  }
}

void LayoutManagerBase::Installed(View* host_view) {
  DCHECK(host_view);
  DCHECK(!host_view_);
  DCHECK(child_infos_.empty());

  base::AutoReset<bool> setter(&suppress_invalidate_, true);
  PropagateInstalled(host_view);
}

void LayoutManagerBase::ViewAdded(View* host, View* view) {
  DCHECK_EQ(host_view_, host);
  DCHECK(!base::Contains(child_infos_, view));

  base::AutoReset<bool> setter(&suppress_invalidate_, true);
  const bool invalidate = PropagateViewAdded(host, view);
  if (invalidate || view->GetVisible())
    InvalidateHost(false);
}

void LayoutManagerBase::ViewRemoved(View* host, View* view) {
  DCHECK_EQ(host_view_, host);
  DCHECK(base::Contains(child_infos_, view));

  auto it = child_infos_.find(view);
  DCHECK(it != child_infos_.end());
  const bool removed_visible = it->second.can_be_visible && !it->second.ignored;

  base::AutoReset<bool> setter(&suppress_invalidate_, true);
  const bool invalidate = PropagateViewRemoved(host, view);
  if (invalidate || removed_visible)
    InvalidateHost(false);
}

void LayoutManagerBase::ViewVisibilitySet(View* host,
                                          View* view,
                                          bool visible) {
  DCHECK_EQ(host_view_, host);
  auto it = child_infos_.find(view);
  DCHECK(it != child_infos_.end());
  const bool was_ignored = it->second.ignored;
  if (it->second.can_be_visible == visible)
    return;

  base::AutoReset<bool> setter(&suppress_invalidate_, true);
  const bool invalidate = PropagateViewVisibilitySet(host, view, visible);
  if (invalidate || !was_ignored)
    InvalidateHost(false);
}

void LayoutManagerBase::AddOwnedLayoutInternal(
    std::unique_ptr<LayoutManagerBase> owned_layout) {
  DCHECK(owned_layout);
  DCHECK(!owned_layout->parent_layout_);
  if (host_view_) {
    owned_layout->Installed(host_view_);
    for (View* child_view : host_view_->children()) {
      const ChildInfo& child_info = child_infos_.find(child_view)->second;
      owned_layout->PropagateChildViewIgnoredByLayout(child_view,
                                                      child_info.ignored);
      owned_layout->PropagateViewVisibilitySet(host_view_, child_view,
                                               child_info.can_be_visible);
    }
  }
  owned_layout->parent_layout_ = this;
  owned_layouts_.emplace_back(std::move(owned_layout));
}

LayoutManagerBase* LayoutManagerBase::GetRootLayoutManager() {
  LayoutManagerBase* result = this;
  while (result->parent_layout_)
    result = result->parent_layout_;
  return result;
}

bool LayoutManagerBase::PropagateChildViewIgnoredByLayout(View* child_view,
                                                          bool ignored) {
  child_infos_[child_view].ignored = ignored;

  bool result = false;
  for (auto& owned_layout : owned_layouts_) {
    result |=
        owned_layout->PropagateChildViewIgnoredByLayout(child_view, ignored);
  }
  result |= OnChildViewIgnoredByLayout(child_view, ignored);
  return result;
}

bool LayoutManagerBase::PropagateViewAdded(View* host, View* view) {
  child_infos_.emplace(view, ChildInfo{view->GetVisible(), false});

  bool result = false;

  for (auto& owned_layout : owned_layouts_)
    result |= owned_layout->PropagateViewAdded(host, view);

  result |= OnViewAdded(host, view);
  return result;
}

bool LayoutManagerBase::PropagateViewRemoved(View* host, View* view) {
  child_infos_.erase(view);

  bool result = false;

  for (auto& owned_layout : owned_layouts_)
    result |= owned_layout->PropagateViewRemoved(host, view);

  result |= OnViewRemoved(host, view);
  return result;
}

bool LayoutManagerBase::PropagateViewVisibilitySet(View* host,
                                                   View* view,
                                                   bool visible) {
  child_infos_[view].can_be_visible = visible;

  bool result = false;

  for (auto& owned_layout : owned_layouts_)
    result |= owned_layout->PropagateViewVisibilitySet(host, view, visible);

  result |= OnViewVisibilitySet(host, view, visible);
  return result;
}

void LayoutManagerBase::PropagateInstalled(View* host) {
  host_view_ = host;
  for (auto it = host->children().begin(); it != host->children().end(); ++it) {
    child_infos_.emplace(*it, ChildInfo{(*it)->GetVisible(), false});
  }

  for (auto& owned_layout : owned_layouts_)
    owned_layout->PropagateInstalled(host);

  OnInstalled(host);
}

void LayoutManagerBase::PropagateInvalidateLayout() {
  for (auto& owned_layout : owned_layouts_)
    owned_layout->PropagateInvalidateLayout();

  OnLayoutChanged();
}

}  // namespace views
