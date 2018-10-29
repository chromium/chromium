// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/gfx/transform.h"

namespace ui {

namespace {

std::string IntVectorToString(const std::vector<int>& items) {
  std::string str;
  for (size_t i = 0; i < items.size(); ++i) {
    if (i > 0)
      str += ",";
    str += base::NumberToString(items[i]);
  }
  return str;
}

std::string GetBoundsAsString(const AXTree& tree, int32_t id) {
  AXNode* node = tree.GetFromId(id);
  gfx::RectF bounds = tree.GetTreeBounds(node);
  return base::StringPrintf("(%.0f, %.0f) size (%.0f x %.0f)", bounds.x(),
                            bounds.y(), bounds.width(), bounds.height());
}

std::string GetUnclippedBoundsAsString(const AXTree& tree, int32_t id) {
  AXNode* node = tree.GetFromId(id);
  gfx::RectF bounds = tree.GetTreeBounds(node, nullptr, false);
  return base::StringPrintf("(%.0f, %.0f) size (%.0f x %.0f)", bounds.x(),
                            bounds.y(), bounds.width(), bounds.height());
}

bool IsNodeOffscreen(const AXTree& tree, int32_t id) {
  AXNode* node = tree.GetFromId(id);
  bool result = false;
  tree.GetTreeBounds(node, &result);
  return result;
}

class FakeAXTreeDelegate : public AXTreeDelegate {
 public:
  FakeAXTreeDelegate()
      : tree_data_changed_(false),
        root_changed_(false) {}

  void OnNodeDataWillChange(AXTree* tree,
                            const AXNodeData& old_node_data,
                            const AXNodeData& new_node_data) override {}
  void OnTreeDataChanged(AXTree* tree,
                         const ui::AXTreeData& old_data,
                         const ui::AXTreeData& new_data) override {
    tree_data_changed_ = true;
  }
  void OnNodeWillBeDeleted(AXTree* tree, AXNode* node) override {
    deleted_ids_.push_back(node->id());
  }

  void OnSubtreeWillBeDeleted(AXTree* tree, AXNode* node) override {
    subtree_deleted_ids_.push_back(node->id());
  }

  void OnNodeWillBeReparented(AXTree* tree, AXNode* node) override {}

  void OnSubtreeWillBeReparented(AXTree* tree, AXNode* node) override {}

  void OnNodeCreated(AXTree* tree, AXNode* node) override {
    created_ids_.push_back(node->id());
  }

  void OnNodeReparented(AXTree* tree, AXNode* node) override {}

  void OnNodeChanged(AXTree* tree, AXNode* node) override {
    changed_ids_.push_back(node->id());
  }

  void OnAtomicUpdateFinished(AXTree* tree,
                              bool root_changed,
                              const std::vector<Change>& changes) override {
    root_changed_ = root_changed;

    for (size_t i = 0; i < changes.size(); ++i) {
      int id = changes[i].node->id();
      switch (changes[i].type) {
        case NODE_CREATED:
          node_creation_finished_ids_.push_back(id);
          break;
        case SUBTREE_CREATED:
          subtree_creation_finished_ids_.push_back(id);
          break;
        case NODE_REPARENTED:
          node_reparented_finished_ids_.push_back(id);
          break;
        case SUBTREE_REPARENTED:
          subtree_reparented_finished_ids_.push_back(id);
          break;
        case NODE_CHANGED:
          change_finished_ids_.push_back(id);
          break;
      }
    }
  }

  void OnRoleChanged(AXTree* tree,
                     AXNode* node,
                     ax::mojom::Role old_role,
                     ax::mojom::Role new_role) override {
    attribute_change_log_.push_back(base::StringPrintf(
        "Role changed from %s to %s", ToString(old_role), ToString(new_role)));
  }

  void OnStateChanged(AXTree* tree,
                      AXNode* node,
                      ax::mojom::State state,
                      bool new_value) override {
    attribute_change_log_.push_back(base::StringPrintf(
        "%s changed to %s", ToString(state), new_value ? "true" : "false"));
  }

  void OnStringAttributeChanged(AXTree* tree,
                                AXNode* node,
                                ax::mojom::StringAttribute attr,
                                const std::string& old_value,
                                const std::string& new_value) override {
    attribute_change_log_.push_back(
        base::StringPrintf("%s changed from %s to %s", ToString(attr),
                           old_value.c_str(), new_value.c_str()));
  }

  void OnIntAttributeChanged(AXTree* tree,
                             AXNode* node,
                             ax::mojom::IntAttribute attr,
                             int32_t old_value,
                             int32_t new_value) override {
    attribute_change_log_.push_back(base::StringPrintf(
        "%s changed from %d to %d", ToString(attr), old_value, new_value));
  }

  void OnFloatAttributeChanged(AXTree* tree,
                               AXNode* node,
                               ax::mojom::FloatAttribute attr,
                               float old_value,
                               float new_value) override {
    attribute_change_log_.push_back(
        base::StringPrintf("%s changed from %s to %s", ToString(attr),
                           base::NumberToString(old_value).c_str(),
                           base::NumberToString(new_value).c_str()));
  }

  void OnBoolAttributeChanged(AXTree* tree,
                              AXNode* node,
                              ax::mojom::BoolAttribute attr,
                              bool new_value) override {
    attribute_change_log_.push_back(base::StringPrintf(
        "%s changed to %s", ToString(attr), new_value ? "true" : "false"));
  }

  void OnIntListAttributeChanged(
      AXTree* tree,
      AXNode* node,
      ax::mojom::IntListAttribute attr,
      const std::vector<int32_t>& old_value,
      const std::vector<int32_t>& new_value) override {
    attribute_change_log_.push_back(
        base::StringPrintf("%s changed from %s to %s", ToString(attr),
                           IntVectorToString(old_value).c_str(),
                           IntVectorToString(new_value).c_str()));
  }

  bool tree_data_changed() const { return tree_data_changed_; }
  bool root_changed() const { return root_changed_; }
  const std::vector<int32_t>& deleted_ids() { return deleted_ids_; }
  const std::vector<int32_t>& subtree_deleted_ids() {
    return subtree_deleted_ids_;
  }
  const std::vector<int32_t>& created_ids() { return created_ids_; }
  const std::vector<int32_t>& node_creation_finished_ids() {
    return node_creation_finished_ids_;
  }
  const std::vector<int32_t>& subtree_creation_finished_ids() {
    return subtree_creation_finished_ids_;
  }
  const std::vector<int32_t>& node_reparented_finished_ids() {
    return node_reparented_finished_ids_;
  }
  const std::vector<int32_t>& subtree_reparented_finished_ids() {
    return subtree_reparented_finished_ids_;
  }
  const std::vector<int32_t>& change_finished_ids() {
    return change_finished_ids_;
  }
  const std::vector<std::string>& attribute_change_log() {
    return attribute_change_log_;
  }

 private:
  bool tree_data_changed_;
  bool root_changed_;
  std::vector<int32_t> deleted_ids_;
  std::vector<int32_t> subtree_deleted_ids_;
  std::vector<int32_t> created_ids_;
  std::vector<int32_t> changed_ids_;
  std::vector<int32_t> node_creation_finished_ids_;
  std::vector<int32_t> subtree_creation_finished_ids_;
  std::vector<int32_t> node_reparented_finished_ids_;
  std::vector<int32_t> subtree_reparented_finished_ids_;
  std::vector<int32_t> change_finished_ids_;
  std::vector<std::string> attribute_change_log_;
};

}  // namespace

TEST(AXTreeTest, SerializeSimpleAXTree) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kDialog;
  root.AddState(ax::mojom::State::kFocusable);
  root.location = gfx::RectF(0, 0, 800, 600);
  root.child_ids.push_back(2);
  root.child_ids.push_back(3);

  AXNodeData button;
  button.id = 2;
  button.role = ax::mojom::Role::kButton;
  button.location = gfx::RectF(20, 20, 200, 30);

  AXNodeData checkbox;
  checkbox.id = 3;
  checkbox.role = ax::mojom::Role::kCheckBox;
  checkbox.location = gfx::RectF(20, 50, 200, 30);

  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.push_back(root);
  initial_state.nodes.push_back(button);
  initial_state.nodes.push_back(checkbox);
  initial_state.has_tree_data = true;
  initial_state.tree_data.title = "Title";
  AXSerializableTree src_tree(initial_state);

  std::unique_ptr<AXTreeSource<const AXNode*, AXNodeData, AXTreeData>>
      tree_source(src_tree.CreateTreeSource());
  AXTreeSerializer<const AXNode*, AXNodeData, AXTreeData> serializer(
      tree_source.get());
  AXTreeUpdate update;
  serializer.SerializeChanges(src_tree.root(), &update);

  AXTree dst_tree;
  ASSERT_TRUE(dst_tree.Unserialize(update));

  const AXNode* root_node = dst_tree.root();
  ASSERT_TRUE(root_node != nullptr);
  EXPECT_EQ(root.id, root_node->id());
  EXPECT_EQ(root.role, root_node->data().role);

  ASSERT_EQ(2, root_node->child_count());

  const AXNode* button_node = root_node->ChildAtIndex(0);
  EXPECT_EQ(button.id, button_node->id());
  EXPECT_EQ(button.role, button_node->data().role);

  const AXNode* checkbox_node = root_node->ChildAtIndex(1);
  EXPECT_EQ(checkbox.id, checkbox_node->id());
  EXPECT_EQ(checkbox.role, checkbox_node->data().role);

  EXPECT_EQ(
      "AXTree title=Title\n"
      "id=1 dialog FOCUSABLE (0, 0)-(800, 600) actions= child_ids=2,3\n"
      "  id=2 button (20, 20)-(200, 30) actions=\n"
      "  id=3 checkBox (20, 50)-(200, 30) actions=\n",
      dst_tree.ToString());
}

TEST(AXTreeTest, SerializeAXTreeUpdate) {
  AXNodeData list;
  list.id = 3;
  list.role = ax::mojom::Role::kList;
  list.child_ids.push_back(4);
  list.child_ids.push_back(5);
  list.child_ids.push_back(6);

  AXNodeData list_item_2;
  list_item_2.id = 5;
  list_item_2.role = ax::mojom::Role::kListItem;

  AXNodeData list_item_3;
  list_item_3.id = 6;
  list_item_3.role = ax::mojom::Role::kListItem;

  AXNodeData button;
  button.id = 7;
  button.role = ax::mojom::Role::kButton;

  AXTreeUpdate update;
  update.root_id = 3;
  update.nodes.push_back(list);
  update.nodes.push_back(list_item_2);
  update.nodes.push_back(list_item_3);
  update.nodes.push_back(button);

  EXPECT_EQ(
      "AXTreeUpdate: root id 3\n"
      "id=3 list (0, 0)-(0, 0) actions= child_ids=4,5,6\n"
      "  id=5 listItem (0, 0)-(0, 0) actions=\n"
      "  id=6 listItem (0, 0)-(0, 0) actions=\n"
      "id=7 button (0, 0)-(0, 0) actions=\n",
      update.ToString());
}

TEST(AXTreeTest, LeaveOrphanedDeletedSubtreeFails) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[2].id = 3;
  AXTree tree(initial_state);

  // This should fail because we delete a subtree rooted at id=2
  // but never update it.
  AXTreeUpdate update;
  update.node_id_to_clear = 2;
  update.nodes.resize(1);
  update.nodes[0].id = 3;
  EXPECT_FALSE(tree.Unserialize(update));
  ASSERT_EQ("Nodes left pending by the update: 2", tree.error());
}

TEST(AXTreeTest, LeaveOrphanedNewChildFails) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  // This should fail because we add a new child to the root node
  // but never update it.
  AXTreeUpdate update;
  update.nodes.resize(1);
  update.nodes[0].id = 1;
  update.nodes[0].child_ids.push_back(2);
  EXPECT_FALSE(tree.Unserialize(update));
  ASSERT_EQ("Nodes left pending by the update: 2", tree.error());
}

TEST(AXTreeTest, DuplicateChildIdFails) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  // This should fail because a child id appears twice.
  AXTreeUpdate update;
  update.nodes.resize(2);
  update.nodes[0].id = 1;
  update.nodes[0].child_ids.push_back(2);
  update.nodes[0].child_ids.push_back(2);
  update.nodes[1].id = 2;
  EXPECT_FALSE(tree.Unserialize(update));
  ASSERT_EQ("Node 1 has duplicate child id 2", tree.error());
}

TEST(AXTreeTest, InvalidReparentingFails) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;

  AXTree tree(initial_state);

  // This should fail because node 3 is reparented from node 2 to node 1
  // without deleting node 1's subtree first.
  AXTreeUpdate update;
  update.nodes.resize(3);
  update.nodes[0].id = 1;
  update.nodes[0].child_ids.push_back(3);
  update.nodes[0].child_ids.push_back(2);
  update.nodes[1].id = 2;
  update.nodes[2].id = 3;
  EXPECT_FALSE(tree.Unserialize(update));
  ASSERT_EQ("Node 3 reparented from 2 to 1", tree.error());
}

TEST(AXTreeTest, NoReparentingOfRootIfNoNewRoot) {
  AXNodeData root;
  root.id = 1;
  AXNodeData child1;
  child1.id = 2;
  AXNodeData child2;
  child2.id = 3;

  root.child_ids = {child1.id};
  child1.child_ids = {child2.id};

  AXTreeUpdate initial_state;
  initial_state.root_id = root.id;
  initial_state.nodes = {root, child1, child2};

  AXTree tree(initial_state);

  // Update the root but don't change it by reparenting |child2| to be a child
  // of the root.
  root.child_ids = {child1.id, child2.id};
  child1.child_ids = {};

  AXTreeUpdate update;
  update.root_id = root.id;
  update.node_id_to_clear = root.id;
  update.nodes = {root, child1, child2};

  FakeAXTreeDelegate fake_delegate;
  tree.SetDelegate(&fake_delegate);
  ASSERT_TRUE(tree.Unserialize(update));

  EXPECT_EQ(0U, fake_delegate.deleted_ids().size());
  EXPECT_EQ(0U, fake_delegate.subtree_deleted_ids().size());
  EXPECT_EQ(0U, fake_delegate.created_ids().size());

  EXPECT_EQ(0U, fake_delegate.node_creation_finished_ids().size());
  EXPECT_EQ(0U, fake_delegate.subtree_creation_finished_ids().size());
  EXPECT_EQ(0U, fake_delegate.node_reparented_finished_ids().size());

  ASSERT_EQ(2U, fake_delegate.subtree_reparented_finished_ids().size());
  EXPECT_EQ(child1.id, fake_delegate.subtree_reparented_finished_ids()[0]);
  EXPECT_EQ(child2.id, fake_delegate.subtree_reparented_finished_ids()[1]);

  ASSERT_EQ(1U, fake_delegate.change_finished_ids().size());
  EXPECT_EQ(root.id, fake_delegate.change_finished_ids()[0]);

  EXPECT_FALSE(fake_delegate.root_changed());
  EXPECT_FALSE(fake_delegate.tree_data_changed());

  tree.SetDelegate(nullptr);
}

TEST(AXTreeTest, ReparentRootIfRootChanged) {
  AXNodeData root;
  root.id = 1;
  AXNodeData child1;
  child1.id = 2;
  AXNodeData child2;
  child2.id = 3;

  root.child_ids = {child1.id};
  child1.child_ids = {child2.id};

  AXTreeUpdate initial_state;
  initial_state.root_id = root.id;
  initial_state.nodes = {root, child1, child2};

  AXTree tree(initial_state);

  // Create a new root and reparent |child2| to be a child of the new root.
  AXNodeData root2;
  root2.id = 4;
  root2.child_ids = {child1.id, child2.id};
  child1.child_ids = {};

  AXTreeUpdate update;
  update.root_id = root2.id;
  update.node_id_to_clear = root.id;
  update.nodes = {root2, child1, child2};

  FakeAXTreeDelegate fake_delegate;
  tree.SetDelegate(&fake_delegate);
  ASSERT_TRUE(tree.Unserialize(update));

  ASSERT_EQ(1U, fake_delegate.deleted_ids().size());
  EXPECT_EQ(root.id, fake_delegate.deleted_ids()[0]);

  ASSERT_EQ(1U, fake_delegate.subtree_deleted_ids().size());
  EXPECT_EQ(root.id, fake_delegate.subtree_deleted_ids()[0]);

  ASSERT_EQ(1U, fake_delegate.created_ids().size());
  EXPECT_EQ(root2.id, fake_delegate.created_ids()[0]);

  EXPECT_EQ(0U, fake_delegate.node_creation_finished_ids().size());

  ASSERT_EQ(1U, fake_delegate.subtree_creation_finished_ids().size());
  EXPECT_EQ(root2.id, fake_delegate.subtree_creation_finished_ids()[0]);

  ASSERT_EQ(2U, fake_delegate.node_reparented_finished_ids().size());
  EXPECT_EQ(child1.id, fake_delegate.node_reparented_finished_ids()[0]);
  EXPECT_EQ(child2.id, fake_delegate.node_reparented_finished_ids()[1]);

  EXPECT_EQ(0U, fake_delegate.subtree_reparented_finished_ids().size());

  EXPECT_EQ(0U, fake_delegate.change_finished_ids().size());

  EXPECT_TRUE(fake_delegate.root_changed());
  EXPECT_FALSE(fake_delegate.tree_data_changed());

  tree.SetDelegate(nullptr);
}

TEST(AXTreeTest, TreeDelegateIsCalled) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(2);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;

  AXTree tree(initial_state);
  AXTreeUpdate update;
  update.root_id = 3;
  update.node_id_to_clear = 1;
  update.nodes.resize(2);
  update.nodes[0].id = 3;
  update.nodes[0].child_ids.push_back(4);
  update.nodes[1].id = 4;

  FakeAXTreeDelegate fake_delegate;
  tree.SetDelegate(&fake_delegate);

  ASSERT_TRUE(tree.Unserialize(update));

  ASSERT_EQ(2U, fake_delegate.deleted_ids().size());
  EXPECT_EQ(1, fake_delegate.deleted_ids()[0]);
  EXPECT_EQ(2, fake_delegate.deleted_ids()[1]);

  ASSERT_EQ(1U, fake_delegate.subtree_deleted_ids().size());
  EXPECT_EQ(1, fake_delegate.subtree_deleted_ids()[0]);

  ASSERT_EQ(2U, fake_delegate.created_ids().size());
  EXPECT_EQ(3, fake_delegate.created_ids()[0]);
  EXPECT_EQ(4, fake_delegate.created_ids()[1]);

  ASSERT_EQ(1U, fake_delegate.subtree_creation_finished_ids().size());
  EXPECT_EQ(3, fake_delegate.subtree_creation_finished_ids()[0]);

  ASSERT_EQ(1U, fake_delegate.node_creation_finished_ids().size());
  EXPECT_EQ(4, fake_delegate.node_creation_finished_ids()[0]);

  ASSERT_TRUE(fake_delegate.root_changed());

  tree.SetDelegate(nullptr);
}

TEST(AXTreeTest, TreeDelegateIsCalledForTreeDataChanges) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.has_tree_data = true;
  initial_state.tree_data.title = "Initial";
  AXTree tree(initial_state);

  FakeAXTreeDelegate fake_delegate;
  tree.SetDelegate(&fake_delegate);

  // An empty update shouldn't change tree data.
  AXTreeUpdate empty_update;
  EXPECT_TRUE(tree.Unserialize(empty_update));
  EXPECT_FALSE(fake_delegate.tree_data_changed());
  EXPECT_EQ("Initial", tree.data().title);

  // An update with tree data shouldn't change tree data if
  // |has_tree_data| isn't set.
  AXTreeUpdate ignored_tree_data_update;
  ignored_tree_data_update.tree_data.title = "Ignore Me";
  EXPECT_TRUE(tree.Unserialize(ignored_tree_data_update));
  EXPECT_FALSE(fake_delegate.tree_data_changed());
  EXPECT_EQ("Initial", tree.data().title);

  // An update with |has_tree_data| set should update the tree data.
  AXTreeUpdate tree_data_update;
  tree_data_update.has_tree_data = true;
  tree_data_update.tree_data.title = "New Title";
  EXPECT_TRUE(tree.Unserialize(tree_data_update));
  EXPECT_TRUE(fake_delegate.tree_data_changed());
  EXPECT_EQ("New Title", tree.data().title);

  tree.SetDelegate(nullptr);
}

TEST(AXTreeTest, ReparentingDoesNotTriggerNodeCreated) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;

  FakeAXTreeDelegate fake_delegate;
  AXTree tree(initial_state);
  tree.SetDelegate(&fake_delegate);

  AXTreeUpdate update;
  update.nodes.resize(2);
  update.node_id_to_clear = 2;
  update.root_id = 1;
  update.nodes[0].id = 1;
  update.nodes[0].child_ids.push_back(3);
  update.nodes[1].id = 3;
  EXPECT_TRUE(tree.Unserialize(update)) << tree.error();
  std::vector<int> created = fake_delegate.node_creation_finished_ids();
  std::vector<int> subtree_reparented =
      fake_delegate.subtree_reparented_finished_ids();
  std::vector<int> node_reparented =
      fake_delegate.node_reparented_finished_ids();
  ASSERT_FALSE(base::ContainsValue(created, 3));
  ASSERT_TRUE(base::ContainsValue(subtree_reparented, 3));
  ASSERT_FALSE(base::ContainsValue(node_reparented, 3));
}

TEST(AXTreeTest, TreeDelegateIsNotCalledForReparenting) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(2);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;

  AXTree tree(initial_state);
  AXTreeUpdate update;
  update.node_id_to_clear = 1;
  update.root_id = 2;
  update.nodes.resize(2);
  update.nodes[0].id = 2;
  update.nodes[0].child_ids.push_back(4);
  update.nodes[1].id = 4;

  FakeAXTreeDelegate fake_delegate;
  tree.SetDelegate(&fake_delegate);

  EXPECT_TRUE(tree.Unserialize(update));

  ASSERT_EQ(1U, fake_delegate.deleted_ids().size());
  EXPECT_EQ(1, fake_delegate.deleted_ids()[0]);

  ASSERT_EQ(1U, fake_delegate.subtree_deleted_ids().size());
  EXPECT_EQ(1, fake_delegate.subtree_deleted_ids()[0]);

  ASSERT_EQ(1U, fake_delegate.created_ids().size());
  EXPECT_EQ(4, fake_delegate.created_ids()[0]);

  ASSERT_EQ(1U, fake_delegate.subtree_creation_finished_ids().size());
  EXPECT_EQ(4, fake_delegate.subtree_creation_finished_ids()[0]);

  ASSERT_EQ(1U, fake_delegate.subtree_reparented_finished_ids().size());
  EXPECT_EQ(2, fake_delegate.subtree_reparented_finished_ids()[0]);

  EXPECT_EQ(0U, fake_delegate.node_creation_finished_ids().size());
  EXPECT_EQ(0U, fake_delegate.node_reparented_finished_ids().size());

  ASSERT_TRUE(fake_delegate.root_changed());

  tree.SetDelegate(nullptr);
}

// UAF caught by ax_tree_fuzzer
TEST(AXTreeTest, BogusAXTree) {
  AXTreeUpdate initial_state;
  AXNodeData node;
  node.id = 0;
  initial_state.nodes.push_back(node);
  initial_state.nodes.push_back(node);
  ui::AXTree tree;
  tree.Unserialize(initial_state);
}

// UAF caught by ax_tree_fuzzer
TEST(AXTreeTest, BogusAXTree2) {
  AXTreeUpdate initial_state;
  AXNodeData node;
  node.id = 0;
  initial_state.nodes.push_back(node);
  AXNodeData node2;
  node2.id = 0;
  node2.child_ids.push_back(0);
  node2.child_ids.push_back(0);
  initial_state.nodes.push_back(node2);
  ui::AXTree tree;
  tree.Unserialize(initial_state);
}

// UAF caught by ax_tree_fuzzer
TEST(AXTreeTest, BogusAXTree3) {
  AXTreeUpdate initial_state;
  AXNodeData node;
  node.id = 0;
  node.child_ids.push_back(1);
  initial_state.nodes.push_back(node);

  AXNodeData node2;
  node2.id = 1;
  node2.child_ids.push_back(1);
  node2.child_ids.push_back(1);
  initial_state.nodes.push_back(node2);

  ui::AXTree tree;
  tree.Unserialize(initial_state);
}

TEST(AXTreeTest, RoleAndStateChangeCallbacks) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kButton;
  initial_state.nodes[0].SetCheckedState(ax::mojom::CheckedState::kTrue);
  initial_state.nodes[0].AddState(ax::mojom::State::kFocusable);
  AXTree tree(initial_state);

  FakeAXTreeDelegate fake_delegate;
  tree.SetDelegate(&fake_delegate);

  // Change the role and state.
  AXTreeUpdate update;
  update.root_id = 1;
  update.nodes.resize(1);
  update.nodes[0].id = 1;
  update.nodes[0].role = ax::mojom::Role::kCheckBox;
  update.nodes[0].SetCheckedState(ax::mojom::CheckedState::kFalse);
  update.nodes[0].AddState(ax::mojom::State::kFocusable);
  update.nodes[0].AddState(ax::mojom::State::kVisited);
  EXPECT_TRUE(tree.Unserialize(update));

  const std::vector<std::string>& change_log =
      fake_delegate.attribute_change_log();
  ASSERT_EQ(3U, change_log.size());
  EXPECT_EQ("Role changed from button to checkBox", change_log[0]);
  EXPECT_EQ("visited changed to true", change_log[1]);
  EXPECT_EQ("checkedState changed from 2 to 1", change_log[2]);

  tree.SetDelegate(nullptr);
}

TEST(AXTreeTest, AttributeChangeCallbacks) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "N1");
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kDescription, "D1");
  initial_state.nodes[0].AddBoolAttribute(ax::mojom::BoolAttribute::kLiveAtomic,
                                          true);
  initial_state.nodes[0].AddBoolAttribute(ax::mojom::BoolAttribute::kBusy,
                                          false);
  initial_state.nodes[0].AddFloatAttribute(
      ax::mojom::FloatAttribute::kMinValueForRange, 1.0);
  initial_state.nodes[0].AddFloatAttribute(
      ax::mojom::FloatAttribute::kMaxValueForRange, 10.0);
  initial_state.nodes[0].AddFloatAttribute(
      ax::mojom::FloatAttribute::kStepValueForRange, 3.0);
  initial_state.nodes[0].AddIntAttribute(ax::mojom::IntAttribute::kScrollX, 5);
  initial_state.nodes[0].AddIntAttribute(ax::mojom::IntAttribute::kScrollXMin,
                                         1);
  AXTree tree(initial_state);

  FakeAXTreeDelegate fake_delegate;
  tree.SetDelegate(&fake_delegate);

  // Change existing attributes.
  AXTreeUpdate update0;
  update0.root_id = 1;
  update0.nodes.resize(1);
  update0.nodes[0].id = 1;
  update0.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kName, "N2");
  update0.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kDescription,
                                      "D2");
  update0.nodes[0].AddBoolAttribute(ax::mojom::BoolAttribute::kLiveAtomic,
                                    false);
  update0.nodes[0].AddBoolAttribute(ax::mojom::BoolAttribute::kBusy, true);
  update0.nodes[0].AddFloatAttribute(
      ax::mojom::FloatAttribute::kMinValueForRange, 2.0);
  update0.nodes[0].AddFloatAttribute(
      ax::mojom::FloatAttribute::kMaxValueForRange, 9.0);
  update0.nodes[0].AddFloatAttribute(
      ax::mojom::FloatAttribute::kStepValueForRange, 0.5);
  update0.nodes[0].AddIntAttribute(ax::mojom::IntAttribute::kScrollX, 6);
  update0.nodes[0].AddIntAttribute(ax::mojom::IntAttribute::kScrollXMin, 2);
  EXPECT_TRUE(tree.Unserialize(update0));

  const std::vector<std::string>& change_log =
      fake_delegate.attribute_change_log();
  ASSERT_EQ(9U, change_log.size());
  EXPECT_EQ("name changed from N1 to N2", change_log[0]);
  EXPECT_EQ("description changed from D1 to D2", change_log[1]);
  EXPECT_EQ("liveAtomic changed to false", change_log[2]);
  EXPECT_EQ("busy changed to true", change_log[3]);
  EXPECT_EQ("minValueForRange changed from 1 to 2", change_log[4]);
  EXPECT_EQ("maxValueForRange changed from 10 to 9", change_log[5]);
  EXPECT_EQ("stepValueForRange changed from 3 to .5", change_log[6]);
  EXPECT_EQ("scrollX changed from 5 to 6", change_log[7]);
  EXPECT_EQ("scrollXMin changed from 1 to 2", change_log[8]);

  FakeAXTreeDelegate fake_delegate2;
  tree.SetDelegate(&fake_delegate2);

  // Add and remove attributes.
  AXTreeUpdate update1;
  update1.root_id = 1;
  update1.nodes.resize(1);
  update1.nodes[0].id = 1;
  update1.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kDescription,
                                      "D3");
  update1.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kValue, "V3");
  update1.nodes[0].AddBoolAttribute(ax::mojom::BoolAttribute::kModal, true);
  update1.nodes[0].AddFloatAttribute(ax::mojom::FloatAttribute::kValueForRange,
                                     5.0);
  update1.nodes[0].AddFloatAttribute(
      ax::mojom::FloatAttribute::kMaxValueForRange, 9.0);
  update1.nodes[0].AddIntAttribute(ax::mojom::IntAttribute::kScrollX, 7);
  update1.nodes[0].AddIntAttribute(ax::mojom::IntAttribute::kScrollXMax, 10);
  EXPECT_TRUE(tree.Unserialize(update1));

  const std::vector<std::string>& change_log2 =
      fake_delegate2.attribute_change_log();
  ASSERT_EQ(11U, change_log2.size());
  EXPECT_EQ("name changed from N2 to ", change_log2[0]);
  EXPECT_EQ("description changed from D2 to D3", change_log2[1]);
  EXPECT_EQ("value changed from  to V3", change_log2[2]);
  EXPECT_EQ("busy changed to false", change_log2[3]);
  EXPECT_EQ("modal changed to true", change_log2[4]);
  EXPECT_EQ("minValueForRange changed from 2 to 0", change_log2[5]);
  EXPECT_EQ("stepValueForRange changed from 3 to .5", change_log[6]);
  EXPECT_EQ("valueForRange changed from 0 to 5", change_log2[7]);
  EXPECT_EQ("scrollXMin changed from 2 to 0", change_log2[8]);
  EXPECT_EQ("scrollX changed from 6 to 7", change_log2[9]);
  EXPECT_EQ("scrollXMax changed from 0 to 10", change_log2[10]);

  tree.SetDelegate(nullptr);
}

TEST(AXTreeTest, IntListChangeCallbacks) {
  std::vector<int32_t> one;
  one.push_back(1);

  std::vector<int32_t> two;
  two.push_back(2);
  two.push_back(2);

  std::vector<int32_t> three;
  three.push_back(3);

  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].AddIntListAttribute(
      ax::mojom::IntListAttribute::kControlsIds, one);
  initial_state.nodes[0].AddIntListAttribute(
      ax::mojom::IntListAttribute::kRadioGroupIds, two);
  AXTree tree(initial_state);

  FakeAXTreeDelegate fake_delegate;
  tree.SetDelegate(&fake_delegate);

  // Change existing attributes.
  AXTreeUpdate update0;
  update0.root_id = 1;
  update0.nodes.resize(1);
  update0.nodes[0].id = 1;
  update0.nodes[0].AddIntListAttribute(
      ax::mojom::IntListAttribute::kControlsIds, two);
  update0.nodes[0].AddIntListAttribute(
      ax::mojom::IntListAttribute::kRadioGroupIds, three);
  EXPECT_TRUE(tree.Unserialize(update0));

  const std::vector<std::string>& change_log =
      fake_delegate.attribute_change_log();
  ASSERT_EQ(2U, change_log.size());
  EXPECT_EQ("controlsIds changed from 1 to 2,2", change_log[0]);
  EXPECT_EQ("radioGroupIds changed from 2,2 to 3", change_log[1]);

  FakeAXTreeDelegate fake_delegate2;
  tree.SetDelegate(&fake_delegate2);

  // Add and remove attributes.
  AXTreeUpdate update1;
  update1.root_id = 1;
  update1.nodes.resize(1);
  update1.nodes[0].id = 1;
  update1.nodes[0].AddIntListAttribute(
      ax::mojom::IntListAttribute::kRadioGroupIds, two);
  update1.nodes[0].AddIntListAttribute(ax::mojom::IntListAttribute::kFlowtoIds,
                                       three);
  EXPECT_TRUE(tree.Unserialize(update1));

  const std::vector<std::string>& change_log2 =
      fake_delegate2.attribute_change_log();
  ASSERT_EQ(3U, change_log2.size());
  EXPECT_EQ("controlsIds changed from 2,2 to ", change_log2[0]);
  EXPECT_EQ("radioGroupIds changed from 3 to 2,2", change_log2[1]);
  EXPECT_EQ("flowtoIds changed from  to 3", change_log2[2]);

  tree.SetDelegate(nullptr);
}

// Create a very simple tree and make sure that we can get the bounds of
// any node.
TEST(AXTreeTest, GetBoundsBasic) {
  AXTreeUpdate tree_update;
  tree_update.root_id = 1;
  tree_update.nodes.resize(2);
  tree_update.nodes[0].id = 1;
  tree_update.nodes[0].location = gfx::RectF(0, 0, 800, 600);
  tree_update.nodes[0].child_ids.push_back(2);
  tree_update.nodes[1].id = 2;
  tree_update.nodes[1].location = gfx::RectF(100, 10, 400, 300);
  AXTree tree(tree_update);

  EXPECT_EQ("(0, 0) size (800 x 600)", GetBoundsAsString(tree, 1));
  EXPECT_EQ("(100, 10) size (400 x 300)", GetBoundsAsString(tree, 2));
}

// If a node doesn't specify its location but at least one child does have
// a location, its computed bounds should be the union of all child bounds.
TEST(AXTreeTest, EmptyNodeBoundsIsUnionOfChildren) {
  AXTreeUpdate tree_update;
  tree_update.root_id = 1;
  tree_update.nodes.resize(4);
  tree_update.nodes[0].id = 1;
  tree_update.nodes[0].location = gfx::RectF(0, 0, 800, 600);
  tree_update.nodes[0].child_ids.push_back(2);
  tree_update.nodes[1].id = 2;
  tree_update.nodes[1].location = gfx::RectF();  // Deliberately empty.
  tree_update.nodes[1].child_ids.push_back(3);
  tree_update.nodes[1].child_ids.push_back(4);
  tree_update.nodes[2].id = 3;
  tree_update.nodes[2].location = gfx::RectF(100, 10, 400, 20);
  tree_update.nodes[3].id = 4;
  tree_update.nodes[3].location = gfx::RectF(200, 30, 400, 20);

  AXTree tree(tree_update);
  EXPECT_EQ("(100, 10) size (500 x 40)", GetBoundsAsString(tree, 2));
}

// If a node doesn't specify its location but at least one child does have
// a location, it will be offscreen if all of its children are offscreen.
TEST(AXTreeTest, EmptyNodeNotOffscreenEvenIfAllChildrenOffscreen) {
  AXTreeUpdate tree_update;
  tree_update.root_id = 1;
  tree_update.nodes.resize(4);
  tree_update.nodes[0].id = 1;
  tree_update.nodes[0].location = gfx::RectF(0, 0, 800, 600);
  tree_update.nodes[0].role = ax::mojom::Role::kRootWebArea;
  tree_update.nodes[0].AddBoolAttribute(
      ax::mojom::BoolAttribute::kClipsChildren, true);
  tree_update.nodes[0].child_ids.push_back(2);
  tree_update.nodes[1].id = 2;
  tree_update.nodes[1].location = gfx::RectF();  // Deliberately empty.
  tree_update.nodes[1].child_ids.push_back(3);
  tree_update.nodes[1].child_ids.push_back(4);
  // Both children are offscreen
  tree_update.nodes[2].id = 3;
  tree_update.nodes[2].location = gfx::RectF(900, 10, 400, 20);
  tree_update.nodes[3].id = 4;
  tree_update.nodes[3].location = gfx::RectF(1000, 30, 400, 20);

  AXTree tree(tree_update);
  EXPECT_FALSE(IsNodeOffscreen(tree, 2));
  EXPECT_TRUE(IsNodeOffscreen(tree, 3));
  EXPECT_TRUE(IsNodeOffscreen(tree, 4));
}

// Test that getting the bounds of a node works when there's a transform.
TEST(AXTreeTest, GetBoundsWithTransform) {
  AXTreeUpdate tree_update;
  tree_update.root_id = 1;
  tree_update.nodes.resize(3);
  tree_update.nodes[0].id = 1;
  tree_update.nodes[0].location = gfx::RectF(0, 0, 400, 300);
  tree_update.nodes[0].transform.reset(new gfx::Transform());
  tree_update.nodes[0].transform->Scale(2.0, 2.0);
  tree_update.nodes[0].child_ids.push_back(2);
  tree_update.nodes[0].child_ids.push_back(3);
  tree_update.nodes[1].id = 2;
  tree_update.nodes[1].location = gfx::RectF(20, 10, 50, 5);
  tree_update.nodes[2].id = 3;
  tree_update.nodes[2].location = gfx::RectF(20, 30, 50, 5);
  tree_update.nodes[2].transform.reset(new gfx::Transform());
  tree_update.nodes[2].transform->Scale(2.0, 2.0);

  AXTree tree(tree_update);
  EXPECT_EQ("(0, 0) size (800 x 600)", GetBoundsAsString(tree, 1));
  EXPECT_EQ("(40, 20) size (100 x 10)", GetBoundsAsString(tree, 2));
  EXPECT_EQ("(80, 120) size (200 x 20)", GetBoundsAsString(tree, 3));
}

// Test that getting the bounds of a node that's inside a container
// works correctly.
TEST(AXTreeTest, GetBoundsWithContainerId) {
  AXTreeUpdate tree_update;
  tree_update.root_id = 1;
  tree_update.nodes.resize(4);
  tree_update.nodes[0].id = 1;
  tree_update.nodes[0].location = gfx::RectF(0, 0, 800, 600);
  tree_update.nodes[0].child_ids.push_back(2);
  tree_update.nodes[1].id = 2;
  tree_update.nodes[1].location = gfx::RectF(100, 50, 600, 500);
  tree_update.nodes[1].child_ids.push_back(3);
  tree_update.nodes[1].child_ids.push_back(4);
  tree_update.nodes[2].id = 3;
  tree_update.nodes[2].offset_container_id = 2;
  tree_update.nodes[2].location = gfx::RectF(20, 30, 50, 5);
  tree_update.nodes[3].id = 4;
  tree_update.nodes[3].location = gfx::RectF(20, 30, 50, 5);

  AXTree tree(tree_update);
  EXPECT_EQ("(120, 80) size (50 x 5)", GetBoundsAsString(tree, 3));
  EXPECT_EQ("(20, 30) size (50 x 5)", GetBoundsAsString(tree, 4));
}

// Test that getting the bounds of a node that's inside a scrolling container
// works correctly.
TEST(AXTreeTest, GetBoundsWithScrolling) {
  AXTreeUpdate tree_update;
  tree_update.root_id = 1;
  tree_update.nodes.resize(3);
  tree_update.nodes[0].id = 1;
  tree_update.nodes[0].location = gfx::RectF(0, 0, 800, 600);
  tree_update.nodes[0].child_ids.push_back(2);
  tree_update.nodes[1].id = 2;
  tree_update.nodes[1].location = gfx::RectF(100, 50, 600, 500);
  tree_update.nodes[1].AddIntAttribute(ax::mojom::IntAttribute::kScrollX, 5);
  tree_update.nodes[1].AddIntAttribute(ax::mojom::IntAttribute::kScrollY, 10);
  tree_update.nodes[1].child_ids.push_back(3);
  tree_update.nodes[2].id = 3;
  tree_update.nodes[2].offset_container_id = 2;
  tree_update.nodes[2].location = gfx::RectF(20, 30, 50, 5);

  AXTree tree(tree_update);
  EXPECT_EQ("(115, 70) size (50 x 5)", GetBoundsAsString(tree, 3));
}

TEST(AXTreeTest, GetBoundsEmptyBoundsInheritsFromParent) {
  AXTreeUpdate tree_update;
  tree_update.root_id = 1;
  tree_update.nodes.resize(3);
  tree_update.nodes[0].id = 1;
  tree_update.nodes[0].location = gfx::RectF(0, 0, 800, 600);
  tree_update.nodes[1].AddBoolAttribute(
      ax::mojom::BoolAttribute::kClipsChildren, true);
  tree_update.nodes[0].child_ids.push_back(2);
  tree_update.nodes[1].id = 2;
  tree_update.nodes[1].location = gfx::RectF(300, 200, 100, 100);
  tree_update.nodes[1].child_ids.push_back(3);
  tree_update.nodes[2].id = 3;
  tree_update.nodes[2].location = gfx::RectF();

  AXTree tree(tree_update);
  EXPECT_EQ("(0, 0) size (800 x 600)", GetBoundsAsString(tree, 1));
  EXPECT_EQ("(300, 200) size (100 x 100)", GetBoundsAsString(tree, 2));
  EXPECT_EQ("(300, 200) size (100 x 100)", GetBoundsAsString(tree, 3));
  EXPECT_EQ("(0, 0) size (800 x 600)", GetUnclippedBoundsAsString(tree, 1));
  EXPECT_EQ("(300, 200) size (100 x 100)", GetUnclippedBoundsAsString(tree, 2));
  EXPECT_EQ("(300, 200) size (100 x 100)", GetUnclippedBoundsAsString(tree, 3));
  EXPECT_FALSE(IsNodeOffscreen(tree, 1));
  EXPECT_FALSE(IsNodeOffscreen(tree, 2));
  EXPECT_TRUE(IsNodeOffscreen(tree, 3));
}

TEST(AXTreeTest, GetBoundsCropsChildToRoot) {
  AXTreeUpdate tree_update;
  tree_update.root_id = 1;
  tree_update.nodes.resize(5);
  tree_update.nodes[0].id = 1;
  tree_update.nodes[0].location = gfx::RectF(0, 0, 800, 600);
  tree_update.nodes[0].AddBoolAttribute(
      ax::mojom::BoolAttribute::kClipsChildren, true);
  tree_update.nodes[0].child_ids.push_back(2);
  tree_update.nodes[0].child_ids.push_back(3);
  tree_update.nodes[0].child_ids.push_back(4);
  tree_update.nodes[0].child_ids.push_back(5);
  // Cropped in the top left
  tree_update.nodes[1].id = 2;
  tree_update.nodes[1].location = gfx::RectF(-100, -100, 150, 150);
  // Cropped in the bottom right
  tree_update.nodes[2].id = 3;
  tree_update.nodes[2].location = gfx::RectF(700, 500, 150, 150);
  // Offscreen on the top
  tree_update.nodes[3].id = 4;
  tree_update.nodes[3].location = gfx::RectF(50, -200, 150, 150);
  // Offscreen on the bottom
  tree_update.nodes[4].id = 5;
  tree_update.nodes[4].location = gfx::RectF(50, 700, 150, 150);

  AXTree tree(tree_update);
  EXPECT_EQ("(0, 0) size (50 x 50)", GetBoundsAsString(tree, 2));
  EXPECT_EQ("(700, 500) size (100 x 100)", GetBoundsAsString(tree, 3));
  EXPECT_EQ("(50, 0) size (150 x 1)", GetBoundsAsString(tree, 4));
  EXPECT_EQ("(50, 599) size (150 x 1)", GetBoundsAsString(tree, 5));

  // Check the unclipped bounds are as expected.
  EXPECT_EQ("(-100, -100) size (150 x 150)",
            GetUnclippedBoundsAsString(tree, 2));
  EXPECT_EQ("(700, 500) size (150 x 150)", GetUnclippedBoundsAsString(tree, 3));
  EXPECT_EQ("(50, -200) size (150 x 150)", GetUnclippedBoundsAsString(tree, 4));
  EXPECT_EQ("(50, 700) size (150 x 150)", GetUnclippedBoundsAsString(tree, 5));
}

TEST(AXTreeTest, GetBoundsSetsOffscreenIfClipsChildren) {
  AXTreeUpdate tree_update;
  tree_update.root_id = 1;
  tree_update.nodes.resize(5);
  tree_update.nodes[0].id = 1;
  tree_update.nodes[0].location = gfx::RectF(0, 0, 800, 600);
  tree_update.nodes[0].AddBoolAttribute(
      ax::mojom::BoolAttribute::kClipsChildren, true);
  tree_update.nodes[0].child_ids.push_back(2);
  tree_update.nodes[0].child_ids.push_back(3);

  tree_update.nodes[1].id = 2;
  tree_update.nodes[1].location = gfx::RectF(0, 0, 200, 200);
  tree_update.nodes[1].AddBoolAttribute(
      ax::mojom::BoolAttribute::kClipsChildren, true);
  tree_update.nodes[1].child_ids.push_back(4);

  tree_update.nodes[2].id = 3;
  tree_update.nodes[2].location = gfx::RectF(0, 0, 200, 200);
  tree_update.nodes[2].child_ids.push_back(5);

  // Clipped by its parent
  tree_update.nodes[3].id = 4;
  tree_update.nodes[3].location = gfx::RectF(250, 250, 100, 100);
  tree_update.nodes[3].offset_container_id = 2;

  // Outside of its parent, but its parent does not clip children,
  // so it should not be offscreen.
  tree_update.nodes[4].id = 5;
  tree_update.nodes[4].location = gfx::RectF(250, 250, 100, 100);
  tree_update.nodes[4].offset_container_id = 3;

  AXTree tree(tree_update);
  EXPECT_TRUE(IsNodeOffscreen(tree, 4));
  EXPECT_FALSE(IsNodeOffscreen(tree, 5));
}

TEST(AXTreeTest, GetBoundsUpdatesOffscreen) {
  AXTreeUpdate tree_update;
  tree_update.root_id = 1;
  tree_update.nodes.resize(5);
  tree_update.nodes[0].id = 1;
  tree_update.nodes[0].location = gfx::RectF(0, 0, 800, 600);
  tree_update.nodes[0].role = ax::mojom::Role::kRootWebArea;
  tree_update.nodes[0].AddBoolAttribute(
      ax::mojom::BoolAttribute::kClipsChildren, true);
  tree_update.nodes[0].child_ids.push_back(2);
  tree_update.nodes[0].child_ids.push_back(3);
  tree_update.nodes[0].child_ids.push_back(4);
  tree_update.nodes[0].child_ids.push_back(5);
  // Fully onscreen
  tree_update.nodes[1].id = 2;
  tree_update.nodes[1].location = gfx::RectF(10, 10, 150, 150);
  // Cropped in the bottom right
  tree_update.nodes[2].id = 3;
  tree_update.nodes[2].location = gfx::RectF(700, 500, 150, 150);
  // Offscreen on the top
  tree_update.nodes[3].id = 4;
  tree_update.nodes[3].location = gfx::RectF(50, -200, 150, 150);
  // Offscreen on the bottom
  tree_update.nodes[4].id = 5;
  tree_update.nodes[4].location = gfx::RectF(50, 700, 150, 150);

  AXTree tree(tree_update);
  EXPECT_FALSE(IsNodeOffscreen(tree, 2));
  EXPECT_FALSE(IsNodeOffscreen(tree, 3));
  EXPECT_TRUE(IsNodeOffscreen(tree, 4));
  EXPECT_TRUE(IsNodeOffscreen(tree, 5));
}

TEST(AXTreeTest, IntReverseRelations) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(4);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].AddIntAttribute(
      ax::mojom::IntAttribute::kActivedescendantId, 2);
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[0].child_ids.push_back(4);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].AddIntAttribute(ax::mojom::IntAttribute::kMemberOfId,
                                         1);
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].AddIntAttribute(ax::mojom::IntAttribute::kMemberOfId,
                                         1);
  AXTree tree(initial_state);

  auto reverse_active_descendant =
      tree.GetReverseRelations(ax::mojom::IntAttribute::kActivedescendantId, 2);
  ASSERT_EQ(1U, reverse_active_descendant.size());
  EXPECT_TRUE(base::ContainsKey(reverse_active_descendant, 1));

  reverse_active_descendant =
      tree.GetReverseRelations(ax::mojom::IntAttribute::kActivedescendantId, 1);
  ASSERT_EQ(0U, reverse_active_descendant.size());

  auto reverse_errormessage =
      tree.GetReverseRelations(ax::mojom::IntAttribute::kErrormessageId, 1);
  ASSERT_EQ(0U, reverse_errormessage.size());

  auto reverse_member_of =
      tree.GetReverseRelations(ax::mojom::IntAttribute::kMemberOfId, 1);
  ASSERT_EQ(2U, reverse_member_of.size());
  EXPECT_TRUE(base::ContainsKey(reverse_member_of, 3));
  EXPECT_TRUE(base::ContainsKey(reverse_member_of, 4));

  AXTreeUpdate update = initial_state;
  update.nodes.resize(5);
  update.nodes[0].int_attributes.clear();
  update.nodes[0].AddIntAttribute(ax::mojom::IntAttribute::kActivedescendantId,
                                  5);
  update.nodes[0].child_ids.push_back(5);
  update.nodes[2].int_attributes.clear();
  update.nodes[4].id = 5;
  update.nodes[4].AddIntAttribute(ax::mojom::IntAttribute::kMemberOfId, 1);

  EXPECT_TRUE(tree.Unserialize(update));

  reverse_active_descendant =
      tree.GetReverseRelations(ax::mojom::IntAttribute::kActivedescendantId, 2);
  ASSERT_EQ(0U, reverse_active_descendant.size());

  reverse_active_descendant =
      tree.GetReverseRelations(ax::mojom::IntAttribute::kActivedescendantId, 5);
  ASSERT_EQ(1U, reverse_active_descendant.size());
  EXPECT_TRUE(base::ContainsKey(reverse_active_descendant, 1));

  reverse_member_of =
      tree.GetReverseRelations(ax::mojom::IntAttribute::kMemberOfId, 1);
  ASSERT_EQ(2U, reverse_member_of.size());
  EXPECT_TRUE(base::ContainsKey(reverse_member_of, 4));
  EXPECT_TRUE(base::ContainsKey(reverse_member_of, 5));
}

TEST(AXTreeTest, IntListReverseRelations) {
  std::vector<int32_t> node_two;
  node_two.push_back(2);

  std::vector<int32_t> nodes_two_three;
  nodes_two_three.push_back(2);
  nodes_two_three.push_back(3);

  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].AddIntListAttribute(
      ax::mojom::IntListAttribute::kLabelledbyIds, node_two);
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[2].id = 3;

  AXTree tree(initial_state);

  auto reverse_labelled_by =
      tree.GetReverseRelations(ax::mojom::IntListAttribute::kLabelledbyIds, 2);
  ASSERT_EQ(1U, reverse_labelled_by.size());
  EXPECT_TRUE(base::ContainsKey(reverse_labelled_by, 1));

  reverse_labelled_by =
      tree.GetReverseRelations(ax::mojom::IntListAttribute::kLabelledbyIds, 3);
  ASSERT_EQ(0U, reverse_labelled_by.size());

  // Change existing attributes.
  AXTreeUpdate update = initial_state;
  update.nodes[0].intlist_attributes.clear();
  update.nodes[0].AddIntListAttribute(
      ax::mojom::IntListAttribute::kLabelledbyIds, nodes_two_three);
  EXPECT_TRUE(tree.Unserialize(update));

  reverse_labelled_by =
      tree.GetReverseRelations(ax::mojom::IntListAttribute::kLabelledbyIds, 3);
  ASSERT_EQ(1U, reverse_labelled_by.size());
  EXPECT_TRUE(base::ContainsKey(reverse_labelled_by, 1));
}

TEST(AXTreeTest, DeletingNodeUpdatesReverseRelations) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids = {2, 3};
  initial_state.nodes[1].id = 2;
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].AddIntAttribute(
      ax::mojom::IntAttribute::kActivedescendantId, 2);
  AXTree tree(initial_state);

  auto reverse_active_descendant =
      tree.GetReverseRelations(ax::mojom::IntAttribute::kActivedescendantId, 2);
  ASSERT_EQ(1U, reverse_active_descendant.size());
  EXPECT_TRUE(base::ContainsKey(reverse_active_descendant, 3));

  AXTreeUpdate update;
  update.root_id = 1;
  update.nodes.resize(1);
  update.nodes[0].id = 1;
  update.nodes[0].child_ids = {2};
  EXPECT_TRUE(tree.Unserialize(update));

  reverse_active_descendant =
      tree.GetReverseRelations(ax::mojom::IntAttribute::kActivedescendantId, 2);
  ASSERT_EQ(0U, reverse_active_descendant.size());
}

TEST(AXTreeTest, ReverseRelationsDoNotKeepGrowing) {
  // The number of total entries in int_reverse_relations and
  // intlist_reverse_relations should not keep growing as the tree
  // changes.

  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(2);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].AddIntAttribute(
      ax::mojom::IntAttribute::kActivedescendantId, 2);
  initial_state.nodes[0].AddIntListAttribute(
      ax::mojom::IntListAttribute::kLabelledbyIds, {2});
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  AXTree tree(initial_state);

  for (int i = 0; i < 1000; ++i) {
    AXTreeUpdate update;
    update.root_id = 1;
    update.nodes.resize(2);
    update.nodes[0].id = 1;
    update.nodes[1].id = i + 3;
    update.nodes[0].AddIntAttribute(
        ax::mojom::IntAttribute::kActivedescendantId, update.nodes[1].id);
    update.nodes[0].AddIntListAttribute(
        ax::mojom::IntListAttribute::kLabelledbyIds, {update.nodes[1].id});
    update.nodes[1].AddIntAttribute(ax::mojom::IntAttribute::kMemberOfId, 1);
    update.nodes[0].child_ids.push_back(update.nodes[1].id);
    EXPECT_TRUE(tree.Unserialize(update));
  }

  size_t map_key_count = 0;
  size_t set_entry_count = 0;
  for (auto& iter : tree.int_reverse_relations()) {
    map_key_count += iter.second.size() + 1;
    for (auto it2 = iter.second.begin(); it2 != iter.second.end(); ++it2) {
      set_entry_count += it2->second.size();
    }
  }

  // Note: 10 is arbitary, the idea here is just that we mutated the tree
  // 1000 times, so if we have fewer than 10 entries in the maps / sets then
  // the map isn't growing / leaking. Same below.
  EXPECT_LT(map_key_count, 10U);
  EXPECT_LT(set_entry_count, 10U);

  map_key_count = 0;
  set_entry_count = 0;
  for (auto& iter : tree.intlist_reverse_relations()) {
    map_key_count += iter.second.size() + 1;
    for (auto it2 = iter.second.begin(); it2 != iter.second.end(); ++it2) {
      set_entry_count += it2->second.size();
    }
  }
  EXPECT_LT(map_key_count, 10U);
  EXPECT_LT(set_entry_count, 10U);
}

TEST(AXTreeTest, SkipIgnoredNodes) {
  AXTreeUpdate tree_update;
  tree_update.root_id = 1;
  tree_update.nodes.resize(5);
  tree_update.nodes[0].id = 1;
  tree_update.nodes[0].child_ids = {2, 3};
  tree_update.nodes[1].id = 2;
  tree_update.nodes[1].AddState(ax::mojom::State::kIgnored);
  tree_update.nodes[1].child_ids = {4, 5};
  tree_update.nodes[2].id = 3;
  tree_update.nodes[3].id = 4;
  tree_update.nodes[4].id = 5;

  AXTree tree(tree_update);
  AXNode* root = tree.root();
  ASSERT_EQ(2, root->child_count());
  ASSERT_EQ(2, root->ChildAtIndex(0)->id());
  ASSERT_EQ(3, root->ChildAtIndex(1)->id());

  EXPECT_EQ(3, root->GetUnignoredChildCount());
  EXPECT_EQ(4, root->GetUnignoredChildAtIndex(0)->id());
  EXPECT_EQ(5, root->GetUnignoredChildAtIndex(1)->id());
  EXPECT_EQ(3, root->GetUnignoredChildAtIndex(2)->id());
  EXPECT_EQ(0, root->GetUnignoredChildAtIndex(0)->GetUnignoredIndexInParent());
  EXPECT_EQ(1, root->GetUnignoredChildAtIndex(1)->GetUnignoredIndexInParent());
  EXPECT_EQ(2, root->GetUnignoredChildAtIndex(2)->GetUnignoredIndexInParent());

  EXPECT_EQ(1, root->GetUnignoredChildAtIndex(0)->GetUnignoredParent()->id());
}

TEST(AXTreeTest, ChildTreeIds) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(4);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[0].child_ids.push_back(4);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kChildTreeId, "92");
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kChildTreeId, "93");
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].AddStringAttribute(
      ax::mojom::StringAttribute::kChildTreeId, "93");
  AXTree tree(initial_state);

  auto child_tree_91_nodes =
      tree.GetNodeIdsForChildTreeId(AXTreeID::FromString("91"));
  EXPECT_EQ(0U, child_tree_91_nodes.size());

  auto child_tree_92_nodes =
      tree.GetNodeIdsForChildTreeId(AXTreeID::FromString("92"));
  EXPECT_EQ(1U, child_tree_92_nodes.size());
  EXPECT_TRUE(base::ContainsKey(child_tree_92_nodes, 2));

  auto child_tree_93_nodes =
      tree.GetNodeIdsForChildTreeId(AXTreeID::FromString("93"));
  EXPECT_EQ(2U, child_tree_93_nodes.size());
  EXPECT_TRUE(base::ContainsKey(child_tree_93_nodes, 3));
  EXPECT_TRUE(base::ContainsKey(child_tree_93_nodes, 4));

  AXTreeUpdate update = initial_state;
  update.nodes[2].string_attributes.clear();
  update.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kChildTreeId,
                                     "92");
  update.nodes[3].string_attributes.clear();

  EXPECT_TRUE(tree.Unserialize(update));

  child_tree_92_nodes =
      tree.GetNodeIdsForChildTreeId(AXTreeID::FromString("92"));
  EXPECT_EQ(2U, child_tree_92_nodes.size());
  EXPECT_TRUE(base::ContainsKey(child_tree_92_nodes, 2));
  EXPECT_TRUE(base::ContainsKey(child_tree_92_nodes, 3));

  child_tree_93_nodes =
      tree.GetNodeIdsForChildTreeId(AXTreeID::FromString("93"));
  EXPECT_EQ(0U, child_tree_93_nodes.size());
}

}  // namespace ui
