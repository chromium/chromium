// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/browser_accessibility_manager.h"

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_updates_and_events.h"
#include "ui/accessibility/platform/ax_fragment_root_delegate_win.h"
#include "ui/accessibility/platform/ax_fragment_root_win.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"
#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/accessibility/platform/browser_accessibility_manager_win.h"
#include "ui/accessibility/platform/browser_accessibility_win.h"
#include "ui/accessibility/platform/test_ax_node_id_delegate.h"
#include "ui/accessibility/platform/test_ax_node_wrapper.h"
#include "ui/accessibility/platform/test_ax_platform_tree_manager_delegate.h"

namespace {

class TestFragmentRootDelegate : public ui::AXFragmentRootDelegateWin {
 public:
  TestFragmentRootDelegate(
      ui::BrowserAccessibilityManager* browser_accessibility_manager)
      : browser_accessibility_manager_(browser_accessibility_manager) {}

  gfx::NativeViewAccessible GetChildOfAXFragmentRoot() override {
    return browser_accessibility_manager_->GetBrowserAccessibilityRoot()
        ->GetNativeViewAccessible();
  }

  gfx::NativeViewAccessible GetParentOfAXFragmentRoot() override {
    return nullptr;
  }

  bool IsAXFragmentRootAControlElement() override { return true; }

  raw_ptr<ui::BrowserAccessibilityManager> browser_accessibility_manager_;
};
}  // namespace

namespace ui {

class BrowserAccessibilityManagerWinTest : public testing::Test {
 public:
  BrowserAccessibilityManagerWinTest() = default;

  BrowserAccessibilityManagerWinTest(
      const BrowserAccessibilityManagerWinTest&) = delete;
  BrowserAccessibilityManagerWinTest& operator=(
      const BrowserAccessibilityManagerWinTest&) = delete;

  ~BrowserAccessibilityManagerWinTest() override = default;

 protected:
  std::unique_ptr<TestAXPlatformTreeManagerDelegate>
      test_browser_accessibility_delegate_;
  TestAXNodeIdDelegate node_id_delegate_;

 private:
  void SetUp() override;

  // This is needed to prevent a DCHECK failure when OnAccessibilityApiUsage
  // is called in BrowserAccessibility::GetRole.
  base::test::SingleThreadTaskEnvironment task_environment_;
};

void BrowserAccessibilityManagerWinTest::SetUp() {
  test_browser_accessibility_delegate_ =
      std::make_unique<TestAXPlatformTreeManagerDelegate>();
}

TEST_F(BrowserAccessibilityManagerWinTest, DynamicallyAddedIFrame) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;

  test_browser_accessibility_delegate_->accelerated_widget_ =
      gfx::kMockAcceleratedWidget;

  std::unique_ptr<BrowserAccessibilityManager> root_manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(root), node_id_delegate_,
          test_browser_accessibility_delegate_.get()));

  TestFragmentRootDelegate test_fragment_root_delegate(root_manager.get());

  AXPlatformNode* root_document_root_node =
      AXPlatformNode::FromNativeViewAccessible(
          root_manager->GetBrowserAccessibilityRoot()
              ->GetNativeViewAccessible());

  std::unique_ptr<AXPlatformNodeDelegate> fragment_root =
      std::make_unique<AXFragmentRootWin>(gfx::kMockAcceleratedWidget,
                                          &test_fragment_root_delegate);

  EXPECT_EQ(fragment_root->GetChildCount(), 1u);
  EXPECT_EQ(fragment_root->ChildAtIndex(0),
            root_document_root_node->GetNativeViewAccessible());

  // Simulate the case where an iframe is created but the update to add the
  // element to the root frame's document has not yet come through.
  std::unique_ptr<TestAXPlatformTreeManagerDelegate> iframe_delegate =
      std::make_unique<TestAXPlatformTreeManagerDelegate>();
  iframe_delegate->is_root_frame_ = false;
  iframe_delegate->accelerated_widget_ = gfx::kMockAcceleratedWidget;

  std::unique_ptr<BrowserAccessibilityManager> iframe_manager(
      BrowserAccessibilityManager::Create(MakeAXTreeUpdateForTesting(root),
                                          node_id_delegate_,
                                          iframe_delegate.get()));

  // The new frame is not a root frame, so the fragment root's lone child should
  // still be the same as before.
  EXPECT_EQ(fragment_root->GetChildCount(), 1u);
  EXPECT_EQ(fragment_root->ChildAtIndex(0),
            root_document_root_node->GetNativeViewAccessible());
}

TEST_F(BrowserAccessibilityManagerWinTest, ChildTree) {
  AXNodeData child_tree_root;
  child_tree_root.id = 1;
  child_tree_root.role = ax::mojom::Role::kRootWebArea;
  AXTreeUpdate child_tree_update = MakeAXTreeUpdateForTesting(child_tree_root);

  AXNodeData parent_tree_root;
  parent_tree_root.id = 1;
  parent_tree_root.role = ax::mojom::Role::kRootWebArea;
  parent_tree_root.AddChildTreeId(child_tree_update.tree_data.tree_id);
  AXTreeUpdate parent_tree_update =
      MakeAXTreeUpdateForTesting(parent_tree_root);

  child_tree_update.tree_data.parent_tree_id =
      parent_tree_update.tree_data.tree_id;

  test_browser_accessibility_delegate_->accelerated_widget_ =
      gfx::kMockAcceleratedWidget;

  std::unique_ptr<BrowserAccessibilityManager> parent_manager(
      BrowserAccessibilityManager::Create(
          parent_tree_update, node_id_delegate_,
          test_browser_accessibility_delegate_.get()));

  TestFragmentRootDelegate test_fragment_root_delegate(parent_manager.get());

  AXPlatformNode* root_document_root_node =
      AXPlatformNode::FromNativeViewAccessible(
          parent_manager->GetBrowserAccessibilityRoot()
              ->GetNativeViewAccessible());

  std::unique_ptr<AXPlatformNodeDelegate> fragment_root =
      std::make_unique<AXFragmentRootWin>(gfx::kMockAcceleratedWidget,
                                          &test_fragment_root_delegate);

  EXPECT_EQ(fragment_root->GetChildCount(), 1u);
  EXPECT_EQ(fragment_root->ChildAtIndex(0),
            root_document_root_node->GetNativeViewAccessible());

  // Add the child tree.
  std::unique_ptr<TestAXPlatformTreeManagerDelegate> child_tree_delegate =
      std::make_unique<TestAXPlatformTreeManagerDelegate>();
  child_tree_delegate->is_root_frame_ = false;
  child_tree_delegate->accelerated_widget_ = gfx::kMockAcceleratedWidget;
  std::unique_ptr<BrowserAccessibilityManager> child_manager(
      BrowserAccessibilityManager::Create(child_tree_update, node_id_delegate_,
                                          child_tree_delegate.get()));

  // The fragment root's lone child should still be the same as before.
  EXPECT_EQ(fragment_root->GetChildCount(), 1u);
  EXPECT_EQ(fragment_root->ChildAtIndex(0),
            root_document_root_node->GetNativeViewAccessible());
}

// Verifies that FireAriaNotificationEvent bypasses the UIA event listener
// optimization when the source is not web content (e.g., Views). UIA only
// calls AdviseEventAdded on fragment roots it has already discovered, so
// transient HWNDs like popup menus have empty listener maps that would
// incorrectly suppress events. See crbug.com/40672441.
TEST_F(BrowserAccessibilityManagerWinTest,
       AriaNotificationSkipsListenerCheckForNonWebContent) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;

  test_browser_accessibility_delegate_->accelerated_widget_ =
      gfx::kMockAcceleratedWidget;
  test_browser_accessibility_delegate_->is_web_content_source_ = false;

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(root), node_id_delegate_,
          test_browser_accessibility_delegate_.get()));

  TestFragmentRootDelegate test_fragment_root_delegate(manager.get());

  std::unique_ptr<AXPlatformNodeDelegate> fragment_root =
      std::make_unique<AXFragmentRootWin>(gfx::kMockAcceleratedWidget,
                                          &test_fragment_root_delegate);

  auto* platform_node = static_cast<AXPlatformNodeWin*>(
      manager->GetBrowserAccessibilityRoot()->GetAXPlatformNode());
  ASSERT_FALSE(
      platform_node->HasEventListenerForEvent(UIA_NotificationEventId));

  manager->FireAriaNotificationEvent(
      manager->GetBrowserAccessibilityRoot(), "test notification",
      ax::mojom::AriaNotificationPriority::kNormal,
      ax::mojom::AriaNotificationInterrupt::kNone, "");
}

TEST_F(BrowserAccessibilityManagerWinTest,
       CheckedStateChangedMapsToExposedPattern) {
  // This test validates that CHECKED_STATE_CHANGED on a menu button
  // (which exposes ExpandCollapse, not Toggle) must raise
  // ExpandCollapseState, not ToggleState.
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;

  // HasPopup=menu exposes ExpandCollapse, not Toggle.
  AXNodeData menu_button;
  menu_button.id = 2;
  menu_button.role = ax::mojom::Role::kButton;
  menu_button.SetHasPopup(ax::mojom::HasPopup::kMenu);

  // An expandable popup button also exposes ExpandCollapse.
  AXNodeData popup_button;
  popup_button.id = 3;
  popup_button.role = ax::mojom::Role::kPopUpButton;
  popup_button.AddState(ax::mojom::State::kCollapsed);

  // Genuine toggle/checkable control: raises ToggleState.
  AXNodeData checkbox;
  checkbox.id = 4;
  checkbox.role = ax::mojom::Role::kCheckBox;

  // A toggle button without a popup exposes Toggle, not ExpandCollapse.
  AXNodeData toggle_button;
  toggle_button.id = 5;
  toggle_button.role = ax::mojom::Role::kToggleButton;

  root.child_ids = {menu_button.id, popup_button.id, checkbox.id,
                    toggle_button.id};

  test_browser_accessibility_delegate_->accelerated_widget_ =
      gfx::kMockAcceleratedWidget;

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(root, menu_button, popup_button, checkbox,
                                     toggle_button),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));

  BrowserAccessibility* menu_button_node = manager->GetFromID(menu_button.id);
  ASSERT_TRUE(menu_button_node);
  BrowserAccessibility* popup_button_node = manager->GetFromID(popup_button.id);
  ASSERT_TRUE(popup_button_node);
  BrowserAccessibility* checkbox_node = manager->GetFromID(checkbox.id);
  ASSERT_TRUE(checkbox_node);
  BrowserAccessibility* toggle_button_node =
      manager->GetFromID(toggle_button.id);
  ASSERT_TRUE(toggle_button_node);

  EXPECT_EQ(BrowserAccessibilityManagerWin::GetCheckedStateChangedUiaProperty(
                *menu_button_node),
            UIA_ExpandCollapseExpandCollapseStatePropertyId);
  EXPECT_EQ(BrowserAccessibilityManagerWin::GetCheckedStateChangedUiaProperty(
                *popup_button_node),
            UIA_ExpandCollapseExpandCollapseStatePropertyId);

  EXPECT_EQ(BrowserAccessibilityManagerWin::GetCheckedStateChangedUiaProperty(
                *checkbox_node),
            UIA_ToggleToggleStatePropertyId);
  EXPECT_EQ(BrowserAccessibilityManagerWin::GetCheckedStateChangedUiaProperty(
                *toggle_button_node),
            UIA_ToggleToggleStatePropertyId);
}

// A Views-sourced tree whose WebView node bridges to the web content tree must
// expose the hosted RootWebArea in its own place, so UIA and MSAA/IA2 see the
// same nodes they saw before ViewsAX.
TEST_F(BrowserAccessibilityManagerWinTest, WebViewHostIsNotInThePlatformTree) {
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
  test_browser_accessibility_delegate_->accelerated_widget_ =
      gfx::kMockAcceleratedWidget;
  std::unique_ptr<BrowserAccessibilityManager> views_manager(
      BrowserAccessibilityManager::Create(
          views_update, node_id_delegate_,
          test_browser_accessibility_delegate_.get()));

  std::unique_ptr<TestAXPlatformTreeManagerDelegate> web_delegate =
      std::make_unique<TestAXPlatformTreeManagerDelegate>();
  web_delegate->is_root_frame_ = false;
  web_delegate->accelerated_widget_ = gfx::kMockAcceleratedWidget;
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
  BrowserAccessibility* toolbar_node = views_manager->GetFromID(2);

  EXPECT_EQ(2u, views_root_node->PlatformChildCount());
  EXPECT_EQ(web_root_node->GetNativeViewAccessible(),
            views_root_node->ChildAtIndex(1));
  EXPECT_EQ(web_root_node->GetNativeViewAccessible(),
            toolbar_node->PlatformGetNextSibling()->GetNativeViewAccessible());
  EXPECT_EQ(ax::mojom::Role::kRootWebArea,
            views_root_node->PlatformGetChild(1)->GetRole());

  // The hosted root takes the place of its host, thus it walks back to the
  // siblings of that host and it reports the index of that host.
  EXPECT_EQ(toolbar_node, web_root_node->PlatformGetPreviousSibling());
  EXPECT_EQ(0u, toolbar_node->GetIndexInParent());
  EXPECT_EQ(1u, web_root_node->GetIndexInParent());

  // The host holds its child tree ID until the WebView clears it, thus it
  // stays ignored, and the toolbar is the only child that is left.
  web_manager.reset();
  EXPECT_EQ(1u, views_root_node->PlatformChildCount());
}

// An ignored host that holds a child tree ID is unreachable through every
// platform accessor, thus it owns no platform node. Whether the tree that takes
// its place is there does not change this.
TEST_F(BrowserAccessibilityManagerWinTest, WebViewHostOwnsNoPlatformNode) {
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
  test_browser_accessibility_delegate_->accelerated_widget_ =
      gfx::kMockAcceleratedWidget;
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
  web_delegate->accelerated_widget_ = gfx::kMockAcceleratedWidget;
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
  EXPECT_EQ(gfx::NativeViewAccessible(), host_node->GetNativeViewAccessible());

  // The host keeps its child tree ID here, thus it stays ignored and owns no
  // platform node, whether or not the tree that takes its place is there.
  web_manager.reset();

  EXPECT_FALSE(host_node->GetAXPlatformNode());
  EXPECT_EQ(gfx::NativeViewAccessible(), host_node->GetNativeViewAccessible());
}

namespace {

// Lets a test choose whether a node owns a platform node. See
// BrowserAccessibility::ShouldHavePlatformNode for details.
class TogglablePlatformNodeBrowserAccessibilityWin
    : public BrowserAccessibilityWin {
 public:
  TogglablePlatformNodeBrowserAccessibilityWin(
      BrowserAccessibilityManager* manager,
      AXNode* node)
      : BrowserAccessibilityWin(manager, node) {}

  bool ShouldHavePlatformNode() const override { return should_have_; }

  void SetShouldHavePlatformNode(bool value) {
    should_have_ = value;
    UpdatePlatformNode();
  }

 private:
  bool should_have_ = true;
};

}  // namespace

class BrowserAccessibilityPlatformNodeWinTest
    : public BrowserAccessibilityManagerWinTest {
 protected:
  std::unique_ptr<TogglablePlatformNodeBrowserAccessibilityWin> MakeNode() {
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
    auto node = std::make_unique<TogglablePlatformNodeBrowserAccessibilityWin>(
        manager_.get(), manager_->GetFromID(2)->node());
    // The manager does this for every wrapper that it creates.
    node->OnDataChanged();
    return node;
  }

  std::unique_ptr<BrowserAccessibilityManager> manager_;
};

TEST_F(BrowserAccessibilityPlatformNodeWinTest,
       ANodeOwnsAPlatformNodeByDefault) {
  std::unique_ptr<TogglablePlatformNodeBrowserAccessibilityWin> node =
      MakeNode();

  EXPECT_TRUE(node->ShouldHavePlatformNode());
  EXPECT_TRUE(node->GetAXPlatformNode());
  EXPECT_TRUE(node->GetNativeViewAccessible());
}

TEST_F(BrowserAccessibilityPlatformNodeWinTest, ANodeCanOwnNoPlatformNode) {
  std::unique_ptr<TogglablePlatformNodeBrowserAccessibilityWin> node =
      MakeNode();
  node->SetShouldHavePlatformNode(false);

  EXPECT_FALSE(node->GetAXPlatformNode());
  EXPECT_FALSE(node->GetNativeViewAccessible());
}

// An event names a platform node, thus a node that owns none can fire none.
// The Windows override gives an event to a node below a collapsed select, and
// to a node that changed its ignored state, thus it must test this first.
TEST_F(BrowserAccessibilityPlatformNodeWinTest, NoPlatformNodeFiresNoEvent) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {2};

  AXNodeData collapsed_select;
  collapsed_select.id = 2;
  collapsed_select.role = ax::mojom::Role::kComboBoxSelect;
  collapsed_select.AddState(ax::mojom::State::kCollapsed);
  collapsed_select.child_ids = {3};

  AXNodeData option;
  option.id = 3;
  option.role = ax::mojom::Role::kMenuListOption;

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(root, collapsed_select, option),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));

  auto node = std::make_unique<TogglablePlatformNodeBrowserAccessibilityWin>(
      manager.get(), manager->GetFromID(3)->node());
  node->OnDataChanged();

  // The option is inside a leaf, thus the base class alone would give it no
  // event, and only the collapsed select keeps it able to fire one.
  ASSERT_TRUE(node->IsChildOfLeaf());
  ASSERT_TRUE(node->GetCollapsedMenuListSelectAncestor());
  ASSERT_TRUE(node->CanFireEvents());

  node->SetShouldHavePlatformNode(false);

  EXPECT_FALSE(node->CanFireEvents());
}

TEST_F(BrowserAccessibilityPlatformNodeWinTest,
       EveryAccessorIsSafeWithNoPlatformNode) {
  std::unique_ptr<TogglablePlatformNodeBrowserAccessibilityWin> node =
      MakeNode();
  node->SetShouldHavePlatformNode(false);

  EXPECT_TRUE(node->GetHypertext().empty());
  node->UpdatePlatformAttributes();
  node->OnLocationChanged();
  node->OnDataChanged();
}

TEST_F(BrowserAccessibilityPlatformNodeWinTest, APlatformNodeComesBack) {
  std::unique_ptr<TogglablePlatformNodeBrowserAccessibilityWin> node =
      MakeNode();
  node->SetShouldHavePlatformNode(false);
  ASSERT_FALSE(node->GetAXPlatformNode());

  node->SetShouldHavePlatformNode(true);

  EXPECT_TRUE(node->GetAXPlatformNode());
  EXPECT_TRUE(node->GetNativeViewAccessible());
}

TEST_F(BrowserAccessibilityPlatformNodeWinTest, RepeatedChangesAreSafe) {
  std::unique_ptr<TogglablePlatformNodeBrowserAccessibilityWin> node =
      MakeNode();

  for (int i = 0; i < 3; ++i) {
    node->SetShouldHavePlatformNode(false);
    ASSERT_FALSE(node->GetAXPlatformNode()) << "iteration " << i;

    node->SetShouldHavePlatformNode(true);
    ASSERT_TRUE(node->GetAXPlatformNode()) << "iteration " << i;
  }
}

TEST_F(BrowserAccessibilityPlatformNodeWinTest,
       RepeatedUpdatesKeepOnePlatformNode) {
  std::unique_ptr<TogglablePlatformNodeBrowserAccessibilityWin> node =
      MakeNode();
  AXPlatformNode* first = node->GetAXPlatformNode();

  node->UpdatePlatformNode();
  node->UpdatePlatformNode();

  EXPECT_EQ(first, node->GetAXPlatformNode());
}

}  // namespace ui
