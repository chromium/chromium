// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/browser_accessibility_manager.h"

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_tree_observer.h"
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
#include "ui/accessibility/platform/uia_registrar_win.h"

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

// Intercepts platform event dispatch in BrowserAccessibilityManagerWin, via
// SetPlatformEventCallbackForTesting(), so tests can simulate synchronous
// re-entrancy or tree mutation when an event is raised. While installed, no
// event reaches the OS.
class TestBrowserAccessibilityManagerWin
    : public ui::BrowserAccessibilityManagerWin {
 public:
  using EventCallback =
      base::RepeatingCallback<void(LONG, ui::BrowserAccessibility*)>;

  TestBrowserAccessibilityManagerWin(
      const ui::AXTreeUpdate& initial_tree,
      ui::AXNodeIdDelegate& node_id_delegate,
      ui::AXPlatformTreeManagerDelegate* delegate)
      : ui::BrowserAccessibilityManagerWin(initial_tree,
                                           node_id_delegate,
                                           delegate) {
    // Unretained is safe: the callback is owned by `this`.
    SetPlatformEventCallbackForTesting(base::BindRepeating(
        &TestBrowserAccessibilityManagerWin::OnPlatformEvent,
        base::Unretained(this)));
  }

  void SetWinEventCallback(EventCallback callback) {
    win_event_callback_ = std::move(callback);
  }
  void SetUiaEventCallback(EventCallback callback) {
    uia_event_callback_ = std::move(callback);
  }
  void SetUiaPropertyCallback(EventCallback callback) {
    uia_property_callback_ = std::move(callback);
  }

 private:
  void OnPlatformEvent(PlatformEventTypeForTesting type,
                       LONG id,
                       ui::BrowserAccessibility* node) {
    // Copy the callback: running it may destroy `this`.
    EventCallback callback;
    switch (type) {
      case PlatformEventTypeForTesting::kWinEvent:
        callback = win_event_callback_;
        break;
      case PlatformEventTypeForTesting::kUiaEvent:
        callback = uia_event_callback_;
        break;
      case PlatformEventTypeForTesting::kUiaPropertyChanged:
        callback = uia_property_callback_;
        break;
      case PlatformEventTypeForTesting::kUiaStructureChanged:
        break;
    }
    if (callback) {
      callback.Run(id, node);
    }
  }

  EventCallback win_event_callback_;
  EventCallback uia_event_callback_;
  EventCallback uia_property_callback_;
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

TEST_F(BrowserAccessibilityManagerWinTest,
       LocationChangeGroupedByCommonAncestor) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAccessibilityGroupLocationChangeByCommonAncestor);

  // Tree structure:
  // ++Root (ID 1)
  // ++++Container 1 (ID 2)
  // ++++++Child 1A (ID 3)
  // ++++++Child 1B (ID 4)
  // ++++Container 2 (ID 5)
  // ++++++Child 2A (ID 6)
  // ++++++Child 2B (ID 7)
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);
  root.child_ids = {2, 5};

  AXNodeData container1;
  container1.id = 2;
  container1.role = ax::mojom::Role::kGenericContainer;
  container1.relative_bounds.offset_container_id = 1;
  container1.relative_bounds.bounds = gfx::RectF(10, 10, 100, 100);
  container1.child_ids = {3, 4};

  AXNodeData child1a;
  child1a.id = 3;
  child1a.role = ax::mojom::Role::kStaticText;
  child1a.relative_bounds.offset_container_id = 2;
  child1a.relative_bounds.bounds = gfx::RectF(0, 0, 50, 10);

  AXNodeData child1b;
  child1b.id = 4;
  child1b.role = ax::mojom::Role::kStaticText;
  child1b.relative_bounds.offset_container_id = 2;
  child1b.relative_bounds.bounds = gfx::RectF(0, 10, 50, 10);

  AXNodeData container2;
  container2.id = 5;
  container2.role = ax::mojom::Role::kGenericContainer;
  container2.relative_bounds.offset_container_id = 1;
  container2.relative_bounds.bounds = gfx::RectF(200, 10, 100, 100);
  container2.child_ids = {6, 7};

  AXNodeData child2a;
  child2a.id = 6;
  child2a.role = ax::mojom::Role::kStaticText;
  child2a.relative_bounds.offset_container_id = 5;
  child2a.relative_bounds.bounds = gfx::RectF(0, 0, 50, 10);

  AXNodeData child2b;
  child2b.id = 7;
  child2b.role = ax::mojom::Role::kStaticText;
  child2b.relative_bounds.offset_container_id = 5;
  child2b.relative_bounds.bounds = gfx::RectF(0, 10, 50, 10);

  // Build the initial tree update.
  AXTreeUpdate initial_tree;
  AXTreeData tree_data;
  tree_data.tree_id = AXTreeID::CreateNewAXTreeID();
  tree_data.focused_tree_id = tree_data.tree_id;
  tree_data.parent_tree_id = AXTreeIDUnknown();
  initial_tree.tree_data = tree_data;
  initial_tree.has_tree_data = true;
  initial_tree.root_id = 1;
  initial_tree.nodes = {root,       container1, child1a, child1b,
                        container2, child2a,    child2b};

  test_browser_accessibility_delegate_->accelerated_widget_ =
      gfx::kMockAcceleratedWidget;

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          initial_tree, node_id_delegate_,
          test_browser_accessibility_delegate_.get()));

  std::vector<AXNodeID> fired_location_change_node_ids;
  manager->ToBrowserAccessibilityManagerWin()
      ->SetLocationChangeEventCallbackForTesting(
          base::BindLambdaForTesting([&](BrowserAccessibility* node) {
            fired_location_change_node_ids.push_back(node->GetId());
          }));

  // 1. Single node location change (Child 1A, ID 3).
  // A single location change fires a location change event on that node.
  {
    fired_location_change_node_ids.clear();
    AXLocationAndScrollUpdates updates;
    AXRelativeBounds bounds;
    bounds.offset_container_id = 2;
    bounds.bounds = gfx::RectF(10, 0, 50, 10);
    updates.location_changes.emplace_back(3, bounds);

    manager->OnLocationChanges(updates);

    EXPECT_THAT(fired_location_change_node_ids, testing::ElementsAre(3));
  }

  // 2. Multiple location changes under the same container (Child 1A ID 3 &
  // Child 1B ID 4). The lowest common ancestor is Container 1 (ID 2).
  {
    fired_location_change_node_ids.clear();
    AXLocationAndScrollUpdates updates;
    AXRelativeBounds bounds1;
    bounds1.offset_container_id = 2;
    bounds1.bounds = gfx::RectF(20, 0, 50, 10);
    updates.location_changes.emplace_back(3, bounds1);

    AXRelativeBounds bounds2;
    bounds2.offset_container_id = 2;
    bounds2.bounds = gfx::RectF(20, 10, 50, 10);
    updates.location_changes.emplace_back(4, bounds2);

    manager->OnLocationChanges(updates);

    EXPECT_THAT(fired_location_change_node_ids, testing::ElementsAre(2));
  }

  // 3. Multiple location changes across different branches (Child 1A ID 3 &
  // Child 2A ID 6). The lowest common ancestor is Root (ID 1).
  {
    fired_location_change_node_ids.clear();
    AXLocationAndScrollUpdates updates;
    AXRelativeBounds bounds1;
    bounds1.offset_container_id = 2;
    bounds1.bounds = gfx::RectF(30, 0, 50, 10);
    updates.location_changes.emplace_back(3, bounds1);

    AXRelativeBounds bounds2;
    bounds2.offset_container_id = 5;
    bounds2.bounds = gfx::RectF(30, 0, 50, 10);
    updates.location_changes.emplace_back(6, bounds2);

    manager->OnLocationChanges(updates);

    EXPECT_THAT(fired_location_change_node_ids, testing::ElementsAre(1));
  }

  // 4. Ensure LCA algorithm goes upward from first common ancestor found to
  // lowest overall common ancestor (Container 2 ID 5, Child 1B ID 4, and Child
  // 1A ID 3). The lowest common ancestor is Root (ID 1).
  {
    fired_location_change_node_ids.clear();
    AXLocationAndScrollUpdates updates;
    AXRelativeBounds bounds1;
    bounds1.offset_container_id = 1;
    bounds1.bounds = gfx::RectF(40, 10, 100, 100);
    updates.location_changes.emplace_back(5, bounds1);

    AXRelativeBounds bounds2;
    bounds2.offset_container_id = 2;
    bounds2.bounds = gfx::RectF(40, 10, 50, 10);
    updates.location_changes.emplace_back(4, bounds2);

    AXRelativeBounds bounds3;
    bounds2.offset_container_id = 2;
    bounds2.bounds = gfx::RectF(40, 0, 50, 10);
    updates.location_changes.emplace_back(3, bounds3);

    manager->OnLocationChanges(updates);

    EXPECT_THAT(fired_location_change_node_ids, testing::ElementsAre(1));
  }
}

namespace {

base::OnceClosure* GetReentrantWinEventCallbackStorage() {
  static base::NoDestructor<base::OnceClosure> callback;
  return callback.get();
}

void CALLBACK ReentrantDescriptionChangeWinEventProc(HWINEVENTHOOK handle,
                                                     DWORD event,
                                                     HWND hwnd,
                                                     LONG obj_id,
                                                     LONG child_id,
                                                     DWORD event_thread,
                                                     DWORD event_time) {
  if (event == EVENT_OBJECT_DESCRIPTIONCHANGE &&
      *GetReentrantWinEventCallbackStorage()) {
    std::move(*GetReentrantWinEventCallbackStorage()).Run();
  }
}

}  // namespace

TEST_F(BrowserAccessibilityManagerWinTest,
       AtomicUpdateReentrantNodeDestruction) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {2, 3};

  AXNodeData button1;
  button1.id = 2;
  button1.role = ax::mojom::Role::kButton;
  button1.SetDescription("old description 1");

  AXNodeData button2;
  button2.id = 3;
  button2.role = ax::mojom::Role::kButton;
  button2.SetDescription("old description 2");

  // Use a real HWND owned by the current thread so ::NotifyWinEvent
  // synchronously dispatches in-context WinEvent hooks on this thread.
  HWND hwnd = ::CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE,
                                nullptr, nullptr, nullptr);
  ASSERT_TRUE(hwnd);
  absl::Cleanup destroy_hwnd = [hwnd] { ::DestroyWindow(hwnd); };
  test_browser_accessibility_delegate_->accelerated_widget_ = hwnd;

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(root, button1, button2), node_id_delegate_,
          test_browser_accessibility_delegate_.get()));

  HWINEVENTHOOK hook = ::SetWinEventHook(
      EVENT_OBJECT_DESCRIPTIONCHANGE, EVENT_OBJECT_DESCRIPTIONCHANGE,
      ::GetModuleHandle(nullptr), &ReentrantDescriptionChangeWinEventProc,
      ::GetCurrentProcessId(), ::GetCurrentThreadId(), WINEVENT_INCONTEXT);
  ASSERT_TRUE(hook);
  absl::Cleanup unhook = [hook] {
    ::UnhookWinEvent(hook);
    *GetReentrantWinEventCallbackStorage() = base::OnceClosure();
  };

  std::vector<AXTreeObserver::Change> changes = {
      {manager->ax_tree()->GetFromId(2), AXTreeObserver::NODE_CHANGED},
      {manager->ax_tree()->GetFromId(3), AXTreeObserver::NODE_CHANGED}};
  bool hook_ran = false;
  *GetReentrantWinEventCallbackStorage() = base::BindLambdaForTesting([&]() {
    hook_ran = true;
    // Clear the outer `changes` vector before unserializing `inner_update` so
    // `AXTreeObserver::Change::node` (`raw_ptr<AXNode>`) does not dangle when
    // `Unserialize()` destroys `AXNode`s 2 and 3.
    changes.clear();
    AXNodeData updated_root = root;
    updated_root.child_ids = {};
    AXTreeUpdate inner_update;
    inner_update.nodes = {updated_root};
    EXPECT_TRUE(manager->ax_tree()->Unserialize(inner_update));
  });

  // Update both sibling buttons' descriptions in a single atomic update so
  // both are queued in `objs_to_update`. When the first button fires
  // EVENT_OBJECT_DESCRIPTIONCHANGE during UpdateStep3FireEvents(), the
  // synchronous hook removes both buttons from the tree while `manager`
  // remains alive.
  // Note: Call `OnAtomicUpdateFinished` directly for the outer update rather
  // than `ax_tree()->Unserialize()`, because `AXTree::Unserialize()` invokes
  // `OnAtomicUpdateFinished` inside `observers_.Notify()`, and
  // `AXTree::observers_` uses
  // `ObserverListReentrancyPolicy::kDisallowReentrancy` which
  // `DUMP_WILL_BE_CHECK`s if the hook's `Unserialize()` iterates `observers_`
  // while the outer `Notify()` iterator is still on the stack.
  button1.SetDescription("new description 1");
  button2.SetDescription("new description 2");
  manager->ax_tree()->GetFromId(2)->SetData(button1);
  manager->ax_tree()->GetFromId(3)->SetData(button2);
  manager->OnAtomicUpdateFinished(manager->ax_tree(), /*root_changed=*/false,
                                  changes);

  EXPECT_TRUE(hook_ran);
  EXPECT_EQ(nullptr, manager->GetFromID(2));
  EXPECT_EQ(nullptr, manager->GetFromID(3));
}

TEST_F(BrowserAccessibilityManagerWinTest,
       AtomicUpdateReentrantInPlaceNodeModification) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {2};

  AXNodeData button;
  button.id = 2;
  button.role = ax::mojom::Role::kButton;
  button.SetDescription("old description");
  button.child_ids = {3};

  AXNodeData static_text;
  static_text.id = 3;
  static_text.role = ax::mojom::Role::kStaticText;
  static_text.SetName("old text");

  HWND hwnd = ::CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE,
                                nullptr, nullptr, nullptr);
  ASSERT_TRUE(hwnd);
  absl::Cleanup destroy_hwnd = [hwnd] { ::DestroyWindow(hwnd); };
  test_browser_accessibility_delegate_->accelerated_widget_ = hwnd;

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(root, button, static_text),
          node_id_delegate_, test_browser_accessibility_delegate_.get()));

  HWINEVENTHOOK hook = ::SetWinEventHook(
      EVENT_OBJECT_DESCRIPTIONCHANGE, EVENT_OBJECT_DESCRIPTIONCHANGE,
      ::GetModuleHandle(nullptr), &ReentrantDescriptionChangeWinEventProc,
      ::GetCurrentProcessId(), ::GetCurrentThreadId(), WINEVENT_INCONTEXT);
  ASSERT_TRUE(hook);
  absl::Cleanup unhook = [hook] {
    ::UnhookWinEvent(hook);
    *GetReentrantWinEventCallbackStorage() = base::OnceClosure();
  };

  bool hook_ran = false;
  *GetReentrantWinEventCallbackStorage() = base::BindLambdaForTesting([&]() {
    hook_ran = true;
    AXNodeData inner_button = button;
    inner_button.SetDescription("reentrant description");
    AXNodeData inner_text = static_text;
    inner_text.SetName("reentrant text");
    AXTreeUpdate inner_update;
    inner_update.nodes = {inner_button, inner_text};
    EXPECT_TRUE(manager->ax_tree()->Unserialize(inner_update));
  });

  // Modify both description and child text in the outer update so that after
  // `FireNativeEvent(EVENT_OBJECT_DESCRIPTIONCHANGE)` returns, outer
  // `UpdateStep3FireEvents()` would otherwise proceed to hypertext diffing
  // against moved-from `old_win_attributes` and cleared `update_state_`.
  // As above, call `OnAtomicUpdateFinished` directly for the outer update so
  // the inner `Unserialize()` does not trip `AXTree::observers_`'s
  // `kDisallowReentrancy` check during `observers_.Notify()`.
  button.SetDescription("new description");
  static_text.SetName("new text");
  manager->ax_tree()->GetFromId(2)->SetData(button);
  manager->ax_tree()->GetFromId(3)->SetData(static_text);
  std::vector<AXTreeObserver::Change> changes = {
      {manager->ax_tree()->GetFromId(2), AXTreeObserver::NODE_CHANGED},
      {manager->ax_tree()->GetFromId(3), AXTreeObserver::NODE_CHANGED}};
  manager->OnAtomicUpdateFinished(manager->ax_tree(), /*root_changed=*/false,
                                  changes);

  EXPECT_TRUE(hook_ran);
  ASSERT_NE(nullptr, manager->GetFromID(2));
  EXPECT_EQ("reentrant description",
            manager->GetFromID(2)->GetStringAttribute(
                ax::mojom::StringAttribute::kDescription));
}

// Synchronous platform event dispatch during FinalizeAccessibilityEvents may
// re-entrantly destroy the manager; remaining queued events must not be
// dispatched on a destroyed manager. The tests below destroy the manager while
// finalizing each kind of queued event.
//
// The pending-event sets are unordered, so each test queues a single node and
// checks that TestComplete, which is raised last, is not raised.

// Destroying the manager while finalizing `aria_properties_events_`.
TEST_F(BrowserAccessibilityManagerWinTest,
       FinalizeAriaPropertiesEventsHandlesManagerDestruction) {
  test_browser_accessibility_delegate_->accelerated_widget_ =
      gfx::kMockAcceleratedWidget;
  const LONG test_complete_event =
      UiaRegistrarWin::GetInstance().GetTestCompleteEventId();

  {
    AXNodeData root;
    root.id = 1;
    root.role = ax::mojom::Role::kRootWebArea;
    root.child_ids = {2};

    AXNodeData button;
    button.id = 2;
    button.role = ax::mojom::Role::kButton;

    auto manager = std::make_unique<TestBrowserAccessibilityManagerWin>(
        MakeAXTreeUpdateForTesting(root, button), node_id_delegate_,
        test_browser_accessibility_delegate_.get());

    std::vector<AXNodeID> fired_nodes;
    bool test_complete_fired = false;
    manager->SetUiaPropertyCallback(base::BindLambdaForTesting(
        [&](LONG uia_property, BrowserAccessibility* node) {
          if (uia_property == UIA_AriaPropertiesPropertyId) {
            fired_nodes.push_back(node->GetId());
            manager.reset();
          }
        }));
    manager->SetUiaEventCallback(base::BindLambdaForTesting(
        [&](LONG uia_event, BrowserAccessibility* node) {
          if (uia_event == test_complete_event) {
            test_complete_fired = true;
          }
        }));

    button.AddBoolAttribute(ax::mojom::BoolAttribute::kBusy, true);
    AXUpdatesAndEvents bundle;
    bundle.updates.emplace_back();
    bundle.updates[0].nodes = {button};
    bundle.events.emplace_back(root.id, ax::mojom::Event::kEndOfTest);
    EXPECT_TRUE(manager->OnAccessibilityEvents(bundle));
    EXPECT_FALSE(manager);
    EXPECT_THAT(fired_nodes, testing::ElementsAre(2));
    EXPECT_FALSE(test_complete_fired);
  }
}

// Destroying the manager while finalizing `selection_changed_nodes_` and
// `text_changed_nodes_`.
TEST_F(BrowserAccessibilityManagerWinTest,
       FinalizeTextEventsHandlesManagerDestruction) {
  test_browser_accessibility_delegate_->accelerated_widget_ =
      gfx::kMockAcceleratedWidget;
  const LONG test_complete_event =
      UiaRegistrarWin::GetInstance().GetTestCompleteEventId();

  {
    AXNodeData root;
    root.id = 1;
    root.role = ax::mojom::Role::kRootWebArea;
    root.child_ids = {2, 3};

    AXNodeData static_text;
    static_text.id = 2;
    static_text.role = ax::mojom::Role::kStaticText;
    static_text.SetName("hello");

    AXNodeData text_field;
    text_field.id = 3;
    text_field.role = ax::mojom::Role::kTextField;
    text_field.SetValue("a");

    // `selection_changed_nodes_` (UIA_Text_TextSelectionChangedEventId).
    auto manager = std::make_unique<TestBrowserAccessibilityManagerWin>(
        MakeAXTreeUpdateForTesting(root, static_text, text_field),
        node_id_delegate_, test_browser_accessibility_delegate_.get());

    std::vector<AXNodeID> fired_selection_nodes;
    bool test_complete_fired = false;
    manager->SetUiaEventCallback(base::BindLambdaForTesting(
        [&](LONG uia_event, BrowserAccessibility* node) {
          if (uia_event == UIA_Text_TextSelectionChangedEventId) {
            fired_selection_nodes.push_back(node->GetId());
            manager.reset();
          } else if (uia_event == test_complete_event) {
            test_complete_fired = true;
          }
        }));

    // `root` (ID 1) has no text field ancestor, so only `root` is queued.
    AXUpdatesAndEvents sel_bundle;
    sel_bundle.updates.emplace_back();
    sel_bundle.updates[0].has_tree_data = true;
    sel_bundle.updates[0].tree_data = manager->GetTreeData();
    sel_bundle.updates[0].tree_data.sel_anchor_object_id = 1;
    sel_bundle.updates[0].tree_data.sel_anchor_offset = 0;
    sel_bundle.updates[0].tree_data.sel_focus_object_id = 1;
    sel_bundle.updates[0].tree_data.sel_focus_offset = 1;
    sel_bundle.events.emplace_back(root.id, ax::mojom::Event::kEndOfTest);
    EXPECT_TRUE(manager->OnAccessibilityEvents(sel_bundle));
    EXPECT_FALSE(manager);
    EXPECT_THAT(fired_selection_nodes, testing::ElementsAre(1));
    EXPECT_FALSE(test_complete_fired);

    // `text_changed_nodes_` (UIA_Text_TextChangedEventId).
    manager = std::make_unique<TestBrowserAccessibilityManagerWin>(
        MakeAXTreeUpdateForTesting(root, static_text, text_field),
        node_id_delegate_, test_browser_accessibility_delegate_.get());

    std::vector<AXNodeID> fired_text_nodes;
    test_complete_fired = false;
    manager->SetUiaEventCallback(base::BindLambdaForTesting(
        [&](LONG uia_event, BrowserAccessibility* node) {
          if (uia_event == UIA_Text_TextChangedEventId) {
            fired_text_nodes.push_back(node->GetId());
            manager.reset();
          } else if (uia_event == test_complete_event) {
            test_complete_fired = true;
          }
        }));

    // `text_field` (ID 3) is its own text pattern provider, so only it is
    // queued.
    AXNodeData changed_text_field = text_field;
    changed_text_field.SetValue("a2");

    AXUpdatesAndEvents text_bundle;
    text_bundle.updates.emplace_back();
    text_bundle.updates[0].nodes = {changed_text_field};
    text_bundle.events.emplace_back(root.id, ax::mojom::Event::kEndOfTest);
    EXPECT_TRUE(manager->OnAccessibilityEvents(text_bundle));
    EXPECT_FALSE(manager);
    EXPECT_THAT(fired_text_nodes, testing::ElementsAre(3));
    EXPECT_FALSE(test_complete_fired);
  }
}

// Destroying the manager while finalizing `ia2_selection_events_` and
// `uia_selection_events_`.
TEST_F(BrowserAccessibilityManagerWinTest,
       FinalizeSelectionItemEventsHandlesManagerDestruction) {
  test_browser_accessibility_delegate_->accelerated_widget_ =
      gfx::kMockAcceleratedWidget;

  {
    AXNodeData root;
    root.id = 1;
    root.role = ax::mojom::Role::kRootWebArea;
    root.child_ids = {2};

    AXNodeData list_box;
    list_box.id = 2;
    list_box.role = ax::mojom::Role::kListBox;
    list_box.AddState(ax::mojom::State::kMultiselectable);
    list_box.child_ids = {3, 4};

    AXNodeData option1;
    option1.id = 3;
    option1.role = ax::mojom::Role::kListBoxOption;
    option1.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, false);

    AXNodeData option2;
    option2.id = 4;
    option2.role = ax::mojom::Role::kListBoxOption;
    option2.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, false);

    AXNodeData selected_option1 = option1;
    selected_option1.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                                      true);
    AXNodeData selected_option2 = option2;
    selected_option2.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                                      true);

    // `ia2_selection_events_` (EVENT_OBJECT_SELECTIONADD).
    auto manager = std::make_unique<TestBrowserAccessibilityManagerWin>(
        MakeAXTreeUpdateForTesting(root, list_box, option1, option2),
        node_id_delegate_, test_browser_accessibility_delegate_.get());

    std::vector<AXNodeID> fired_ia2_nodes;
    manager->SetWinEventCallback(base::BindLambdaForTesting(
        [&](LONG win_event, BrowserAccessibility* node) {
          if (win_event == EVENT_OBJECT_SELECTIONADD) {
            fired_ia2_nodes.push_back(node->GetId());
            manager.reset();
          }
        }));

    AXUpdatesAndEvents ia2_bundle;
    ia2_bundle.updates.emplace_back();
    ia2_bundle.updates[0].nodes = {selected_option1, selected_option2};
    EXPECT_TRUE(manager->OnAccessibilityEvents(ia2_bundle));
    EXPECT_FALSE(manager);
    EXPECT_THAT(fired_ia2_nodes, testing::ElementsAre(3));

    // `uia_selection_events_`
    // (UIA_SelectionItem_ElementAddedToSelectionEventId).
    manager = std::make_unique<TestBrowserAccessibilityManagerWin>(
        MakeAXTreeUpdateForTesting(root, list_box, option1, option2),
        node_id_delegate_, test_browser_accessibility_delegate_.get());

    std::vector<AXNodeID> fired_uia_nodes;
    manager->SetUiaEventCallback(base::BindLambdaForTesting(
        [&](LONG uia_event, BrowserAccessibility* node) {
          if (uia_event == UIA_SelectionItem_ElementAddedToSelectionEventId) {
            fired_uia_nodes.push_back(node->GetId());
            manager.reset();
          }
        }));

    AXUpdatesAndEvents uia_bundle;
    uia_bundle.updates.emplace_back();
    uia_bundle.updates[0].nodes = {selected_option1, selected_option2};
    EXPECT_TRUE(manager->OnAccessibilityEvents(uia_bundle));
    EXPECT_FALSE(manager);
    EXPECT_THAT(fired_uia_nodes, testing::ElementsAre(3));
  }
}

// Destroying the manager while raising ElementSelected on the only selected
// child of a single-selection container must skip the following
// SelectionItemIsSelected property change.
TEST_F(BrowserAccessibilityManagerWinTest,
       FinalizeOnlySelectedChildHandlesManagerDestruction) {
  test_browser_accessibility_delegate_->accelerated_widget_ =
      gfx::kMockAcceleratedWidget;

  {
    AXNodeData root;
    root.id = 1;
    root.role = ax::mojom::Role::kRootWebArea;
    root.child_ids = {2};

    AXNodeData list_box;
    list_box.id = 2;
    list_box.role = ax::mojom::Role::kListBox;
    list_box.child_ids = {3, 4};

    AXNodeData option1;
    option1.id = 3;
    option1.role = ax::mojom::Role::kListBoxOption;
    option1.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, false);

    AXNodeData option2;
    option2.id = 4;
    option2.role = ax::mojom::Role::kListBoxOption;
    option2.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, false);

    auto manager = std::make_unique<TestBrowserAccessibilityManagerWin>(
        MakeAXTreeUpdateForTesting(root, list_box, option1, option2),
        node_id_delegate_, test_browser_accessibility_delegate_.get());

    std::vector<AXNodeID> selected_nodes;
    bool is_selected_property_fired = false;
    manager->SetUiaEventCallback(base::BindLambdaForTesting(
        [&](LONG uia_event, BrowserAccessibility* node) {
          if (uia_event == UIA_SelectionItem_ElementSelectedEventId) {
            selected_nodes.push_back(node->GetId());
            manager.reset();
          }
        }));
    manager->SetUiaPropertyCallback(base::BindLambdaForTesting(
        [&](LONG uia_property, BrowserAccessibility* node) {
          if (uia_property == UIA_SelectionItemIsSelectedPropertyId) {
            is_selected_property_fired = true;
          }
        }));

    option1.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
    AXUpdatesAndEvents bundle;
    bundle.updates.emplace_back();
    bundle.updates[0].nodes = {option1};
    EXPECT_TRUE(manager->OnAccessibilityEvents(bundle));
    EXPECT_FALSE(manager);
    EXPECT_THAT(selected_nodes, testing::ElementsAre(3));
    EXPECT_FALSE(is_selected_property_fired);
  }
}

// Defense in depth: in production the tree can't change during a raise
// without the manager being destroyed, since a nested OnAccessibilityEvents()
// is a CHECK failure on Windows. Queued nodes are still looked up again by ID
// before each raise, so a node deleted in the meantime is skipped while the
// remaining nodes still receive their events.
TEST_F(BrowserAccessibilityManagerWinTest,
       FinalizeAccessibilityEventsSkipsDeletedNodes) {
  test_browser_accessibility_delegate_->accelerated_widget_ =
      gfx::kMockAcceleratedWidget;
  const LONG test_complete_event =
      UiaRegistrarWin::GetInstance().GetTestCompleteEventId();

  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {2, 3};

  AXNodeData button;
  button.id = 2;
  button.role = ax::mojom::Role::kButton;

  AXNodeData text_field;
  text_field.id = 3;
  text_field.role = ax::mojom::Role::kTextField;
  text_field.SetValue("a");

  auto manager = std::make_unique<TestBrowserAccessibilityManagerWin>(
      MakeAXTreeUpdateForTesting(root, button, text_field), node_id_delegate_,
      test_browser_accessibility_delegate_.get());

  std::vector<AXNodeID> fired_aria_property_nodes;
  manager->SetUiaPropertyCallback(base::BindLambdaForTesting(
      [&](LONG uia_property, BrowserAccessibility* node) {
        if (uia_property != UIA_AriaPropertiesPropertyId) {
          return;
        }
        fired_aria_property_nodes.push_back(node->GetId());
        // `aria_properties_events_` is finalized first, so delete `text_field`
        // (ID 3) while it is still queued in `text_changed_nodes_`.
        AXNodeData updated_root = root;
        updated_root.child_ids = {2};
        AXTreeUpdate remove_text_field_update;
        remove_text_field_update.root_id = 1;
        remove_text_field_update.nodes = {updated_root};
        ASSERT_TRUE(manager->ax_tree()->Unserialize(remove_text_field_update));
        ASSERT_EQ(nullptr, manager->GetFromID(3));
      }));

  std::vector<AXNodeID> fired_text_changed_nodes;
  std::vector<AXNodeID> fired_test_complete_nodes;
  manager->SetUiaEventCallback(base::BindLambdaForTesting(
      [&](LONG uia_event, BrowserAccessibility* node) {
        if (uia_event == UIA_Text_TextChangedEventId) {
          fired_text_changed_nodes.push_back(node->GetId());
        } else if (uia_event == test_complete_event) {
          fired_test_complete_nodes.push_back(node->GetId());
        }
      }));

  button.AddBoolAttribute(ax::mojom::BoolAttribute::kBusy, true);
  AXNodeData changed_text_field = text_field;
  changed_text_field.SetValue("a2");
  AXUpdatesAndEvents bundle;
  bundle.updates.emplace_back();
  bundle.updates[0].nodes = {button, changed_text_field};
  bundle.events.emplace_back(root.id, ax::mojom::Event::kEndOfTest);
  EXPECT_TRUE(manager->OnAccessibilityEvents(bundle));
  EXPECT_THAT(fired_aria_property_nodes, testing::ElementsAre(2));
  EXPECT_THAT(fired_text_changed_nodes, testing::IsEmpty());
  EXPECT_THAT(fired_test_complete_nodes, testing::ElementsAre(1));
}

// Ignored-changed nodes are tracked by ID, which is only unique within a tree.
// A node from another frame's tree with the same ID must not be treated as
// ignored-changed.
TEST_F(BrowserAccessibilityManagerWinTest,
       IsIgnoredChangedNodeOnlyMatchesOwnTree) {
  test_browser_accessibility_delegate_->accelerated_widget_ =
      gfx::kMockAcceleratedWidget;

  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {2};
  AXNodeData button;
  button.id = 2;
  button.role = ax::mojom::Role::kButton;

  auto manager = std::make_unique<TestBrowserAccessibilityManagerWin>(
      MakeAXTreeUpdateForTesting(root, button), node_id_delegate_,
      test_browser_accessibility_delegate_.get());

  auto other_delegate = std::make_unique<TestAXPlatformTreeManagerDelegate>();
  other_delegate->is_root_frame_ = false;
  other_delegate->accelerated_widget_ = gfx::kMockAcceleratedWidget;
  std::unique_ptr<BrowserAccessibilityManager> other_manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(root, button), node_id_delegate_,
          other_delegate.get()));

  bool checked = false;
  manager->SetWinEventCallback(base::BindLambdaForTesting(
      [&](LONG win_event, BrowserAccessibility* node) {
        if (win_event != EVENT_OBJECT_HIDE || node->GetId() != 2) {
          return;
        }
        checked = true;
        EXPECT_TRUE(manager->IsIgnoredChangedNode(manager->GetFromID(2)));
        EXPECT_FALSE(
            manager->IsIgnoredChangedNode(other_manager->GetFromID(2)));
      }));

  button.AddState(ax::mojom::State::kIgnored);
  AXUpdatesAndEvents bundle;
  bundle.updates.emplace_back();
  bundle.updates[0].nodes = {button};
  EXPECT_TRUE(manager->OnAccessibilityEvents(bundle));
  EXPECT_TRUE(checked);
}

}  // namespace ui
