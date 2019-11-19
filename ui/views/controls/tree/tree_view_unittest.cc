// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/tree/tree_view.h"

#include <numeric>
#include <string>

#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/models/tree_node_model.h"
#include "ui/views/controls/prefix_selector.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/tree/tree_view_controller.h"
#include "ui/views/test/views_test_base.h"

using ui::TreeModel;
using ui::TreeModelNode;
using ui::TreeNode;

using base::ASCIIToUTF16;

namespace views {

class TestNode : public TreeNode<TestNode> {
 public:
  TestNode() = default;
  ~TestNode() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestNode);
};

// Creates the following structure:
// 'root'
//   'a'
//   'b'
//     'b1'
//   'c'
class TreeViewTest : public ViewsTestBase {
 public:
  TreeViewTest() : model_(std::make_unique<TestNode>()) {
    static_cast<TestNode*>(model_.GetRoot())->SetTitle(ASCIIToUTF16("root"));
    Add(model_.GetRoot(), 0, "a");
    Add(Add(model_.GetRoot(), 1, "b"), 0, "b1");
    Add(model_.GetRoot(), 2, "c");
  }

 protected:
  TestNode* Add(TestNode* parent, size_t index, const std::string& title);

  std::string TreeViewContentsAsString();

  std::string GetSelectedNodeTitle();

  std::string GetEditingNodeTitle();

  TestNode* GetNodeByTitle(const std::string& title);

  void IncrementSelection(bool next);
  void CollapseOrSelectParent();
  void ExpandOrSelectChild();
  int GetRowCount();
  PrefixSelector* selector() { return tree_.GetPrefixSelector(); }

  ui::TreeNodeModel<TestNode> model_;
  TreeView tree_;

 private:
  std::string InternalNodeAsString(TreeView::InternalNode* node);

  TestNode* GetNodeByTitleImpl(TestNode* node, const base::string16& title);

  DISALLOW_COPY_AND_ASSIGN(TreeViewTest);
};

TestNode* TreeViewTest::Add(TestNode* parent,
                            size_t index,
                            const std::string& title) {
  std::unique_ptr<TestNode> new_node = std::make_unique<TestNode>();
  new_node->SetTitle(ASCIIToUTF16(title));
  return model_.Add(parent, std::move(new_node), index);
}

std::string TreeViewTest::TreeViewContentsAsString() {
  return InternalNodeAsString(&tree_.root_);
}

std::string TreeViewTest::GetSelectedNodeTitle() {
  TreeModelNode* model_node = tree_.GetSelectedNode();
  return model_node ? base::UTF16ToASCII(model_node->GetTitle())
                    : std::string();
}

std::string TreeViewTest::GetEditingNodeTitle() {
  TreeModelNode* model_node = tree_.GetEditingNode();
  return model_node ? base::UTF16ToASCII(model_node->GetTitle())
                    : std::string();
}

TestNode* TreeViewTest::GetNodeByTitle(const std::string& title) {
  return GetNodeByTitleImpl(model_.GetRoot(), ASCIIToUTF16(title));
}

void TreeViewTest::IncrementSelection(bool next) {
  tree_.IncrementSelection(next ? TreeView::INCREMENT_NEXT :
                           TreeView::INCREMENT_PREVIOUS);
}

void TreeViewTest::CollapseOrSelectParent() {
  tree_.CollapseOrSelectParent();
}

void TreeViewTest::ExpandOrSelectChild() {
  tree_.ExpandOrSelectChild();
}

int TreeViewTest::GetRowCount() {
  return tree_.GetRowCount();
}

TestNode* TreeViewTest::GetNodeByTitleImpl(TestNode* node,
                                           const base::string16& title) {
  if (node->GetTitle() == title)
    return node;
  for (auto& child : node->children()) {
    TestNode* matching_node = GetNodeByTitleImpl(child.get(), title);
    if (matching_node)
      return matching_node;
  }
  return nullptr;
}

std::string TreeViewTest::InternalNodeAsString(
    TreeView::InternalNode* node) {
  std::string result = base::UTF16ToASCII(node->model_node()->GetTitle());
  if (node->is_expanded() && !node->children().empty()) {
    result += std::accumulate(
                  node->children().cbegin() + 1, node->children().cend(),
                  " [" + InternalNodeAsString(node->children().front().get()),
                  [this](const std::string& str, const auto& child) {
                    return str + " " + InternalNodeAsString(child.get());
                  }) +
              "]";
  }
  return result;
}

// Verifies setting model correctly updates internal state.
TEST_F(TreeViewTest, SetModel) {
  tree_.SetModel(&model_);
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ(4, GetRowCount());
}

// Verifies SetSelectedNode works.
TEST_F(TreeViewTest, SetSelectedNode) {
  tree_.SetModel(&model_);
  EXPECT_EQ("root", GetSelectedNodeTitle());

  // NULL should clear the selection.
  tree_.SetSelectedNode(nullptr);
  EXPECT_EQ(std::string(), GetSelectedNodeTitle());

  // Select 'c'.
  tree_.SetSelectedNode(GetNodeByTitle("c"));
  EXPECT_EQ("c", GetSelectedNodeTitle());

  // Select 'b1', which should expand 'b'.
  tree_.SetSelectedNode(GetNodeByTitle("b1"));
  EXPECT_EQ("root [a b [b1] c]", TreeViewContentsAsString());
  EXPECT_EQ("b1", GetSelectedNodeTitle());
}

// Makes sure SetRootShown doesn't blow up.
TEST_F(TreeViewTest, HideRoot) {
  tree_.SetModel(&model_);
  tree_.SetRootShown(false);
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("a", GetSelectedNodeTitle());
  EXPECT_EQ(3, GetRowCount());
}

// Expands a node and verifies the children are loaded correctly.
TEST_F(TreeViewTest, Expand) {
  tree_.SetModel(&model_);
  tree_.Expand(GetNodeByTitle("b1"));
  EXPECT_EQ("root [a b [b1] c]", TreeViewContentsAsString());
  EXPECT_EQ("root",GetSelectedNodeTitle());
  EXPECT_EQ(5, GetRowCount());
}

// Collapes a node and verifies state.
TEST_F(TreeViewTest, Collapse) {
  tree_.SetModel(&model_);
  tree_.Expand(GetNodeByTitle("b1"));
  EXPECT_EQ("root [a b [b1] c]", TreeViewContentsAsString());
  EXPECT_EQ(5, GetRowCount());
  tree_.SetSelectedNode(GetNodeByTitle("b1"));
  EXPECT_EQ("b1", GetSelectedNodeTitle());
  tree_.Collapse(GetNodeByTitle("b"));
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  // Selected node should have moved to 'b'
  EXPECT_EQ("b", GetSelectedNodeTitle());
  EXPECT_EQ(4, GetRowCount());
}

// Verifies adding nodes works.
TEST_F(TreeViewTest, TreeNodesAdded) {
  tree_.SetModel(&model_);
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  // Add a node between b and c.
  Add(model_.GetRoot(), 2, "B");
  EXPECT_EQ("root [a b B c]", TreeViewContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ(5, GetRowCount());

  // Add a child of b1, which hasn't been loaded and shouldn't do anything.
  Add(GetNodeByTitle("b1"), 0, "b11");
  EXPECT_EQ("root [a b B c]", TreeViewContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ(5, GetRowCount());

  // Add a child of b, which isn't expanded yet, so it shouldn't effect
  // anything.
  Add(GetNodeByTitle("b"), 1, "b2");
  EXPECT_EQ("root [a b B c]", TreeViewContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ(5, GetRowCount());

  // Expand b and make sure b2 is there.
  tree_.Expand(GetNodeByTitle("b"));
  EXPECT_EQ("root [a b [b1 b2] B c]", TreeViewContentsAsString());
  EXPECT_EQ("root",GetSelectedNodeTitle());
  EXPECT_EQ(7, GetRowCount());
}

// Verifies removing nodes works.
TEST_F(TreeViewTest, TreeNodesRemoved) {
  // Add c1 as a child of c and c11 as a child of c1.
  Add(Add(GetNodeByTitle("c"), 0, "c1"), 0, "c11");
  tree_.SetModel(&model_);

  // Remove c11, which shouldn't have any effect on the tree.
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ(4, GetRowCount());

  // Expand b1, then collapse it and remove its only child, b1. This shouldn't
  // effect the tree.
  tree_.Expand(GetNodeByTitle("b"));
  tree_.Collapse(GetNodeByTitle("b"));
  model_.Remove(GetNodeByTitle("b1")->parent(), GetNodeByTitle("b1"));
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ(4, GetRowCount());

  // Remove 'b'.
  model_.Remove(GetNodeByTitle("b")->parent(), GetNodeByTitle("b"));
  EXPECT_EQ("root [a c]", TreeViewContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ(3, GetRowCount());

  // Remove 'c11', shouldn't visually change anything.
  model_.Remove(GetNodeByTitle("c11")->parent(), GetNodeByTitle("c11"));
  EXPECT_EQ("root [a c]", TreeViewContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ(3, GetRowCount());

  // Select 'c1', remove 'c' and make sure selection changes.
  tree_.SetSelectedNode(GetNodeByTitle("c1"));
  EXPECT_EQ("c1", GetSelectedNodeTitle());
  model_.Remove(GetNodeByTitle("c")->parent(), GetNodeByTitle("c"));
  EXPECT_EQ("root [a]", TreeViewContentsAsString());
  EXPECT_EQ("a", GetSelectedNodeTitle());
  EXPECT_EQ(2, GetRowCount());

  // Add 'c1', 'c2', 'c3', select 'c2', remove it and 'c3" should be selected.
  Add(GetNodeByTitle("a"), 0, "c1");
  Add(GetNodeByTitle("a"), 1, "c2");
  Add(GetNodeByTitle("a"), 2, "c3");
  tree_.SetSelectedNode(GetNodeByTitle("c2"));
  model_.Remove(GetNodeByTitle("c2")->parent(), GetNodeByTitle("c2"));
  EXPECT_EQ("root [a [c1 c3]]", TreeViewContentsAsString());
  EXPECT_EQ("c3", GetSelectedNodeTitle());
  EXPECT_EQ(4, GetRowCount());

  // Now delete 'c3' and then 'c1' should be selected.
  model_.Remove(GetNodeByTitle("c3")->parent(), GetNodeByTitle("c3"));
  EXPECT_EQ("root [a [c1]]", TreeViewContentsAsString());
  EXPECT_EQ("c1", GetSelectedNodeTitle());
  EXPECT_EQ(3, GetRowCount());

  // Finally delete 'c1' and then 'a' should be selected.
  model_.Remove(GetNodeByTitle("c1")->parent(), GetNodeByTitle("c1"));
  EXPECT_EQ("root [a]", TreeViewContentsAsString());
  EXPECT_EQ("a", GetSelectedNodeTitle());
  EXPECT_EQ(2, GetRowCount());

  tree_.SetRootShown(false);
  // Add 'b' and 'c', select 'b' and remove it. Selection should change to 'c'.
  Add(GetNodeByTitle("root"), 1, "b");
  Add(GetNodeByTitle("root"), 2, "c");
  tree_.SetSelectedNode(GetNodeByTitle("b"));
  model_.Remove(GetNodeByTitle("b")->parent(), GetNodeByTitle("b"));
  EXPECT_EQ("root [a c]", TreeViewContentsAsString());
  EXPECT_EQ("c", GetSelectedNodeTitle());
  EXPECT_EQ(2, GetRowCount());
}

class TestController : public TreeViewController {
 public:
  void OnTreeViewSelectionChanged(TreeView* tree_view) override {
    call_count_++;
  }

  bool CanEdit(TreeView* tree_view, ui::TreeModelNode* node) override {
    return true;
  }

  int selection_change_count() const { return call_count_; }

 private:
  int call_count_ = 0;
};

TEST_F(TreeViewTest, RemovingLastNodeNotifiesSelectionChanged) {
  TestController controller;
  tree_.SetController(&controller);
  tree_.SetRootShown(false);
  tree_.SetModel(&model_);

  // Remove all but one node.
  model_.Remove(GetNodeByTitle("b")->parent(), GetNodeByTitle("b"));
  model_.Remove(GetNodeByTitle("c")->parent(), GetNodeByTitle("c"));
  tree_.SetSelectedNode(GetNodeByTitle("a"));
  EXPECT_EQ("root [a]", TreeViewContentsAsString());

  const int prior_call_count = controller.selection_change_count();
  // Remove the final node and expect
  // |TestController::OnTreeViewSelectionChanged| to be called.
  model_.Remove(GetNodeByTitle("a")->parent(), GetNodeByTitle("a"));
  EXPECT_EQ(prior_call_count + 1, controller.selection_change_count());
}

// Verifies changing a node title works.
TEST_F(TreeViewTest, TreeNodeChanged) {
  // Add c1 as a child of c and c11 as a child of c1.
  Add(Add(GetNodeByTitle("c"), 0, "c1"), 0, "c11");
  tree_.SetModel(&model_);

  // Change c11, shouldn't do anything.
  model_.SetTitle(GetNodeByTitle("c11"), ASCIIToUTF16("c11.new"));
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ(4, GetRowCount());

  // Change 'b1', shouldn't do anything.
  model_.SetTitle(GetNodeByTitle("b1"), ASCIIToUTF16("b1.new"));
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ(4, GetRowCount());

  // Change 'b'.
  model_.SetTitle(GetNodeByTitle("b"), ASCIIToUTF16("b.new"));
  EXPECT_EQ("root [a b.new c]", TreeViewContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());
  EXPECT_EQ(4, GetRowCount());
}

// Verifies IncrementSelection() works.
TEST_F(TreeViewTest, IncrementSelection) {
  tree_.SetModel(&model_);

  IncrementSelection(true);
  EXPECT_EQ("a", GetSelectedNodeTitle());
  IncrementSelection(true);
  EXPECT_EQ("b", GetSelectedNodeTitle());
  IncrementSelection(true);
  tree_.Expand(GetNodeByTitle("b"));
  IncrementSelection(false);
  EXPECT_EQ("b1", GetSelectedNodeTitle());
  IncrementSelection(true);
  EXPECT_EQ("c", GetSelectedNodeTitle());
  IncrementSelection(true);
  EXPECT_EQ("c", GetSelectedNodeTitle());

  tree_.SetRootShown(false);
  tree_.SetSelectedNode(GetNodeByTitle("a"));
  EXPECT_EQ("a", GetSelectedNodeTitle());
  IncrementSelection(false);
  EXPECT_EQ("a", GetSelectedNodeTitle());
}

// Verifies CollapseOrSelectParent works.
TEST_F(TreeViewTest, CollapseOrSelectParent) {
  tree_.SetModel(&model_);

  tree_.SetSelectedNode(GetNodeByTitle("root"));
  CollapseOrSelectParent();
  EXPECT_EQ("root", TreeViewContentsAsString());
  EXPECT_EQ("root", GetSelectedNodeTitle());

  // Hide the root, which should implicitly expand the root.
  tree_.SetRootShown(false);
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("a", GetSelectedNodeTitle());

  tree_.SetSelectedNode(GetNodeByTitle("b1"));
  EXPECT_EQ("root [a b [b1] c]", TreeViewContentsAsString());
  EXPECT_EQ("b1", GetSelectedNodeTitle());
  CollapseOrSelectParent();
  EXPECT_EQ("root [a b [b1] c]", TreeViewContentsAsString());
  EXPECT_EQ("b", GetSelectedNodeTitle());
  CollapseOrSelectParent();
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("b", GetSelectedNodeTitle());
}

// Verifies ExpandOrSelectChild works.
TEST_F(TreeViewTest, ExpandOrSelectChild) {
  tree_.SetModel(&model_);

  tree_.SetSelectedNode(GetNodeByTitle("root"));
  ExpandOrSelectChild();
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("a", GetSelectedNodeTitle());

  ExpandOrSelectChild();
  EXPECT_EQ("root [a b c]", TreeViewContentsAsString());
  EXPECT_EQ("a", GetSelectedNodeTitle());

  tree_.SetSelectedNode(GetNodeByTitle("b"));
  ExpandOrSelectChild();
  EXPECT_EQ("root [a b [b1] c]", TreeViewContentsAsString());
  EXPECT_EQ("b", GetSelectedNodeTitle());
  ExpandOrSelectChild();
  EXPECT_EQ("root [a b [b1] c]", TreeViewContentsAsString());
  EXPECT_EQ("b1", GetSelectedNodeTitle());
  ExpandOrSelectChild();
  EXPECT_EQ("root [a b [b1] c]", TreeViewContentsAsString());
  EXPECT_EQ("b1", GetSelectedNodeTitle());
}

// Verify selection is properly updated on each keystroke.
TEST_F(TreeViewTest, SelectOnKeyStroke) {
  tree_.SetModel(&model_);
  tree_.ExpandAll(model_.GetRoot());
  selector()->InsertText(ASCIIToUTF16("b"));
  EXPECT_EQ("b", GetSelectedNodeTitle());
  selector()->InsertText(ASCIIToUTF16("1"));
  EXPECT_EQ("b1", GetSelectedNodeTitle());

  // Invoke OnViewBlur() to reset time.
  selector()->OnViewBlur();
  selector()->InsertText(ASCIIToUTF16("z"));
  EXPECT_EQ("b1", GetSelectedNodeTitle());

  selector()->OnViewBlur();
  selector()->InsertText(ASCIIToUTF16("a"));
  EXPECT_EQ("a", GetSelectedNodeTitle());
}

// Verifies edits are committed when focus is lost.
TEST_F(TreeViewTest, CommitOnFocusLost) {
  tree_.SetModel(&model_);

  tree_.SetSelectedNode(GetNodeByTitle("root"));
  ExpandOrSelectChild();
  tree_.SetEditable(true);
  tree_.StartEditing(GetNodeByTitle("a"));
  tree_.editor()->SetText(ASCIIToUTF16("a changed"));
  tree_.OnDidChangeFocus(nullptr, nullptr);
  EXPECT_TRUE(GetNodeByTitle("a changed") != nullptr);
}

}  // namespace views
