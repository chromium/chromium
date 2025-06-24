// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_TREE_VIEW_ACCESSIBILITY_AX_TREE_SOURCE_H_
#define UI_VIEWS_ACCESSIBILITY_TREE_VIEW_ACCESSIBILITY_AX_TREE_SOURCE_H_

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_source.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/views_export.h"

namespace ui {
struct AXActionData;
struct AXNodeData;
struct AXTreeData;
}  // namespace ui

namespace views {

class WidgetViewAXCache;

// This class exposes the views hierarchy as an accessibility tree permitting
// use with other accessibility classes. It uses the View's ViewAccessibility
// instance as source.
class VIEWS_EXPORT ViewAccessibilityAXTreeSource
    : public ui::
          AXTreeSource<ViewAccessibility*, ui::AXTreeData*, ui::AXNodeData> {
 public:
  ViewAccessibilityAXTreeSource(ui::AXNodeID root_id,
                                const ui::AXTreeID& tree_id,
                                WidgetViewAXCache* cache);
  ViewAccessibilityAXTreeSource(const ViewAccessibilityAXTreeSource&) = delete;
  ViewAccessibilityAXTreeSource& operator=(
      const ViewAccessibilityAXTreeSource&) = delete;
  ~ViewAccessibilityAXTreeSource() override;

  // Invokes an action on an Aura object.
  void HandleAccessibleAction(const ui::AXActionData& action);

  // AXTreeSource:
  bool GetTreeData(ui::AXTreeData* data) const override;
  ViewAccessibility* GetRoot() const override;
  ViewAccessibility* GetFromId(int32_t id) const override;
  int32_t GetId(ViewAccessibility* node) const override;
  void CacheChildrenIfNeeded(ViewAccessibility*) override;
  size_t GetChildCount(ViewAccessibility* node) const override;
  void ClearChildCache(ViewAccessibility*) override;
  ViewAccessibility* ChildAt(ViewAccessibility* node, size_t) const override;
  ViewAccessibility* GetParent(ViewAccessibility* node) const override;
  bool IsIgnored(ViewAccessibility* node) const override;
  bool IsEqual(ViewAccessibility* node1,
               ViewAccessibility* node2) const override;
  ViewAccessibility* GetNull() const override;
  std::string GetDebugString(ViewAccessibility* node) const override;
  void SerializeNode(ViewAccessibility* node,
                     ui::AXNodeData* out_data) const override;

  // Useful for debugging.
  std::string ToString(views::ViewAccessibility* root, std::string prefix);

  const ui::AXTreeID tree_id() const { return tree_id_; }

 private:
  friend class ViewAccessibilityAXTreeSourceTestApi;

  // The ID of the top-level object to use for the AX tree.
  const ui::AXNodeID root_id_;

  // ID to use for the AXTree.
  const ui::AXTreeID tree_id_;

  raw_ptr<WidgetViewAXCache> cache_;
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_TREE_VIEW_ACCESSIBILITY_AX_TREE_SOURCE_H_
