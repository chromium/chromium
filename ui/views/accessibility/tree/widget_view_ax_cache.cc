// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/tree/widget_view_ax_cache.h"

#include "ui/views/accessibility/view_accessibility.h"

namespace views {

WidgetViewAXCache::WidgetViewAXCache() = default;
WidgetViewAXCache::~WidgetViewAXCache() = default;

ViewAccessibility* WidgetViewAXCache::Get(ui::AXNodeID id) const {
  auto it = node_map_.find(id);
  return it != node_map_.end() ? it->second : nullptr;
}

void WidgetViewAXCache::Insert(ViewAccessibility* view_ax) {
  node_map_[view_ax->GetUniqueId()] = view_ax;
}

void WidgetViewAXCache::Remove(ui::AXNodeID id) {
  node_map_.erase(id);
  nodes_with_cached_children_.erase(id);
}

bool WidgetViewAXCache::HasCachedChildren(ViewAccessibility* view_ax) const {
  return nodes_with_cached_children_.contains(view_ax->GetUniqueId());
}

void WidgetViewAXCache::CacheChildrenIfNeeded(ViewAccessibility* view_ax) {
  if (HasCachedChildren(view_ax)) {
    return;
  }

  nodes_with_cached_children_.insert(view_ax->GetUniqueId());
  for (auto child : view_ax->GetChildren()) {
    Insert(child);
  }
}

void WidgetViewAXCache::RemoveFromChildCache(ViewAccessibility* view_ax) {
  nodes_with_cached_children_.erase(view_ax->GetUniqueId());
}

}  // namespace views
