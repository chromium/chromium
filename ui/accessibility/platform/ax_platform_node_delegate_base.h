// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_DELEGATE_BASE_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_DELEGATE_BASE_H_

#include "ui/accessibility/platform/ax_platform_node_delegate.h"

namespace ui {

// Base implementation of AXPlatformNodeDelegate where all functions
// return a default value. Useful for classes that want to implement
// AXPlatformNodeDelegate but don't need to override much of its
// behavior.
class AX_EXPORT AXPlatformNodeDelegateBase : public AXPlatformNodeDelegate {
 public:
  AXPlatformNodeDelegateBase() {}
  ~AXPlatformNodeDelegateBase() override {}

  // Get the accessibility data that should be exposed for this node.
  // Virtually all of the information is obtained from this structure
  // (role, state, name, cursor position, etc.) - the rest of this interface
  // is mostly to implement support for walking the accessibility tree.
  const AXNodeData& GetData() const override;

  // Get the accessibility tree data for this node.
  const AXTreeData& GetTreeData() const override;

  // Get the window the node is contained in.
  gfx::NativeWindow GetTopLevelWidget() override;

  // Get the parent of the node, which may be an AXPlatformNode or it may
  // be a native accessible object implemented by another class.
  gfx::NativeViewAccessible GetParent() override;

  // Get the index in parent. Typically this is the AXNode's index_in_parent_.
  int GetIndexInParent() const override;

  // Get the number of children of this node.
  int GetChildCount() override;

  // Get the child of a node given a 0-based index.
  gfx::NativeViewAccessible ChildAtIndex(int index) override;

  // Get the bounds of this node in screen coordinates, applying clipping
  // to all bounding boxes so that the resulting rect is within the window.
  gfx::Rect GetClippedScreenBoundsRect() const override;

  // Get the bounds of this node in screen coordinates without applying
  // any clipping; it may be outside of the window or offscreen.
  gfx::Rect GetUnclippedScreenBoundsRect() const override;

  // Do a *synchronous* hit test of the given location in global screen
  // coordinates, and the node within this node's subtree (inclusive) that's
  // hit, if any.
  //
  // If the result is anything other than this object or NULL, it will be
  // hit tested again recursively - that allows hit testing to work across
  // implementation classes. It's okay to take advantage of this and return
  // only an immediate child and not the deepest descendant.
  //
  // This function is mainly used by accessibility debugging software.
  // Platforms with touch accessibility use a different asynchronous interface.
  gfx::NativeViewAccessible HitTestSync(int x, int y) override;

  // Return the node within this node's subtree (inclusive) that currently
  // has focus.
  gfx::NativeViewAccessible GetFocus() override;

  // Get whether this node is offscreen.
  bool IsOffscreen() const override;

  AXPlatformNode* GetFromNodeID(int32_t id) override;

  // Given a node ID attribute (one where IsNodeIdIntAttribute is true),
  // and a destination node ID, return a set of all source node IDs that
  // have that relationship attribute between them and the destination.
  std::set<int32_t> GetReverseRelations(ax::mojom::IntAttribute attr,
                                        int32_t dst_id) override;

  // Given a node ID list attribute (one where
  // IsNodeIdIntListAttribute is true), and a destination node ID,
  // return a set of all source node IDs that have that relationship
  // attribute between them and the destination.
  std::set<int32_t> GetReverseRelations(ax::mojom::IntListAttribute attr,
                                        int32_t dst_id) override;

  const AXUniqueId& GetUniqueId() const override;

  //
  // Tables. All of these should be called on a node that's a table-like
  // role.
  //

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

  //
  // Events.
  //

  // Return the platform-native GUI object that should be used as a target
  // for accessibility events.
  gfx::AcceleratedWidget GetTargetForNativeAccessibilityEvent() override;

  //
  // Actions.
  //

  // Perform an accessibility action, switching on the ax::mojom::Action
  // provided in |data|.
  bool AccessibilityPerformAction(const AXActionData& data) override;

  //
  // Testing.
  //

  // Accessibility objects can have the "hot tracked" state set when
  // the mouse is hovering over them, but this makes tests flaky because
  // the test behaves differently when the mouse happens to be over an
  // element. The default value should be falses if not in testing mode.
  bool ShouldIgnoreHoveredStateForTesting() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AXPlatformNodeDelegateBase);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_DELEGATE_H_
