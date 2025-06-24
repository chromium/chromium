// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_TREE_WIDGET_VIEW_AX_CACHE_H_
#define UI_VIEWS_ACCESSIBILITY_TREE_WIDGET_VIEW_AX_CACHE_H_

#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/views/views_export.h"

namespace views {

class ViewAccessibility;
class Widget;

// This class owns and manages the accessibility tree for a Widget. It is owned
// by the `widget_` and must never outlive its owner. This is currently under
// construction.
class VIEWS_EXPORT WidgetViewAXCache {
 public:
  WidgetViewAXCache();
  WidgetViewAXCache(const WidgetViewAXCache&) = delete;
  WidgetViewAXCache& operator=(const WidgetViewAXCache&) = delete;
  ~WidgetViewAXCache();

  ViewAccessibility* Get(ui::AXNodeID id) const;
  void Insert(ViewAccessibility* view_ax);
  void Remove(ui::AXNodeID id);

  bool HasCachedChildren(ViewAccessibility* view_ax) const;
  void CacheChildrenIfNeeded(ViewAccessibility* view_ax);
  void RemoveFromChildCache(ViewAccessibility* view_ax);

 private:
  // Keeps track of the known ViewAccessibility instances by their AXNodeID.
  absl::flat_hash_map<ui::AXNodeID,
                      raw_ptr<ViewAccessibility, VectorExperimental>>
      node_map_;

  // Keeps track of which nodes have added their children to the cache.
  absl::flat_hash_set<ui::AXNodeID> nodes_with_cached_children_;
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_TREE_WIDGET_VIEW_AX_CACHE_H_
