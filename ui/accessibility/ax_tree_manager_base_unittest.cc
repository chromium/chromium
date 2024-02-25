// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree_manager_base.h"

#include <memory>
#include <optional>
#include <utility>

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_observer.h"
#include "ui/accessibility/ax_tree_serializer.h"

namespace ui {

namespace {

class AXTreeManagerBaseTest : public ::testing::Test {
 public:
  AXTreeManagerBaseTest();
  AXTreeManagerBaseTest(const AXTreeManagerBaseTest&) = delete;
  AXTreeManagerBaseTest& operator=(const AXTreeManagerBaseTest&) = delete;
  ~AXTreeManagerBaseTest() override = default;

 protected:
  static constexpr AXNodeID kIframeID = 4;

  static AXTreeUpdate CreateSimpleTreeUpdate();
  static std::unique_ptr<AXTree> CreateSimpleTree();
  static std::unique_ptr<AXTree> CreateComplexTree();
  // Modifies `host_node` and  its owning tree, as well as `child_tree`, in
  // order to connect the two trees in a parent - child relationship.
  static void HostChildTreeAtNode(AXNode& host_node, AXTree& child_tree);

  void SetUp() override;

  AXTreeManagerBase empty_manager_;
  AXTreeID simple_tree_id_;
  AXTreeManagerBase simple_manager_;
  AXTreeID complex_tree_id_;
  AXTreeManagerBase complex_manager_;
};

AXTreeManagerBaseTest::AXTreeManagerBaseTest()
    : simple_manager_(CreateSimpleTree()),
      complex_manager_(CreateComplexTree()) {}

// static
AXTreeUpdate AXTreeManagerBaseTest::CreateSimpleTreeUpdate() {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  AXTreeUpdate update;
  update.root_id = root.id;
  update.nodes = {root};
  update.has_tree_data = true;
  AXTreeData tree_data;
  tree_data.tree_id = AXTreeID::CreateNewAXTreeID();
  update.tree_data = tree_data;
  return update;
}

// static
std::unique_ptr<AXTree> AXTreeManagerBaseTest::CreateSimpleTree() {
  return std::make_unique<AXTree>(CreateSimpleTreeUpdate());
}

// static
std::unique_ptr<AXTree> AXTreeManagerBaseTest::CreateComplexTree() {
  AXNodeData root;
  AXNodeData generic_container_ignored;
  AXNodeData paragraph;
  AXNodeData iframe;

  root.id = 1;
  generic_container_ignored.id = 2;
  paragraph.id = 3;
  iframe.id = kIframeID;

  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {generic_container_ignored.id};

  generic_container_ignored.role = ax::mojom::Role::kGenericContainer;
  generic_container_ignored.AddState(ax::mojom::State::kIgnored);
  generic_container_ignored.child_ids = {paragraph.id, iframe.id};

  iframe.role = ax::mojom::Role::kIframe;

  AXTreeUpdate update;
  update.root_id = root.id;
  update.nodes = {root, generic_container_ignored, paragraph, iframe};
  update.has_tree_data = true;

  AXTreeData tree_data;
  tree_data.tree_id = AXTreeID::CreateNewAXTreeID();
  tree_data.title = "Application";
  update.tree_data = tree_data;

  return std::make_unique<AXTree>(update);
}

// static
void AXTreeManagerBaseTest::HostChildTreeAtNode(AXNode& host_node,
                                            AXTree& child_tree) {
  ASSERT_NE(nullptr, host_node.tree());

  {
    AXNodeData host_node_data = host_node.data();
    ASSERT_NE(ax::mojom::AXTreeIDType::kUnknown,
              child_tree.GetAXTreeID().type());
    host_node_data.AddChildTreeId(child_tree.GetAXTreeID());
    AXTreeUpdate update;
    update.nodes = {host_node_data};
    AXTree* parent_tree = static_cast<AXTree*>(host_node.tree());
    ASSERT_TRUE(parent_tree->Unserialize(update)) << parent_tree->error();
  }

  {
    AXTreeData tree_data = child_tree.data();
    ASSERT_NE(ax::mojom::AXTreeIDType::kUnknown,
              host_node.tree()->GetAXTreeID().type());
    tree_data.parent_tree_id = host_node.tree()->GetAXTreeID();
    AXTreeUpdate update;
    update.has_tree_data = true;
    update.tree_data = tree_data;
    ASSERT_TRUE(child_tree.Unserialize(update)) << child_tree.error();
  }
}

void AXTreeManagerBaseTest::SetUp() {
  ASSERT_EQ(AXTreeIDUnknown(), empty_manager_.GetTreeID());
  simple_tree_id_ = simple_manager_.GetTreeID();
  ASSERT_NE(ax::mojom::AXTreeIDType::kUnknown, simple_tree_id_.type());
  complex_tree_id_ = complex_manager_.GetTreeID();
  ASSERT_NE(ax::mojom::AXTreeIDType::kUnknown, complex_tree_id_.type());
}

class TestAXTreeObserver final : public AXTreeObserver {
 public:
  TestAXTreeObserver() = default;
  ~TestAXTreeObserver() override = default;
  TestAXTreeObserver(const TestAXTreeObserver&) = delete;
  TestAXTreeObserver& operator=(const TestAXTreeObserver&) = delete;

  void OnTreeManagerWillBeRemoved(AXTreeID previous_tree_id) override {
    ++manager_remove_count_;
    previous_tree_id_ = previous_tree_id;
  }

  int manager_remove_count() const { return manager_remove_count_; }

  const AXTreeID& previous_tree_id() const { return previous_tree_id_; }

 private:
  int manager_remove_count_ = 0;
  AXTreeID previous_tree_id_;
};

}  // namespace

TEST_F(AXTreeManagerBaseTest, GetManager) {
  // Since the following two trees are destroyed when their respective managers
  // are destructed, we cannot use a reference to their tree IDs. We should copy
  // the tree IDs by value if we want to verify that the managers have indeed
  // been destructed.
  std::unique_ptr<AXTree> simple_tree = CreateSimpleTree();
  const AXTreeID simple_tree_id = simple_tree->GetAXTreeID();

  std::unique_ptr<AXTree> complex_tree = CreateComplexTree();
  const AXTreeID complex_tree_id = complex_tree->GetAXTreeID();

  {
    const AXTreeManagerBase simple_manager(std::move(simple_tree));
    const AXTreeManagerBase complex_manager(std::move(complex_tree));

    ASSERT_NE(nullptr, AXTreeManagerBase::GetManager(simple_tree_id));
    EXPECT_EQ(&simple_manager, AXTreeManagerBase::GetManager(simple_tree_id));

    ASSERT_NE(nullptr, AXTreeManagerBase::GetManager(complex_tree_id));
    EXPECT_EQ(&complex_manager, AXTreeManagerBase::GetManager(complex_tree_id));
  }

  ASSERT_EQ(nullptr, AXTreeManagerBase::GetManager(simple_tree_id));
  ASSERT_EQ(nullptr, AXTreeManagerBase::GetManager(complex_tree_id));
}

TEST_F(AXTreeManagerBaseTest, MoveConstructor) {
  AXTreeManagerBase new_manager(std::move(simple_manager_));
  EXPECT_EQ(simple_tree_id_, new_manager.GetTreeID());
  EXPECT_NE(nullptr, new_manager.GetTree());
  EXPECT_EQ(AXTreeIDUnknown(), simple_manager_.GetTreeID());
  EXPECT_EQ(nullptr, simple_manager_.GetTree());

  new_manager = std::move(complex_manager_);
  EXPECT_EQ(complex_tree_id_, new_manager.GetTreeID());
  EXPECT_NE(nullptr, new_manager.GetTree());
  EXPECT_EQ(AXTreeIDUnknown(), complex_manager_.GetTreeID());
  EXPECT_EQ(nullptr, complex_manager_.GetTree());

  empty_manager_ = std::move(new_manager);
  EXPECT_EQ(complex_tree_id_, empty_manager_.GetTreeID());
  EXPECT_NE(nullptr, empty_manager_.GetTree());
  EXPECT_EQ(AXTreeIDUnknown(), new_manager.GetTreeID());
  EXPECT_EQ(nullptr, new_manager.GetTree());
}

TEST_F(AXTreeManagerBaseTest, SetTree) {
  // Try setting a new tree on construction via an `AXTreeUpdate`.
  const AXTreeUpdate initial_state = CreateSimpleTreeUpdate();
  const AXTreeID& initial_tree_id = initial_state.tree_data.tree_id;
  const AXTreeManagerBase initial_manager(initial_state);
  EXPECT_EQ(initial_tree_id, initial_manager.GetTreeID());

  std::unique_ptr<AXTree> new_tree = CreateSimpleTree();
  const AXTreeID& new_tree_id = new_tree->GetAXTreeID();
  std::unique_ptr<AXTree> old_tree =
      simple_manager_.SetTree(std::move(new_tree));

  ASSERT_NE(nullptr, old_tree.get());
  ASSERT_EQ(nullptr, new_tree.get());

  EXPECT_EQ(simple_tree_id_, old_tree->GetAXTreeID());
  EXPECT_EQ(new_tree_id, simple_manager_.GetTreeID());

  new_tree = simple_manager_.SetTree(initial_state);
  ASSERT_NE(nullptr, new_tree.get());
  EXPECT_EQ(new_tree_id, new_tree->GetAXTreeID());
  EXPECT_EQ(initial_tree_id, initial_manager.GetTreeID());
}

TEST_F(AXTreeManagerBaseTest, ReleaseTree) {
  std::unique_ptr<AXTree> simple_tree = simple_manager_.ReleaseTree();
  EXPECT_EQ(AXTreeIDUnknown(), simple_manager_.GetTreeID());
  EXPECT_EQ(nullptr, simple_manager_.GetTree());
  ASSERT_NE(nullptr, simple_tree.get());
  EXPECT_EQ(simple_tree_id_, simple_tree->GetAXTreeID());
}

TEST_F(AXTreeManagerBaseTest, GetNode) {
  EXPECT_EQ(simple_manager_.GetRoot(), AXTreeManagerBase::GetNodeFromTree(
                                           simple_tree_id_, /* AXNodeID */ 1));
  EXPECT_EQ(
      complex_manager_.GetRoot(),
      AXTreeManagerBase::GetNodeFromTree(complex_tree_id_, /* AXNodeID */ 1));
  EXPECT_EQ(simple_manager_.GetRoot(),
            simple_manager_.GetNode(/* AXNodeID */ 1));

  AXNode* iframe =
      AXTreeManagerBase::GetNodeFromTree(complex_tree_id_, kIframeID);
  ASSERT_NE(nullptr, iframe);
  EXPECT_EQ(kIframeID, iframe->id());
}

TEST_F(AXTreeManagerBaseTest, ParentChildTreeRelationship) {
  EXPECT_EQ(nullptr, empty_manager_.GetRoot());
  EXPECT_EQ(nullptr, empty_manager_.GetHostNode());

  AXNode* iframe =
      AXTreeManagerBase::GetNodeFromTree(complex_tree_id_, kIframeID);
  ASSERT_NE(nullptr, iframe);
  const AXNode* simple_manager_root = simple_manager_.GetTree()->root();
  ASSERT_NE(nullptr, simple_manager_root);

  HostChildTreeAtNode(*iframe, *simple_manager_.GetTree());

  EXPECT_EQ(complex_tree_id_, simple_manager_.GetParentTreeID());
  EXPECT_EQ(nullptr, complex_manager_.GetHostNode());
  EXPECT_EQ(iframe, simple_manager_.GetHostNode());
  EXPECT_EQ(simple_manager_root,
            complex_manager_.GetRootOfChildTree(kIframeID));
  EXPECT_EQ(simple_manager_root, complex_manager_.GetRootOfChildTree(*iframe));
}

TEST_F(AXTreeManagerBaseTest, AttachingAndDetachingChildTrees) {
  AXNode* iframe =
      AXTreeManagerBase::GetNodeFromTree(complex_tree_id_, kIframeID);
  ASSERT_NE(nullptr, iframe);
  AXNode* root = complex_manager_.GetTree()->root();
  ASSERT_NE(nullptr, root);
  const AXNode* child_root = simple_manager_.GetTree()->root();
  ASSERT_NE(nullptr, child_root);

  EXPECT_FALSE(complex_manager_.AttachChildTree(root->id(), simple_manager_))
      << "This particular rootnode is not a leaf node.";
  EXPECT_FALSE(complex_manager_.AttachChildTree(*root, simple_manager_))
      << "This particular rootnode is not a leaf node.";

  EXPECT_FALSE(complex_manager_.AttachChildTree(kIframeID, empty_manager_))
      << "Cannot attach an empty tree to any node.";
  EXPECT_FALSE(complex_manager_.AttachChildTree(*iframe, empty_manager_))
      << "Cannot attach an empty tree to any node.";

  EXPECT_TRUE(complex_manager_.AttachChildTree(kIframeID, simple_manager_));
  EXPECT_EQ(iframe, simple_manager_.GetHostNode());
  EXPECT_EQ(child_root, complex_manager_.GetRootOfChildTree(kIframeID));
  EXPECT_EQ(&simple_manager_, complex_manager_.DetachChildTree(*iframe));
  EXPECT_EQ(nullptr, simple_manager_.GetHostNode());
  EXPECT_EQ(nullptr, complex_manager_.GetRootOfChildTree(*iframe));

  EXPECT_TRUE(complex_manager_.AttachChildTree(*iframe, simple_manager_));
  EXPECT_EQ(iframe, simple_manager_.GetHostNode());
  EXPECT_EQ(child_root, complex_manager_.GetRootOfChildTree(*iframe));
  EXPECT_EQ(&simple_manager_, complex_manager_.DetachChildTree(kIframeID));
  EXPECT_EQ(nullptr, simple_manager_.GetHostNode());
  EXPECT_EQ(nullptr, complex_manager_.GetRootOfChildTree(kIframeID));

  std::optional<AXTreeManagerBase> child_manager =
      complex_manager_.AttachChildTree(*iframe, CreateSimpleTreeUpdate());
  ASSERT_TRUE(child_manager.has_value());
  EXPECT_NE(nullptr, child_manager->GetTree());
  EXPECT_EQ(&(child_manager.value()),
            complex_manager_.DetachChildTree(*iframe));
  child_manager =
      complex_manager_.AttachChildTree(kIframeID, CreateSimpleTreeUpdate());
  EXPECT_NE(nullptr, child_manager->GetTree());
  ASSERT_TRUE(child_manager.has_value());
  ;
  EXPECT_EQ(&(child_manager.value()),
            complex_manager_.DetachChildTree(kIframeID));
}

TEST_F(AXTreeManagerBaseTest, Observers) {
  TestAXTreeObserver observer;
  simple_manager_.GetTree()->AddObserver(&observer);
  EXPECT_TRUE(simple_manager_.GetTree()->HasObserver(&observer));
  EXPECT_FALSE(complex_manager_.GetTree()->HasObserver(&observer));

  std::unique_ptr<AXTree> new_tree = CreateSimpleTree();
  // `new_tree_id` should be copied by value and not accessed by reference,
  // because `new_tree` will be destructed before we access `new_tree_id`.
  const AXTreeID new_tree_id = new_tree->GetAXTreeID();
  simple_manager_.SetTree(std::move(new_tree));

  EXPECT_EQ(1, observer.manager_remove_count());
  EXPECT_EQ(simple_tree_id_, observer.previous_tree_id());

  simple_manager_.GetTree()->AddObserver(&observer);
  simple_manager_.ReleaseTree();
  EXPECT_EQ(2, observer.manager_remove_count());
  EXPECT_EQ(new_tree_id, observer.previous_tree_id());

  simple_manager_ = std::move(complex_manager_);
  EXPECT_EQ(2, observer.manager_remove_count())
      << "Tree must have already been destroyed.";
  EXPECT_EQ(new_tree_id, observer.previous_tree_id());
}

}  // namespace ui
