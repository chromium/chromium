// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_TEST_AX_NODE_WRAPPER_H_
#define UI_ACCESSIBILITY_PLATFORM_TEST_AX_NODE_WRAPPER_H_

#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_delegate_base.h"

namespace ui {

// For testing, a TestAXNodeWrapper wraps an AXNode, implements
// AXPlatformNodeDelegate, and owns an AXPlatformNode.
class TestAXNodeWrapper : public AXPlatformNodeDelegateBase {
 public:
  // Create TestAXNodeWrapper instances on-demand from an AXTree and AXNode.
  // Note that this sets the AXTreeDelegate, you can't use this class if
  // you also want to implement AXTreeDelegate.
  static TestAXNodeWrapper* GetOrCreate(AXTree* tree, AXNode* node);

  // Set a global coordinate offset for testing.
  static void SetGlobalCoordinateOffset(const gfx::Vector2d& offset);

  ~TestAXNodeWrapper() override;

  AXPlatformNode* ax_platform_node() { return platform_node_; }

  void BuildAllWrappers(AXTree* tree, AXNode* node);

  // AXPlatformNodeDelegate.
  const AXNodeData& GetData() const override;
  const AXTreeData& GetTreeData() const override;
  gfx::NativeViewAccessible GetParent() override;
  int GetChildCount() override;
  gfx::NativeViewAccessible ChildAtIndex(int index) override;
  gfx::Rect GetClippedScreenBoundsRect() const override;
  gfx::Rect GetUnclippedScreenBoundsRect() const override;
  gfx::NativeViewAccessible HitTestSync(int x, int y) override;
  AXPlatformNode* GetFromNodeID(int32_t id) override;
  int GetIndexInParent() const override;
  int GetTableRowCount() const override;
  int GetTableColCount() const override;
  const std::vector<int32_t> GetColHeaderNodeIds() const override;
  const std::vector<int32_t> GetColHeaderNodeIds(
      int32_t col_index) const override;
  const std::vector<int32_t> GetRowHeaderNodeIds() const override;
  const std::vector<int32_t> GetRowHeaderNodeIds(
      int32_t row_index) const override;
  int32_t GetCellId(int32_t row_index, int32_t col_index) const override;
  int32_t GetTableCellIndex() const override;
  int32_t CellIndexToId(int32_t cell_index) const override;
  bool AccessibilityPerformAction(const AXActionData& data) override;
  bool ShouldIgnoreHoveredStateForTesting() override;
  const ui::AXUniqueId& GetUniqueId() const override;
  std::set<int32_t> GetReverseRelations(ax::mojom::IntAttribute attr,
                                        int32_t dst_id) override;
  std::set<int32_t> GetReverseRelations(ax::mojom::IntListAttribute attr,
                                        int32_t dst_id) override;

 private:
  TestAXNodeWrapper(AXTree* tree, AXNode* node);
  void ReplaceIntAttribute(int32_t node_id,
                           ax::mojom::IntAttribute attribute,
                           int32_t value);

  TestAXNodeWrapper* HitTestSyncInternal(int x, int y);

  AXTree* tree_;
  AXNode* node_;
  ui::AXUniqueId unique_id_;
  AXPlatformNode* platform_node_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_TEST_AX_NODE_WRAPPER_H_
