// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_aura_obj_cache.h"

#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_tree_source_checker.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/accessibility/ax_event_observer.h"
#include "ui/views/accessibility/ax_tree_source_views.h"
#include "ui/views/accessibility/ax_virtual_view.h"
#include "ui/views/accessibility/ax_virtual_view_wrapper.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget_delegate.h"

namespace views::test {
namespace {

bool HasNodeWithName(ui::AXNode* node, const std::string& name) {
  if (node->GetStringAttribute(ax::mojom::StringAttribute::kName) == name)
    return true;
  for (ui::AXNode* child : node->children()) {
    if (HasNodeWithName(child, name))
      return true;
  }
  return false;
}

bool HasNodeWithName(const ui::AXTree& tree, const std::string& name) {
  return HasNodeWithName(tree.root(), name);
}

class AXAuraObjCacheTest : public WidgetTest {
 public:
  AXAuraObjCacheTest() = default;
  ~AXAuraObjCacheTest() override = default;

  ui::AXNodeData GetData(AXAuraObjWrapper* wrapper) {
    ui::AXNodeData data;
    wrapper->Serialize(&data);
    return data;
  }
};

TEST_F(AXAuraObjCacheTest, TestViewRemoval) {
  AXAuraObjCache cache;
  std::unique_ptr<Widget> widget(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  auto* parent = widget->GetRootView()->AddChildView(std::make_unique<View>());
  auto* child = parent->AddChildView(std::make_unique<View>());

  AXAuraObjWrapper* ax_widget = cache.GetOrCreate(widget.get());
  ASSERT_NE(nullptr, ax_widget);
  AXAuraObjWrapper* ax_parent = cache.GetOrCreate(parent);
  ASSERT_NE(nullptr, ax_parent);
  AXAuraObjWrapper* ax_child = cache.GetOrCreate(child);
  ASSERT_NE(nullptr, ax_child);

  // Everything should have an ID, indicating it's in the cache.
  ASSERT_NE(cache.GetID(widget.get()), ui::kInvalidAXNodeID);
  ASSERT_NE(cache.GetID(parent), ui::kInvalidAXNodeID);
  ASSERT_NE(cache.GetID(child), ui::kInvalidAXNodeID);

  // Removing the parent view should remove both the parent and child
  // from the cache, but leave the widget.
  widget->GetRootView()->RemoveChildViewT(parent);
  ASSERT_NE(cache.GetID(widget.get()), ui::kInvalidAXNodeID);
  ASSERT_EQ(ui::kInvalidAXNodeID, cache.GetID(parent));
  ASSERT_EQ(ui::kInvalidAXNodeID, cache.GetID(child));
}

// Helper for the ViewDestruction test.
class ViewBlurObserver : public ViewObserver {
 public:
  ViewBlurObserver(AXAuraObjCache* cache, View* view) : cache_(cache) {
    observation_.Observe(view);
  }

  void OnViewBlurred(View* view) override {
    ASSERT_FALSE(was_called());
    observation_.Reset();

    // The cache entry gets deleted in
    // AXViewObjWrapper::OnViewIsDeleting which occurs later in ~View.
  }

  bool was_called() { return !observation_.IsObserving(); }

 private:
  raw_ptr<AXAuraObjCache> cache_;
  base::ScopedObservation<View, ViewObserver> observation_{this};
};

// Test that stale cache entries are not left behind if a cache entry is
// re-created during View destruction.
TEST_F(AXAuraObjCacheTest, ViewDestruction) {
  AXAuraObjCache cache;

  std::unique_ptr<Widget> widget(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  auto button =
      std::make_unique<LabelButton>(Button::PressedCallback(), u"button");
  auto* button_ptr = button.get();
  widget->GetRootView()->AddChildView(std::move(button));
  widget->Activate();
  button_ptr->RequestFocus();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(button_ptr->HasFocus());

  cache.GetOrCreate(widget.get());
  cache.GetOrCreate(button_ptr);
  // Everything should have an ID, indicating it's in the cache.
  EXPECT_NE(cache.GetID(widget.get()), ui::kInvalidAXNodeID);
  EXPECT_NE(cache.GetID(button_ptr), ui::kInvalidAXNodeID);

  ViewBlurObserver observer(&cache, button_ptr);
  widget->GetRootView()->RemoveChildViewT(button_ptr);

  // The button object is destroyed, so there should be no stale cache entries.
  EXPECT_NE(button_ptr, nullptr);
  EXPECT_EQ(ui::kInvalidAXNodeID, cache.GetID(button_ptr));
  EXPECT_TRUE(observer.was_called());
}

TEST_F(AXAuraObjCacheTest, CacheDestructionUAF) {
  // This test ensures that a UAF is not possible during cache destruction.
  // Two top-level widgets need to be created and inserted into |root_windows_|.
  // This test uses manual memory management, rather than managed helpers
  // that other tests are using. Ensure there is not a UAF crash when deleting
  // the cache.
  auto cache = std::make_unique<AXAuraObjCache>();

  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  widget->Init(std::move(params));
  cache->OnRootWindowObjCreated(widget->GetNativeWindow());

  widget->Activate();
  base::RunLoop().RunUntilIdle();

  cache->GetOrCreate(widget.get());

  // Everything should have an ID, indicating it's in the cache.
  EXPECT_NE(cache->GetID(widget.get()), ui::kInvalidAXNodeID);

  // Create a second top-level widget to ensure |root_windows_| isn't empty.
  auto widget2 = std::make_unique<Widget>();
  Widget::InitParams params2 = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  params2.bounds = gfx::Rect(0, 0, 200, 200);
  widget2->Init(std::move(params2));
  cache->OnRootWindowObjCreated(widget2->GetNativeWindow());

  cache->GetOrCreate(widget2.get());
  widget2->Activate();
  base::RunLoop().RunUntilIdle();

  // Everything should have an ID, indicating it's in the cache.
  EXPECT_NE(cache->GetID(widget2.get()), ui::kInvalidAXNodeID);

  // Delete the first widget, then delete the cache.
  cache->OnRootWindowObjDestroyed(widget->GetNativeWindow());
  widget.reset();
  cache.reset();
}

TEST_F(AXAuraObjCacheTest, ValidTree) {
  // Create a parent window.
  auto parent_widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  parent_widget->Init(std::move(params));
  parent_widget->GetNativeWindow()->SetTitle(u"ParentWindow");
  parent_widget->Show();

  // Create a child window.
  auto child_widget = std::make_unique<Widget>();  // Owned by parent_widget.
  params = CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                        Widget::InitParams::TYPE_BUBBLE);
  params.parent = parent_widget->GetNativeWindow();
  params.child = true;
  params.bounds = gfx::Rect(100, 100, 200, 200);
  child_widget->Init(std::move(params));
  child_widget->GetNativeWindow()->SetTitle(u"ChildWindow");
  child_widget->Show();

  // Create a child view.
  auto button =
      std::make_unique<LabelButton>(Button::PressedCallback(), u"ChildButton");
  button->SetSize(gfx::Size(20, 20));
  child_widget->GetContentsView()->AddChildView(std::move(button));

  // Use AXAuraObjCache to serialize the node tree.
  AXAuraObjCache cache;
  ui::AXTreeID tree_id = ui::AXTreeID::CreateNewAXTreeID();
  AXTreeSourceViews tree_source(
      cache.GetOrCreate(parent_widget->GetNativeWindow())->GetUniqueId(),
      tree_id, &cache);
  ui::AXTreeSerializer<AXAuraObjWrapper*, std::vector<AXAuraObjWrapper*>,
                       ui::AXTreeUpdate*, ui::AXTreeData*, ui::AXNodeData>
      serializer(&tree_source);
  ui::AXTreeUpdate serialized_tree;
  serializer.SerializeChanges(tree_source.GetRoot(), &serialized_tree);

  // Verify tree is valid.
  ui::AXTreeSourceChecker<AXAuraObjWrapper*> checker(&tree_source);
  std::string error_string;
  EXPECT_TRUE(checker.CheckAndGetErrorString(&error_string)) << error_string;
  ui::AXTree ax_tree(serialized_tree);
  EXPECT_TRUE(HasNodeWithName(ax_tree, "ParentWindow"));
  EXPECT_TRUE(HasNodeWithName(ax_tree, "ChildWindow"));
  EXPECT_TRUE(HasNodeWithName(ax_tree, "ChildButton"));
}

TEST_F(AXAuraObjCacheTest, GetFocusIsUnignoredAncestor) {
  AXAuraObjCache cache;
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  widget->Init(std::move(params));
  widget->Show();

  // Note that AXAuraObjCache::GetFocusedView has some logic to force focus on
  // the first child of the client view when one cannot be found from the
  // FocusManager if it has a child tree id.
  ClientView* client = widget->non_client_view()->client_view();
  ASSERT_NE(nullptr, client);
  View* client_child = client->children().front();
  ASSERT_NE(nullptr, client_child);
  client_child->GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
  client_child->GetViewAccessibility().SetChildTreeID(
      ui::AXTreeID::CreateNewAXTreeID());

  auto* parent = widget->GetRootView()->AddChildView(std::make_unique<View>());
  parent->GetViewAccessibility().SetRole(ax::mojom::Role::kTextField);
  parent->SetFocusBehavior(View::FocusBehavior::ALWAYS);

  auto* child = parent->AddChildView(std::make_unique<View>());
  child->GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  child->SetFocusBehavior(View::FocusBehavior::ALWAYS);

  AXAuraObjWrapper* ax_widget = cache.GetOrCreate(widget.get());
  ASSERT_NE(nullptr, ax_widget);
  AXAuraObjWrapper* ax_client_child = cache.GetOrCreate(client_child);
  ASSERT_NE(nullptr, ax_client_child);
  AXAuraObjWrapper* ax_parent = cache.GetOrCreate(parent);
  ASSERT_NE(nullptr, ax_parent);
  AXAuraObjWrapper* ax_child = cache.GetOrCreate(child);
  ASSERT_NE(nullptr, ax_child);

  ASSERT_EQ(nullptr, cache.GetFocus());
  cache.OnRootWindowObjCreated(widget->GetNativeWindow());
  ASSERT_EQ(ax::mojom::Role::kDialog, GetData(cache.GetFocus()).role);
  ASSERT_EQ(ax_client_child, cache.GetFocus());

  parent->RequestFocus();
  ASSERT_EQ(ax::mojom::Role::kTextField, GetData(cache.GetFocus()).role);
  ASSERT_EQ(ax_parent, cache.GetFocus());

  child->RequestFocus();
  ASSERT_EQ(ax::mojom::Role::kGroup, GetData(cache.GetFocus()).role);
  ASSERT_EQ(ax_child, cache.GetFocus());

  // Ignore should cause focus to move upwards.
  child->GetViewAccessibility().SetIsIgnored(true);
  ASSERT_EQ(ax::mojom::Role::kTextField, GetData(cache.GetFocus()).role);
  ASSERT_EQ(ax_parent, cache.GetFocus());

  // Propagate focus to ancestor should also cause focus to move upward.
  parent->GetViewAccessibility().set_propagate_focus_to_ancestor(true);
  ASSERT_EQ(ax::mojom::Role::kWindow, GetData(cache.GetFocus()).role);
  ASSERT_EQ(cache.GetOrCreate(widget->GetRootView()), cache.GetFocus());

  cache.OnRootWindowObjDestroyed(widget->GetNativeWindow());
}

class TestingWidgetDelegateView : public WidgetDelegateView {
 public:
  explicit TestingWidgetDelegateView(base::RunLoop* run_loop)
      : run_loop_(run_loop) {}
  ~TestingWidgetDelegateView() override {
    NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged, false);
    run_loop_->QuitWhenIdle();
  }
  TestingWidgetDelegateView(const TestingWidgetDelegateView&) = delete;
  TestingWidgetDelegateView& operator=(const TestingWidgetDelegateView&) =
      delete;

 private:
  raw_ptr<base::RunLoop> run_loop_;
};

class TestingAXEventObserver : public AXEventObserver {
 public:
  explicit TestingAXEventObserver(AXAuraObjCache* cache) : cache_(cache) {
    observation_.Observe(AXEventManager::Get());
  }
  ~TestingAXEventObserver() override = default;
  TestingAXEventObserver(const TestingAXEventObserver&) = delete;
  TestingAXEventObserver& operator=(const TestingAXEventObserver&) = delete;

 private:
  void OnViewEvent(View* view, ax::mojom::Event event_type) override {
    AXAuraObjWrapper* ax_view = cache_->GetOrCreate(view);
    while (ax_view != nullptr) {
      ax_view = ax_view->GetParent();
    }
  }

  raw_ptr<AXAuraObjCache> cache_;
  base::ScopedObservation<AXEventManager, AXEventObserver> observation_{this};
};

TEST_F(AXAuraObjCacheTest, DoNotCreateWidgetWrapperOnDestroyed) {
  AXAuraObjCache cache;
  TestingAXEventObserver observer(&cache);
  auto widget = std::make_unique<Widget>();

  base::RunLoop run_loop;
  auto delegate = std::make_unique<TestingWidgetDelegateView>(&run_loop);

  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.delegate = delegate.release();
  widget->Init(std::move(params));
  widget->Show();
  auto* widget_ptr = widget.get();

  EXPECT_NE(ui::kInvalidAXNodeID, cache.GetID(widget_ptr));

  // NativeWidget is closed asynchronously.
  widget.reset();
  run_loop.Run();

  EXPECT_EQ(ui::kInvalidAXNodeID, cache.GetID(widget_ptr));
}

TEST_F(AXAuraObjCacheTest, VirtualViews) {
  AXAuraObjCache cache;
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  widget->Init(std::move(params));
  widget->Show();

  auto* parent = widget->GetRootView()->AddChildView(std::make_unique<View>());
  auto virtual_label = std::make_unique<AXVirtualView>();
  auto* virtual_label_ptr = virtual_label.get();
  virtual_label->GetCustomData().role = ax::mojom::Role::kStaticText;
  virtual_label->GetCustomData().SetNameChecked("Label");
  parent->GetViewAccessibility().AddVirtualChildView(std::move(virtual_label));

  AXVirtualViewWrapper* wrapper = virtual_label_ptr->GetOrCreateWrapper(&cache);
  ui::AXNodeID id = wrapper->GetUniqueId();
  AXAuraObjWrapper* wrapper2 = cache.Get(id);
  EXPECT_EQ(wrapper, wrapper2);

  parent->GetViewAccessibility().RemoveVirtualChildView(virtual_label_ptr);
  EXPECT_EQ(nullptr, cache.Get(id));
}

}  // namespace
}  // namespace views::test
