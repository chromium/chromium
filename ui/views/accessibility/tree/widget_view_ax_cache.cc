// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/tree/widget_view_ax_cache.h"

#include <queue>

#include "ui/views/accessibility/view_accessibility.h"

namespace views {

WidgetViewAXCache::WidgetViewAXCache() = default;
WidgetViewAXCache::~WidgetViewAXCache() = default;

void WidgetViewAXCache::Init(ViewAccessibility& root_view_ax, bool full_tree) {
  CHECK_LE(node_map_.size(), 1u);
  if (!full_tree) {
    CHECK(node_map_.empty());
    Insert(&root_view_ax);
    return;
  }

  std::queue<ViewAccessibility*> q;
  q.push(&root_view_ax);

  while (!q.empty()) {
    ViewAccessibility* node = q.front();
    q.pop();

    Insert(node);
    for (const auto& child : node->GetChildren()) {
      if (ViewAccessibility* c = child.get()) {
        q.push(c);
      }
    }
  }
}

ViewAccessibility* WidgetViewAXCache::Get(ui::AXNodeID id) const {
  auto it = node_map_.find(id);
  return it != node_map_.end() ? it->second : nullptr;
}

void WidgetViewAXCache::Insert(ViewAccessibility* view_ax) {
  CHECK(view_ax);
  node_map_[view_ax->GetUniqueId()] = view_ax;
}

void WidgetViewAXCache::Remove(ui::AXNodeID id) {
  node_map_.erase(id);
}

bool WidgetViewAXCache::HasCachedChildren(ViewAccessibility* view_ax) const {
  CHECK(view_ax);
  return cached_children_.contains(view_ax->GetUniqueId());
}

void WidgetViewAXCache::CacheChildrenIfNeeded(ViewAccessibility* view_ax) {
  CHECK(view_ax);
  if (HasCachedChildren(view_ax)) {
    return;
  }

  const auto& children = view_ax->GetChildren();

  auto [it, inserted] = cached_children_.try_emplace(view_ax->GetUniqueId());
  if (!inserted) {
    return;
  }

  it->second.assign(children.begin(), children.end());
}

void WidgetViewAXCache::RemoveFromChildCache(ViewAccessibility* view_ax) {
  CHECK(view_ax);
  cached_children_.erase(view_ax->GetUniqueId());
}

size_t WidgetViewAXCache::CachedChildCount(ViewAccessibility* view_ax) const {
  CHECK(view_ax);
  const ui::AXNodeID id = view_ax->GetUniqueId();
  auto it = cached_children_.find(id);
  return it == cached_children_.end() ? 0u : it->second.size();
}

ViewAccessibility* WidgetViewAXCache::CachedChildAt(ViewAccessibility* view_ax,
                                                    size_t index) const {
  CHECK(view_ax);
  const ui::AXNodeID id = view_ax->GetUniqueId();
  auto it = cached_children_.find(id);
  if (it == cached_children_.end()) {
    return nullptr;
  }

  const auto& children = it->second;
  return index < children.size() ? children[index] : nullptr;
}

}  // namespace views
