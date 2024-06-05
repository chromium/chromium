// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/layout_manager.h"

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/view.h"

namespace views {

LayoutManager::~LayoutManager() = default;

void LayoutManager::Installed(View* host) {}

void LayoutManager::InvalidateLayout() {}

gfx::Size LayoutManager::GetMinimumSize(const View* host) const {
  // Fall back to using preferred size if no minimum size calculation is
  // available (e.g. legacy layout managers).
  //
  // Ideally we'd just call GetPreferredSize() on ourselves here, but because
  // some legacy views with layout managers override GetPreferredSize(), we need
  // to call GetPreferredSize() on the host view instead. The default
  // views::View behavior will be to call GetPreferredSize() on this layout
  // manager, so the fallback behavior in all other cases is as expected.
  return host->GetPreferredSize({});
}

int LayoutManager::GetPreferredHeightForWidth(const View* host,
                                              int width) const {
  return GetPreferredSize(host).height();
}

SizeBounds LayoutManager::GetAvailableSize(const View* host,
                                           const View* view) const {
  return SizeBounds();
}

void LayoutManager::ViewAdded(View* host, View* view) {}

void LayoutManager::ViewRemoved(View* host, View* view) {}

void LayoutManager::ViewVisibilitySet(View* host,
                                      View* view,
                                      bool old_visibility,
                                      bool new_visibility) {
  // Changing the visibility of a child view should force a re-layout. There is
  // more sophisticated logic in LayoutManagerBase but this should be adequate
  // for most legacy layouts (none of which override this method).
  // TODO(dfried): Remove this if/when LayoutManager and LayoutManagerBase can
  // be merged.
  if (old_visibility != new_visibility)
    host->InvalidateLayout();
}

void LayoutManager::SetViewVisibility(View* view, bool visible) {
  DCHECK(!view->parent() || view->parent()->GetLayoutManager() == this ||
         view->parent()->GetLayoutManager() == nullptr);
  base::AutoReset<raw_ptr<View>> setter(&view_setting_visibility_on_, view);
  view->SetVisible(visible);
}

std::vector<raw_ptr<View, VectorExperimental>>
LayoutManager::GetChildViewsInPaintOrder(const View* host) const {
  return host->children();
}

}  // namespace views
