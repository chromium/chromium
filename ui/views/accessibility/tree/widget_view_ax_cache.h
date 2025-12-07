// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_TREE_WIDGET_VIEW_AX_CACHE_H_
#define UI_VIEWS_ACCESSIBILITY_TREE_WIDGET_VIEW_AX_CACHE_H_

#include <vector>

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

  // Build the initial map of all ViewAccessibility instances in the tree
  // rooted at `root_view_ax`. This may be called once for the root, and once
  // for the full tree.
  void Init(ViewAccessibility& root_view_ax, bool full_tree = true);

  ViewAccessibility* Get(ui::AXNodeID id) const;
  void Insert(ViewAccessibility* view_ax);
  void Remove(ui::AXNodeID id);

  bool HasCachedChildren(ViewAccessibility* view_ax) const;

  // Takes a snapshot of the children of `view_ax` and caches them.
  // Note: AXTreeSerializer/AXTreeSource require that child lists remain
  // stable during serialization. In Views today this is always true since
  // everything runs on the main thread, but we still take a snapshot of
  // children here to enforce that contract explicitly. This guards us
  // against subtle bugs if serialization ever becomes multi-threaded or
  // the view hierarchy mutates during a pass. The snapshot is temporary
  // and cleared after serialization completes.
  //
  // TODO(https://crbug.com/449023265): This implementation should allow
  // moving accessibility off of the main thread. Double-check that this works
  // as expected.
  void CacheChildrenIfNeeded(ViewAccessibility* view_ax);

  // Removes the cached children of `view_ax`.
  void RemoveFromChildCache(ViewAccessibility* view_ax);

  // Snapshot accessors used by the tree source during a serialization pass.
  size_t CachedChildCount(ViewAccessibility* view_ax) const;
  ViewAccessibility* CachedChildAt(ViewAccessibility* view_ax,
                                   size_t index) const;

 private:
  // Keeps track of the known ViewAccessibility instances by their AXNodeID.
  absl::flat_hash_map<ui::AXNodeID,
                      raw_ptr<ViewAccessibility, VectorExperimental>>
      node_map_;

  // Caches the children of ViewAccessibility instances marked as being cached.
  // This is used to freeze the children of a ViewAccessibility so the
  // serializer can operate on a stable set of children during serialization.
  absl::flat_hash_map<
      ui::AXNodeID,
      std::vector<raw_ptr<ViewAccessibility, VectorExperimental>>>
      cached_children_;
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_TREE_WIDGET_VIEW_AX_CACHE_H_
