// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/browser_accessibility_manager_auralinux.h"

#include <atk/atk.h>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_updates_and_events.h"
#include "ui/accessibility/platform/ax_platform_for_test.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_auralinux.h"
#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/accessibility/platform/browser_accessibility_auralinux.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/accessibility/platform/test_ax_node_id_delegate.h"
#include "ui/accessibility/platform/test_ax_platform_tree_manager_delegate.h"
#include "ui/base/glib/scoped_gsignal.h"

namespace ui {

class BrowserAccessibilityManagerAuraLinuxTest : public ::testing::Test {
 public:
  BrowserAccessibilityManagerAuraLinuxTest();

  BrowserAccessibilityManagerAuraLinuxTest(
      const BrowserAccessibilityManagerAuraLinuxTest&) = delete;
  BrowserAccessibilityManagerAuraLinuxTest& operator=(
      const BrowserAccessibilityManagerAuraLinuxTest&) = delete;

  ~BrowserAccessibilityManagerAuraLinuxTest() override = default;

 protected:
  std::unique_ptr<TestAXPlatformTreeManagerDelegate>
      test_browser_accessibility_delegate_;
  TestAXNodeIdDelegate node_id_delegate_;

 private:
  ScopedAXModeSetter ax_mode_setter;
  void SetUp() override;

  // See crbug.com/1349124
  base::test::SingleThreadTaskEnvironment task_environment_;
};

BrowserAccessibilityManagerAuraLinuxTest::BrowserAccessibilityManagerAuraLinuxTest()
  : ax_mode_setter(kAXModeComplete) {}

void BrowserAccessibilityManagerAuraLinuxTest::SetUp() {
  test_browser_accessibility_delegate_ =
      std::make_unique<TestAXPlatformTreeManagerDelegate>();
}

TEST_F(BrowserAccessibilityManagerAuraLinuxTest, TestEmitChildrenChanged) {
  AXTreeUpdate initial_state;
  AXTreeID tree_id = AXTreeID::CreateNewAXTreeID();
  initial_state.tree_data.tree_id = tree_id;
  initial_state.has_tree_data = true;
  initial_state.tree_data.loaded = true;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids = {2};
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].child_ids = {3};
  initial_state.nodes[1].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kInlineTextBox;

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          initial_state, node_id_delegate_,
          test_browser_accessibility_delegate_.get()));

  AtkObject* atk_root =
      manager->GetBrowserAccessibilityRoot()->GetNativeViewAccessible();
  AXPlatformNodeAuraLinux* root_document_root_node =
      static_cast<AXPlatformNodeAuraLinux*>(
          AXPlatformNode::FromNativeViewAccessible(atk_root));
  {
    ScopedGSignal signal1(
        atk_root, "children-changed::remove",
        base::BindRepeating(
            +[](AtkObject* obj, int index, gpointer child, gpointer user_data) {
              EXPECT_EQ(ATK_ROLE_DOCUMENT_WEB, atk_object_get_role(obj));
            }));
    EXPECT_TRUE(signal1.Connected());
    ScopedGSignal signal2(
        atk_root, "children-changed::add",
        base::BindRepeating(
            +[](AtkObject* obj, int index, gpointer child, gpointer user_data) {
              EXPECT_EQ(ATK_ROLE_DOCUMENT_WEB, atk_object_get_role(obj));
            }));
    EXPECT_TRUE(signal2.Connected());
  }
  BrowserAccessibility* static_text_accessible =
      manager->GetBrowserAccessibilityRoot()->PlatformGetChild(0);
  // StaticText node triggers 'children-changed' event to the parent,
  // ATK_ROLE_DOCUMENT_WEB, when subtree is changed.
  BrowserAccessibilityManagerAuraLinux* aura_linux_manager =
      manager->ToBrowserAccessibilityManagerAuraLinux();
  aura_linux_manager->FireSubtreeCreatedEvent(static_text_accessible);
  aura_linux_manager->OnSubtreeWillBeDeleted(manager->ax_tree(),
                                             static_text_accessible->node());

  AtkObject* atk_object = root_document_root_node->ChildAtIndex(0);
  {
    ScopedGSignal signal3(
        atk_object, "children-changed::remove",
        base::BindRepeating(
            +[](AtkObject*, int index, gpointer child, gpointer user_data) {
              EXPECT_TRUE(false) << "should not be reached.";
            }));
    EXPECT_TRUE(signal3.Connected());
    ScopedGSignal signal4(
        atk_object, "children-changed::add",
        base::BindRepeating(
            +[](AtkObject* obj, int index, gpointer child, gpointer user_data) {
              EXPECT_TRUE(false) << "should not be reached.";
            }));
    EXPECT_TRUE(signal4.Connected());
  }

  // The static text is a platform leaf.
  ASSERT_EQ(0U, static_text_accessible->PlatformChildCount());
  ASSERT_EQ(1U, static_text_accessible->InternalChildCount());

  BrowserAccessibility* inline_text_accessible =
      static_text_accessible->InternalGetChild(0);
  // PlatformLeaf node such as InlineText should not trigger
  // 'children-changed' event to the parent when subtree is changed.
  aura_linux_manager->FireSubtreeCreatedEvent(inline_text_accessible);
  aura_linux_manager->OnSubtreeWillBeDeleted(manager->ax_tree(),
                                             inline_text_accessible->node());
}

TEST_F(BrowserAccessibilityManagerAuraLinuxTest,
       FireSelectedEventOnUnselectedNode) {
  AXTreeUpdate initial_state;
  AXTreeID tree_id = AXTreeID::CreateNewAXTreeID();
  initial_state.tree_data.tree_id = tree_id;
  initial_state.has_tree_data = true;
  initial_state.tree_data.loaded = true;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids = {2, 3};
  initial_state.nodes[0].role = ax::mojom::Role::kListBox;
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kListBoxOption;
  initial_state.nodes[1].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                                          false);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kListBoxOption;

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          initial_state, node_id_delegate_,
          test_browser_accessibility_delegate_.get()));

  BrowserAccessibility* option =
      manager->GetBrowserAccessibilityRoot()->PlatformGetChild(0);
  ASSERT_FALSE(option->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  AtkObject* option_atk_object = option->GetNativeViewAccessible();
  ASSERT_TRUE(ATK_IS_OBJECT(option_atk_object));

  bool saw_unselected = false;
  ScopedGSignal selected_signal(
      option_atk_object, "state-change",
      base::BindRepeating(
          +[](bool* saw_unselected, AtkObject*, gchar* state_changed,
              gboolean new_value) {
            if (!g_strcmp0(state_changed, "selected") && !new_value) {
              *saw_unselected = true;
            }
          },
          &saw_unselected));
  ASSERT_TRUE(selected_signal.Connected());

  BrowserAccessibilityManagerAuraLinux* aura_linux_manager =
      manager->ToBrowserAccessibilityManagerAuraLinux();
  aura_linux_manager->FireSelectedEvent(option);
  EXPECT_TRUE(saw_unselected);

  BrowserAccessibility* option_without_selected_state =
      manager->GetBrowserAccessibilityRoot()->PlatformGetChild(1);
  ASSERT_FALSE(option_without_selected_state->HasBoolAttribute(
      ax::mojom::BoolAttribute::kSelected));
  AtkObject* option_without_selected_state_atk_object =
      option_without_selected_state->GetNativeViewAccessible();
  ASSERT_TRUE(ATK_IS_OBJECT(option_without_selected_state_atk_object));

  bool saw_selected = false;
  ScopedGSignal no_selected_state_signal(
      option_without_selected_state_atk_object, "state-change",
      base::BindRepeating(
          +[](bool* saw_selected, AtkObject*, gchar* state_changed,
              gboolean new_value) {
            if (!g_strcmp0(state_changed, "selected") && new_value) {
              *saw_selected = true;
            }
          },
          &saw_selected));
  ASSERT_TRUE(no_selected_state_signal.Connected());

  aura_linux_manager->FireSelectedEvent(option_without_selected_state);
  EXPECT_FALSE(saw_selected);
}

namespace {

// Lets a test choose whether a node owns a platform node. See
// BrowserAccessibility::ShouldHavePlatformNode for details.
class TogglablePlatformNodeBrowserAccessibilityAuraLinux
    : public BrowserAccessibilityAuraLinux {
 public:
  TogglablePlatformNodeBrowserAccessibilityAuraLinux(
      BrowserAccessibilityManager* manager,
      AXNode* node)
      : BrowserAccessibilityAuraLinux(manager, node) {}

  bool ShouldHavePlatformNode() const override { return should_have_; }

  void SetShouldHavePlatformNode(bool value) {
    should_have_ = value;
    UpdatePlatformNode();
  }

 private:
  bool should_have_ = true;
};

}  // namespace

class BrowserAccessibilityPlatformNodeAuraLinuxTest
    : public BrowserAccessibilityManagerAuraLinuxTest {
 protected:
  std::unique_ptr<TogglablePlatformNodeBrowserAccessibilityAuraLinux>
  MakeNode() {
    AXNodeData root;
    root.id = 1;
    root.role = ax::mojom::Role::kRootWebArea;
    root.child_ids = {2};

    // Only an ignored node may go without a platform node.
    AXNodeData ignored_child;
    ignored_child.id = 2;
    ignored_child.role = ax::mojom::Role::kGenericContainer;
    ignored_child.AddState(ax::mojom::State::kIgnored);
    ignored_child.child_ids = {3};

    // A node below the ignored one keeps the root off the bottom of the tree.
    // ATK gives no object to the child of a leaf.
    AXNodeData grandchild;
    grandchild.id = 3;
    grandchild.role = ax::mojom::Role::kButton;

    manager_.reset(BrowserAccessibilityManager::Create(
        MakeAXTreeUpdateForTesting(root, ignored_child, grandchild),
        node_id_delegate_, test_browser_accessibility_delegate_.get()));
    auto node = std::make_unique<
        TogglablePlatformNodeBrowserAccessibilityAuraLinux>(
        manager_.get(), manager_->GetFromID(2)->node());
    // The manager does this for every wrapper that it creates.
    node->OnDataChanged();
    return node;
  }

  std::unique_ptr<BrowserAccessibilityManager> manager_;
};

TEST_F(BrowserAccessibilityPlatformNodeAuraLinuxTest,
       ANodeOwnsAPlatformNodeByDefault) {
  std::unique_ptr<TogglablePlatformNodeBrowserAccessibilityAuraLinux> node =
      MakeNode();

  EXPECT_TRUE(node->ShouldHavePlatformNode());
  EXPECT_TRUE(node->GetAXPlatformNode());
  EXPECT_TRUE(node->GetNativeViewAccessible());
}

TEST_F(BrowserAccessibilityPlatformNodeAuraLinuxTest,
       ANodeCanOwnNoPlatformNode) {
  std::unique_ptr<TogglablePlatformNodeBrowserAccessibilityAuraLinux> node =
      MakeNode();
  node->SetShouldHavePlatformNode(false);

  EXPECT_FALSE(node->GetAXPlatformNode());
  EXPECT_FALSE(node->GetNativeViewAccessible());
}

TEST_F(BrowserAccessibilityPlatformNodeAuraLinuxTest,
       EveryAccessorIsSafeWithNoPlatformNode) {
  std::unique_ptr<TogglablePlatformNodeBrowserAccessibilityAuraLinux> node =
      MakeNode();
  node->SetShouldHavePlatformNode(false);

  EXPECT_TRUE(node->GetHypertext().empty());
  EXPECT_TRUE(node->ComputeTextAttributes().empty());
  node->UpdatePlatformAttributes();
  node->OnDataChanged();
}

TEST_F(BrowserAccessibilityPlatformNodeAuraLinuxTest, APlatformNodeComesBack) {
  std::unique_ptr<TogglablePlatformNodeBrowserAccessibilityAuraLinux> node =
      MakeNode();
  node->SetShouldHavePlatformNode(false);
  ASSERT_FALSE(node->GetAXPlatformNode());

  node->SetShouldHavePlatformNode(true);

  EXPECT_TRUE(node->GetAXPlatformNode());
  EXPECT_TRUE(node->GetNativeViewAccessible());
}

TEST_F(BrowserAccessibilityPlatformNodeAuraLinuxTest, RepeatedChangesAreSafe) {
  std::unique_ptr<TogglablePlatformNodeBrowserAccessibilityAuraLinux> node =
      MakeNode();

  for (int i = 0; i < 3; ++i) {
    node->SetShouldHavePlatformNode(false);
    ASSERT_FALSE(node->GetAXPlatformNode()) << "iteration " << i;

    node->SetShouldHavePlatformNode(true);
    ASSERT_TRUE(node->GetAXPlatformNode()) << "iteration " << i;
  }
}

TEST_F(BrowserAccessibilityPlatformNodeAuraLinuxTest,
       RepeatedUpdatesKeepOnePlatformNode) {
  std::unique_ptr<TogglablePlatformNodeBrowserAccessibilityAuraLinux> node =
      MakeNode();
  AXPlatformNode* first = node->GetAXPlatformNode();

  node->UpdatePlatformNode();
  node->UpdatePlatformNode();

  EXPECT_EQ(first, node->GetAXPlatformNode());
}

// A Views-sourced tree whose WebView node bridges to the web content tree must
// expose the hosted web document in its own place, so ATK sees the same nodes
// it saw before ViewsAX instead of an extra ATK_ROLE_PANEL for the host.
TEST_F(BrowserAccessibilityManagerAuraLinuxTest,
       WebViewHostIsNotInThePlatformTree) {
  // A Views-sourced manager only exists under ViewsAX, and
  // ~BrowserAccessibilityManagerAuraLinux CHECKs that without the feature.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(::features::kAccessibilityTreeForViews);

  AXNodeData child_tree_root;
  child_tree_root.id = 1;
  child_tree_root.role = ax::mojom::Role::kRootWebArea;
  AXTreeUpdate child_tree_update = MakeAXTreeUpdateForTesting(child_tree_root);

  AXNodeData views_root;
  views_root.id = 1;
  views_root.role = ax::mojom::Role::kWindow;
  views_root.child_ids = {2, 3};

  AXNodeData toolbar;
  toolbar.id = 2;
  toolbar.role = ax::mojom::Role::kToolbar;

  AXNodeData web_view;
  web_view.id = 3;
  web_view.role = ax::mojom::Role::kWebView;
  web_view.AddChildTreeId(child_tree_update.tree_data.tree_id);
  web_view.AddState(ax::mojom::State::kIgnored);

  AXTreeUpdate views_update =
      MakeAXTreeUpdateForTesting(views_root, toolbar, web_view);
  child_tree_update.tree_data.parent_tree_id = views_update.tree_data.tree_id;

  test_browser_accessibility_delegate_->is_web_content_source_ = false;
  std::unique_ptr<BrowserAccessibilityManager> views_manager(
      BrowserAccessibilityManager::Create(
          views_update, node_id_delegate_,
          test_browser_accessibility_delegate_.get()));

  std::unique_ptr<TestAXPlatformTreeManagerDelegate> web_delegate =
      std::make_unique<TestAXPlatformTreeManagerDelegate>();
  web_delegate->is_root_frame_ = false;
  std::unique_ptr<BrowserAccessibilityManager> web_manager(
      BrowserAccessibilityManager::Create(child_tree_update, node_id_delegate_,
                                          web_delegate.get()));


  // The renderer's first event batch is what connects the hosted tree to its
  // host.
  AXUpdatesAndEvents bundle;
  bundle.updates.resize(1);
  bundle.updates[0].nodes.push_back(child_tree_root);
  ASSERT_TRUE(web_manager->OnAccessibilityEvents(bundle));

  BrowserAccessibility* views_root_node =
      views_manager->GetBrowserAccessibilityRoot();
  BrowserAccessibility* web_root_node =
      web_manager->GetBrowserAccessibilityRoot();

  AtkObject* views_root_atk_object = views_root_node->GetNativeViewAccessible();
  ASSERT_TRUE(ATK_IS_OBJECT(views_root_atk_object));
  EXPECT_EQ(2, atk_object_get_n_accessible_children(views_root_atk_object));

  AtkObject* hosted = atk_object_ref_accessible_child(views_root_atk_object, 1);
  ASSERT_TRUE(ATK_IS_OBJECT(hosted));
  EXPECT_EQ(web_root_node->GetNativeViewAccessible(), hosted);
  EXPECT_EQ(ATK_ROLE_DOCUMENT_WEB, atk_object_get_role(hosted));
  g_object_unref(hosted);

  // The hosted root takes the place of its host, thus it walks back to the
  // siblings of that host and it reports the index of that host.
  BrowserAccessibility* toolbar_node = views_manager->GetFromID(2);
  EXPECT_EQ(toolbar_node, web_root_node->PlatformGetPreviousSibling());
  EXPECT_EQ(0u, toolbar_node->GetIndexInParent());
  EXPECT_EQ(1u, web_root_node->GetIndexInParent());

  // The host holds its child tree ID until the WebView clears it, thus it
  // stays ignored, and the toolbar is the only child that is left.
  web_manager.reset();
  EXPECT_EQ(1, atk_object_get_n_accessible_children(views_root_atk_object));
}

// An ignored host that holds a child tree ID is unreachable through every
// platform accessor, thus it owns no platform node and no ATK object. Whether
// the tree that takes its place is there does not change this.
TEST_F(BrowserAccessibilityManagerAuraLinuxTest,
       WebViewHostOwnsNoPlatformNode) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(::features::kAccessibilityTreeForViews);

  AXNodeData child_tree_root;
  child_tree_root.id = 1;
  child_tree_root.role = ax::mojom::Role::kRootWebArea;
  AXTreeUpdate child_tree_update = MakeAXTreeUpdateForTesting(child_tree_root);

  AXNodeData views_root;
  views_root.id = 1;
  views_root.role = ax::mojom::Role::kWindow;
  views_root.child_ids = {2};

  AXNodeData web_view;
  web_view.id = 2;
  web_view.role = ax::mojom::Role::kWebView;
  web_view.AddChildTreeId(child_tree_update.tree_data.tree_id);
  web_view.AddState(ax::mojom::State::kIgnored);

  AXTreeUpdate views_update = MakeAXTreeUpdateForTesting(views_root, web_view);
  child_tree_update.tree_data.parent_tree_id = views_update.tree_data.tree_id;

  test_browser_accessibility_delegate_->is_web_content_source_ = false;
  std::unique_ptr<BrowserAccessibilityManager> views_manager(
      BrowserAccessibilityManager::Create(
          views_update, node_id_delegate_,
          test_browser_accessibility_delegate_.get()));

  // The host is ignored and holds a child tree ID from the start, thus it owns
  // no platform node even before the tree that takes its place arrives.
  BrowserAccessibility* host_node = views_manager->GetFromID(2);
  ASSERT_FALSE(host_node->GetAXPlatformNode());

  std::unique_ptr<TestAXPlatformTreeManagerDelegate> web_delegate =
      std::make_unique<TestAXPlatformTreeManagerDelegate>();
  web_delegate->is_root_frame_ = false;
  std::unique_ptr<BrowserAccessibilityManager> web_manager(
      BrowserAccessibilityManager::Create(child_tree_update, node_id_delegate_,
                                          web_delegate.get()));

  // The renderer's first event batch is what connects the hosted tree to its
  // host.
  AXUpdatesAndEvents bundle;
  bundle.updates.resize(1);
  bundle.updates[0].nodes.push_back(child_tree_root);
  ASSERT_TRUE(web_manager->OnAccessibilityEvents(bundle));

  EXPECT_FALSE(host_node->GetAXPlatformNode());
  EXPECT_FALSE(ATK_IS_OBJECT(host_node->GetNativeViewAccessible()));

  // The host keeps its child tree ID here, thus it stays ignored and owns no
  // platform node, whether or not the tree that takes its place is there.
  web_manager.reset();

  EXPECT_FALSE(host_node->GetAXPlatformNode());
  EXPECT_FALSE(ATK_IS_OBJECT(host_node->GetNativeViewAccessible()));
}

}  // namespace ui
