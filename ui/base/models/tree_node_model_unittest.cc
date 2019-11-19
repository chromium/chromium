// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/tree_node_model.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace ui {

class TreeNodeModelTest : public testing::Test, public TreeModelObserver {
 public:
  TreeNodeModelTest() = default;
  ~TreeNodeModelTest() override = default;

 protected:
  std::string GetObserverCountStateAndClear() {
    std::string result(base::StringPrintf("added=%d removed=%d changed=%d",
        added_count_, removed_count_, changed_count_));
    added_count_ = removed_count_ = changed_count_ = 0;
    return result;
  }

 private:
  // Overridden from TreeModelObserver:
  void TreeNodesAdded(TreeModel* model,
                      TreeModelNode* parent,
                      size_t start,
                      size_t count) override {
    added_count_++;
  }
  void TreeNodesRemoved(TreeModel* model,
                        TreeModelNode* parent,
                        size_t start,
                        size_t count) override {
    removed_count_++;
  }
  void TreeNodeChanged(TreeModel* model, TreeModelNode* node) override {
    changed_count_++;
  }

  int added_count_ = 0;
  int removed_count_ = 0;
  int changed_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TreeNodeModelTest);
};

typedef TreeNodeWithValue<int> TestNode;

// Verifies if the model is properly adding a new node in the tree and
// notifying the observers.
// The tree looks like this:
// root
// +-- child1
//     +-- foo1
//     +-- foo2
// +-- child2
TEST_F(TreeNodeModelTest, AddNode) {
  TreeNodeModel<TestNode> model(std::make_unique<TestNode>());
  TestNode* root = model.GetRoot();
  model.AddObserver(this);

  TestNode* child1 = model.Add(root, std::make_unique<TestNode>(), 0);

  EXPECT_EQ("added=1 removed=0 changed=0", GetObserverCountStateAndClear());

  for (size_t i = 0; i < 2; ++i)
    child1->Add(std::make_unique<TestNode>(), i);

  TestNode* child2 = model.Add(root, std::make_unique<TestNode>(), 1);

  EXPECT_EQ("added=1 removed=0 changed=0", GetObserverCountStateAndClear());

  EXPECT_EQ(2u, root->children().size());
  EXPECT_EQ(2u, child1->children().size());
  EXPECT_EQ(0u, child2->children().size());
}

// Verifies if the model is properly removing a node from the tree
// and notifying the observers.
TEST_F(TreeNodeModelTest, RemoveNode) {
  TreeNodeModel<TestNode> model(std::make_unique<TestNode>());
  TestNode* root = model.GetRoot();
  model.AddObserver(this);

  TestNode* child1 = root->Add(std::make_unique<TestNode>(), 0);

  EXPECT_FALSE(root->children().empty());

  // Now remove |child1| from |root| and release the memory.
  model.Remove(root, child1);

  EXPECT_EQ("added=0 removed=1 changed=0", GetObserverCountStateAndClear());

  EXPECT_TRUE(root->children().empty());
}

// Verifies if the nodes added under the root are all deleted when calling
// DeleteAll.
// The tree looks like this:
// root
// +-- child1
//     +-- foo
//         +-- bar0
//         +-- bar1
//         +-- bar2
// +-- child2
// +-- child3
TEST_F(TreeNodeModelTest, DeleteAllNodes) {
  TestNode root;

  TestNode* child1 = root.Add(std::make_unique<TestNode>(), 0);
  root.Add(std::make_unique<TestNode>(), 1);  // child2
  root.Add(std::make_unique<TestNode>(), 2);  // child3

  TestNode* foo = child1->Add(std::make_unique<TestNode>(), 0);

  // Add some nodes to |foo|.
  for (size_t i = 0; i < 3; ++i)
    foo->Add(std::make_unique<TestNode>(), i);  // bar[n]

  EXPECT_EQ(3u, root.children().size());
  EXPECT_EQ(1u, child1->children().size());
  EXPECT_EQ(3u, foo->children().size());

  // Now remove the child nodes from root.
  root.DeleteAll();

  EXPECT_EQ(0u, root.children().size());
  EXPECT_TRUE(root.children().empty());
}

// Verifies if GetIndexOf() returns the correct index for the specified node.
// The tree looks like this:
// root
// +-- child1
//     +-- foo1
// +-- child2
TEST_F(TreeNodeModelTest, GetIndexOf) {
  TestNode root;

  TestNode* child1 = root.Add(std::make_unique<TestNode>(), 0);
  TestNode* child2 = root.Add(std::make_unique<TestNode>(), 1);
  TestNode* foo1 = child1->Add(std::make_unique<TestNode>(), 0);

  EXPECT_EQ(-1, root.GetIndexOf(&root));
  EXPECT_EQ(0, root.GetIndexOf(child1));
  EXPECT_EQ(1, root.GetIndexOf(child2));
  EXPECT_EQ(-1, root.GetIndexOf(foo1));

  EXPECT_EQ(-1, child1->GetIndexOf(&root));
  EXPECT_EQ(-1, child1->GetIndexOf(child1));
  EXPECT_EQ(-1, child1->GetIndexOf(child2));
  EXPECT_EQ(0, child1->GetIndexOf(foo1));

  EXPECT_EQ(-1, child2->GetIndexOf(&root));
  EXPECT_EQ(-1, child2->GetIndexOf(child2));
  EXPECT_EQ(-1, child2->GetIndexOf(child1));
  EXPECT_EQ(-1, child2->GetIndexOf(foo1));
}

// Verifies whether a specified node has or not an ancestor.
// The tree looks like this:
// root
// +-- child1
//     +-- foo1
// +-- child2
TEST_F(TreeNodeModelTest, HasAncestor) {
  TestNode root;

  TestNode* child1 = root.Add(std::make_unique<TestNode>(), 0);
  TestNode* child2 = root.Add(std::make_unique<TestNode>(), 1);

  TestNode* foo1 = child1->Add(std::make_unique<TestNode>(), 0);

  EXPECT_TRUE(root.HasAncestor(&root));
  EXPECT_FALSE(root.HasAncestor(child1));
  EXPECT_FALSE(root.HasAncestor(child2));
  EXPECT_FALSE(root.HasAncestor(foo1));

  EXPECT_TRUE(child1->HasAncestor(child1));
  EXPECT_TRUE(child1->HasAncestor(&root));
  EXPECT_FALSE(child1->HasAncestor(child2));
  EXPECT_FALSE(child1->HasAncestor(foo1));

  EXPECT_TRUE(child2->HasAncestor(child2));
  EXPECT_TRUE(child2->HasAncestor(&root));
  EXPECT_FALSE(child2->HasAncestor(child1));
  EXPECT_FALSE(child2->HasAncestor(foo1));

  EXPECT_TRUE(foo1->HasAncestor(foo1));
  EXPECT_TRUE(foo1->HasAncestor(child1));
  EXPECT_TRUE(foo1->HasAncestor(&root));
  EXPECT_FALSE(foo1->HasAncestor(child2));
}

// Verifies if GetTotalNodeCount returns the correct number of nodes from the
// node specified. The count should include the node itself.
// The tree looks like this:
// root
// +-- child1
//     +-- child2
//         +-- child3
// +-- foo1
//     +-- foo2
//         +-- foo3
//     +-- foo4
// +-- bar1
//
// The TotalNodeCount of root is:            9
// The TotalNodeCount of child1 is:          3
// The TotalNodeCount of child2 and foo2 is: 2
// The TotalNodeCount of bar1 is:            1
// And so on...
TEST_F(TreeNodeModelTest, GetTotalNodeCount) {
  TestNode root;

  TestNode* child1 = root.Add(std::make_unique<TestNode>(), 0);
  TestNode* child2 = child1->Add(std::make_unique<TestNode>(), 0);
  child2->Add(std::make_unique<TestNode>(), 0);  // child3

  TestNode* foo1 = root.Add(std::make_unique<TestNode>(), 1);
  TestNode* foo2 = foo1->Add(std::make_unique<TestNode>(), 0);
  foo2->Add(std::make_unique<TestNode>(), 0);  // foo3
  foo1->Add(std::make_unique<TestNode>(), 1);  // foo4

  TestNode* bar1 = root.Add(std::make_unique<TestNode>(), 2);

  EXPECT_EQ(9, root.GetTotalNodeCount());
  EXPECT_EQ(3, child1->GetTotalNodeCount());
  EXPECT_EQ(2, child2->GetTotalNodeCount());
  EXPECT_EQ(2, foo2->GetTotalNodeCount());
  EXPECT_EQ(1, bar1->GetTotalNodeCount());
}

// Makes sure that we are notified when the node is renamed,
// also makes sure the node is properly renamed.
TEST_F(TreeNodeModelTest, SetTitle) {
  TreeNodeModel<TestNode> model(
      std::make_unique<TestNode>(ASCIIToUTF16("root"), 0));
  TestNode* root = model.GetRoot();
  model.AddObserver(this);

  const base::string16 title(ASCIIToUTF16("root2"));
  model.SetTitle(root, title);
  EXPECT_EQ("added=0 removed=0 changed=1", GetObserverCountStateAndClear());
  EXPECT_EQ(title, root->GetTitle());
}

TEST_F(TreeNodeModelTest, BasicOperations) {
  TestNode root;
  EXPECT_EQ(0u, root.children().size());

  TestNode* child1 = root.Add(std::make_unique<TestNode>());
  EXPECT_EQ(1u, root.children().size());
  EXPECT_EQ(&root, child1->parent());

  TestNode* child2 = root.Add(std::make_unique<TestNode>());
  EXPECT_EQ(2u, root.children().size());
  EXPECT_EQ(child1->parent(), child2->parent());

  std::unique_ptr<TestNode> c2 = root.Remove(1);
  EXPECT_EQ(1u, root.children().size());
  EXPECT_EQ(NULL, child2->parent());

  std::unique_ptr<TestNode> c1 = root.Remove(0);
  EXPECT_EQ(0u, root.children().size());
}

TEST_F(TreeNodeModelTest, IsRoot) {
  TestNode root;
  EXPECT_TRUE(root.is_root());

  TestNode* child1 = root.Add(std::make_unique<TestNode>());
  EXPECT_FALSE(child1->is_root());
}

}  // namespace ui
