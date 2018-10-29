// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/test_ax_node_wrapper.h"

#include "base/containers/hash_tables.h"
#include "base/stl_util.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_table_info.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace ui {

namespace {

// A global map from AXNodes to TestAXNodeWrappers.
base::hash_map<AXNode*, TestAXNodeWrapper*> g_node_to_wrapper_map;

// A global coordinate offset.
gfx::Vector2d g_offset;

// A simple implementation of AXTreeDelegate to catch when AXNodes are
// deleted so we can delete their wrappers.
class TestAXTreeDelegate : public AXTreeDelegate {
  void OnNodeDataWillChange(AXTree* tree,
                            const AXNodeData& old_node_data,
                            const AXNodeData& new_node_data) override {}
  void OnTreeDataChanged(AXTree* tree,
                         const AXTreeData& old_data,
                         const AXTreeData& new_data) override {}
  void OnNodeWillBeDeleted(AXTree* tree, AXNode* node) override {
    auto iter = g_node_to_wrapper_map.find(node);
    if (iter != g_node_to_wrapper_map.end()) {
      TestAXNodeWrapper* wrapper = iter->second;
      delete wrapper;
      g_node_to_wrapper_map.erase(iter->first);
    }
  }
  void OnSubtreeWillBeDeleted(AXTree* tree, AXNode* node) override {}
  void OnNodeWillBeReparented(AXTree* tree, AXNode* node) override {}
  void OnSubtreeWillBeReparented(AXTree* tree, AXNode* node) override {}
  void OnNodeCreated(AXTree* tree, AXNode* node) override {}
  void OnNodeReparented(AXTree* tree, AXNode* node) override {}
  void OnNodeChanged(AXTree* tree, AXNode* node) override {}
  void OnAtomicUpdateFinished(AXTree* tree,
                              bool root_changed,
                              const std::vector<Change>& changes) override {}
};

TestAXTreeDelegate g_ax_tree_delegate;

}  // namespace

// static
TestAXNodeWrapper* TestAXNodeWrapper::GetOrCreate(AXTree* tree, AXNode* node) {
  if (!tree || !node)
    return nullptr;

  tree->SetDelegate(&g_ax_tree_delegate);
  auto iter = g_node_to_wrapper_map.find(node);
  if (iter != g_node_to_wrapper_map.end())
    return iter->second;
  TestAXNodeWrapper* wrapper = new TestAXNodeWrapper(tree, node);
  g_node_to_wrapper_map[node] = wrapper;
  return wrapper;
}

// static
void TestAXNodeWrapper::SetGlobalCoordinateOffset(const gfx::Vector2d& offset) {
  g_offset = offset;
}

TestAXNodeWrapper::~TestAXNodeWrapper() {
  platform_node_->Destroy();
}

const AXNodeData& TestAXNodeWrapper::GetData() const {
  return node_->data();
}

const AXTreeData& TestAXNodeWrapper::GetTreeData() const {
  return tree_->data();
}

gfx::NativeViewAccessible TestAXNodeWrapper::GetParent() {
  TestAXNodeWrapper* parent_wrapper = GetOrCreate(tree_, node_->parent());
  return parent_wrapper ?
      parent_wrapper->ax_platform_node()->GetNativeViewAccessible() :
      nullptr;
}

int TestAXNodeWrapper::GetChildCount() {
  return node_->child_count();
}

gfx::NativeViewAccessible TestAXNodeWrapper::ChildAtIndex(int index) {
  CHECK_GE(index, 0);
  CHECK_LT(index, GetChildCount());
  TestAXNodeWrapper* child_wrapper =
      GetOrCreate(tree_, node_->children()[index]);
  return child_wrapper ?
      child_wrapper->ax_platform_node()->GetNativeViewAccessible() :
      nullptr;
}

gfx::Rect TestAXNodeWrapper::GetClippedScreenBoundsRect() const {
  // We could add clipping here if needed.
  gfx::RectF bounds = GetData().location;
  bounds.Offset(g_offset);
  return gfx::ToEnclosingRect(bounds);
}

gfx::Rect TestAXNodeWrapper::GetUnclippedScreenBoundsRect() const {
  gfx::RectF bounds = GetData().location;
  bounds.Offset(g_offset);
  return gfx::ToEnclosingRect(bounds);
}

TestAXNodeWrapper* TestAXNodeWrapper::HitTestSyncInternal(int x, int y) {
  // Here we find the deepest child whose bounding box contains the given point.
  // The assuptions are that there are no overlapping bounding rects and that
  // all children have smaller bounding rects than their parents.
  if (!GetClippedScreenBoundsRect().Contains(gfx::Rect(x, y)))
    return nullptr;

  for (int i = 0; i < GetChildCount(); i++) {
    TestAXNodeWrapper* child = GetOrCreate(tree_, node_->children()[i]);
    if (!child)
      return nullptr;

    TestAXNodeWrapper* result = child->HitTestSyncInternal(x, y);
    if (result) {
      return result;
    }
  }
  return this;
}

gfx::NativeViewAccessible TestAXNodeWrapper::HitTestSync(int x, int y) {
  TestAXNodeWrapper* wrapper = HitTestSyncInternal(x, y);
  return wrapper ? wrapper->ax_platform_node()->GetNativeViewAccessible()
                 : nullptr;
}

// Walk the AXTree and ensure that all wrappers are created
void TestAXNodeWrapper::BuildAllWrappers(AXTree* tree, AXNode* node) {
  for (int i = 0; i < node->child_count(); i++) {
    auto* child = node->children()[i];
    TestAXNodeWrapper::GetOrCreate(tree, child);

    BuildAllWrappers(tree, child);
  }
}

AXPlatformNode* TestAXNodeWrapper::GetFromNodeID(int32_t id) {
  // Force creating all of the wrappers for this tree.
  BuildAllWrappers(tree_, node_);

  for (auto it = g_node_to_wrapper_map.begin();
       it != g_node_to_wrapper_map.end(); ++it) {
    AXNode* node = it->first;
    if (node->id() == id) {
      TestAXNodeWrapper* wrapper = it->second;
      return wrapper->ax_platform_node();
    }
  }
  return nullptr;
}

int TestAXNodeWrapper::GetIndexInParent() const {
  return node_ ? node_->index_in_parent() : -1;
}

void TestAXNodeWrapper::ReplaceIntAttribute(int32_t node_id,
                                            ax::mojom::IntAttribute attribute,
                                            int32_t value) {
  if (!tree_)
    return;

  AXNode* node = tree_->GetFromId(node_id);
  if (!node)
    return;

  AXNodeData new_data = node->data();
  std::vector<std::pair<ax::mojom::IntAttribute, int32_t>>& attributes =
      new_data.int_attributes;

  base::EraseIf(attributes, [attribute](auto& pair) {
    return pair.first == attribute;
  });

  new_data.AddIntAttribute(attribute, value);
  node->SetData(new_data);
}

int TestAXNodeWrapper::GetTableRowCount() const {
  return node_->GetTableRowCount();
}

int TestAXNodeWrapper::GetTableColCount() const {
  return node_->GetTableColCount();
}

const std::vector<int32_t> TestAXNodeWrapper::GetColHeaderNodeIds() const {
  std::vector<int32_t> header_ids;
  node_->GetTableCellColHeaderNodeIds(&header_ids);
  return header_ids;
}

const std::vector<int32_t> TestAXNodeWrapper::GetColHeaderNodeIds(
    int32_t col_index) const {
  std::vector<int32_t> header_ids;
  node_->GetTableColHeaderNodeIds(col_index, &header_ids);
  return header_ids;
}

const std::vector<int32_t> TestAXNodeWrapper::GetRowHeaderNodeIds() const {
  std::vector<int32_t> header_ids;
  node_->GetTableCellRowHeaderNodeIds(&header_ids);
  return header_ids;
}

const std::vector<int32_t> TestAXNodeWrapper::GetRowHeaderNodeIds(
    int32_t row_index) const {
  std::vector<int32_t> header_ids;
  node_->GetTableRowHeaderNodeIds(row_index, &header_ids);
  return header_ids;
}

int32_t TestAXNodeWrapper::GetCellId(int32_t row_index,
                                     int32_t col_index) const {
  ui::AXNode* cell = node_->GetTableCellFromCoords(row_index, col_index);
  if (cell)
    return cell->id();

  return -1;
}

int32_t TestAXNodeWrapper::GetTableCellIndex() const {
  return node_->GetTableCellIndex();
}

int32_t TestAXNodeWrapper::CellIndexToId(int32_t cell_index) const {
  ui::AXNode* cell = node_->GetTableCellFromIndex(cell_index);
  if (cell)
    return cell->id();
  return -1;
}

bool TestAXNodeWrapper::AccessibilityPerformAction(
    const ui::AXActionData& data) {
  if (data.action == ax::mojom::Action::kScrollToPoint) {
    g_offset = gfx::Vector2d(data.target_point.x(), data.target_point.y());
    return true;
  }

  if (data.action == ax::mojom::Action::kScrollToMakeVisible) {
    auto offset = node_->data().location.OffsetFromOrigin();
    g_offset = gfx::Vector2d(-offset.x(), -offset.y());
    return true;
  }

  if (data.action == ax::mojom::Action::kSetSelection) {
    ReplaceIntAttribute(data.anchor_node_id,
                        ax::mojom::IntAttribute::kTextSelStart,
                        data.anchor_offset);
    ReplaceIntAttribute(data.anchor_node_id,
                        ax::mojom::IntAttribute::kTextSelEnd,
                        data.focus_offset);
    return true;
  }

  return true;
}

bool TestAXNodeWrapper::ShouldIgnoreHoveredStateForTesting() {
  return true;
}

std::set<int32_t> TestAXNodeWrapper::GetReverseRelations(
    ax::mojom::IntAttribute attr,
    int32_t dst_id) {
  return tree_->GetReverseRelations(attr, dst_id);
}

std::set<int32_t> TestAXNodeWrapper::GetReverseRelations(
    ax::mojom::IntListAttribute attr,
    int32_t dst_id) {
  return tree_->GetReverseRelations(attr, dst_id);
}

const ui::AXUniqueId& TestAXNodeWrapper::GetUniqueId() const {
  return unique_id_;
}

TestAXNodeWrapper::TestAXNodeWrapper(AXTree* tree, AXNode* node)
    : tree_(tree),
      node_(node),
      platform_node_(AXPlatformNode::Create(this)) {
}

}  // namespace ui
