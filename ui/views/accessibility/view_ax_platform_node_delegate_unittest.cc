// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/view_ax_platform_node_delegate.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/accessibility/ax_event_observer.h"
#include "ui/views/accessibility/ax_widget_obj_wrapper.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace test {

namespace {

class TestButton : public Button {
 public:
  TestButton() : Button(nullptr) {}
  TestButton(const TestButton&) = delete;
  TestButton& operator=(const TestButton&) = delete;
  ~TestButton() override = default;
};

}  // namespace

class ViewAXPlatformNodeDelegateTest : public ViewsTestBase {
 public:
  ViewAXPlatformNodeDelegateTest() = default;
  ViewAXPlatformNodeDelegateTest(const ViewAXPlatformNodeDelegateTest&) =
      delete;
  ViewAXPlatformNodeDelegateTest& operator=(
      const ViewAXPlatformNodeDelegateTest&) = delete;
  ~ViewAXPlatformNodeDelegateTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    widget_ = new Widget;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.bounds = gfx::Rect(0, 0, 200, 200);
    widget_->Init(std::move(params));

    button_ = new TestButton();
    button_->SetID(NON_DEFAULT_VIEW_ID);
    button_->SetSize(gfx::Size(20, 20));

    label_ = new Label();
    label_->SetID(DEFAULT_VIEW_ID);
    button_->AddChildView(label_);

    widget_->GetContentsView()->AddChildView(button_);
    widget_->Show();
  }

  void TearDown() override {
    if (!widget_->IsClosed())
      widget_->Close();
    ViewsTestBase::TearDown();
  }

  ViewAXPlatformNodeDelegate* button_accessibility() {
    return static_cast<ViewAXPlatformNodeDelegate*>(
        &button_->GetViewAccessibility());
  }

  ViewAXPlatformNodeDelegate* label_accessibility() {
    return static_cast<ViewAXPlatformNodeDelegate*>(
        &label_->GetViewAccessibility());
  }

  ViewAXPlatformNodeDelegate* view_accessibility(View* view) {
    return static_cast<ViewAXPlatformNodeDelegate*>(
        &view->GetViewAccessibility());
  }

  bool SetFocused(ViewAXPlatformNodeDelegate* ax_delegate, bool focused) {
    ui::AXActionData data;
    data.action =
        focused ? ax::mojom::Action::kFocus : ax::mojom::Action::kBlur;
    return ax_delegate->AccessibilityPerformAction(data);
  }

  // Sets up a more complicated structure of Views - one parent View with four
  // child Views.
  std::vector<View*> SetUpExtraViews() {
    View* parent_view = new View();
    widget_->GetContentsView()->AddChildView(parent_view);
    std::vector<View*> views{parent_view};

    const int num_children = 4;
    for (int i = 0; i < num_children; i++) {
      View* child_view = new View();
      parent_view->AddChildView(child_view);
      views.push_back(child_view);
    }
    return views;
  }

  // Adds group id information to the first 5 values in |views|. If |views| is
  // empty, populates it with one parent View and four child Views. It is
  // assumed |views| is either empty or has at least 5 items.
  void SetUpExtraViewsWithGroups(std::vector<View*>& views) {
    //                v[0] g1
    //     |        |        |      |
    // v[1] g1  v[2] g1  v[3] g2  v[4]
    if (views.empty())
      views = SetUpExtraViews();
    EXPECT_GE(views.size(), (size_t)5);

    views[0]->SetGroup(1);
    views[1]->SetGroup(1);
    views[2]->SetGroup(1);
    views[3]->SetGroup(2);
    // Skip views[4] - no group id.
  }

  // Adds posInSet and setSize overrides to the first 5 values in |views|. If
  // |views| is empty, populates it with one parent View and four child Views.
  // It is assumed |views| is either empty or has at least 5 items.
  void SetUpExtraViewsWithSetOverrides(std::vector<View*>& views) {
    //                     v[0] p4 s4
    //      |            |            |            |
    //  v[1] p3 s4   v[2] p2 s4   v[3] p- s-   v[4] p1 s4
    if (views.empty())
      views = SetUpExtraViews();
    EXPECT_GE(views.size(), (size_t)5);

    views[0]->GetViewAccessibility().OverridePosInSet(4, 4);
    views[1]->GetViewAccessibility().OverridePosInSet(3, 4);
    views[2]->GetViewAccessibility().OverridePosInSet(2, 4);
    // Skip views[3] - no override.
    views[4]->GetViewAccessibility().OverridePosInSet(1, 4);
  }

 protected:
  const int DEFAULT_VIEW_ID = 0;
  const int NON_DEFAULT_VIEW_ID = 1;

  Widget* widget_ = nullptr;
  Button* button_ = nullptr;
  Label* label_ = nullptr;
};

TEST_F(ViewAXPlatformNodeDelegateTest, RoleShouldMatch) {
  EXPECT_EQ(ax::mojom::Role::kButton, button_accessibility()->GetData().role);
  // Since the label is a subview of |button_|, and the button is keyboard
  // focusable, the label is assumed to form part of the button and not have a
  // role of its own.
  EXPECT_EQ(ax::mojom::Role::kIgnored, label_accessibility()->GetData().role);
  // This will happen for all potentially keyboard-focusable Views with
  // non-keyboard-focusable children, so if we make the button unfocusable, the
  // label will be allowed to have its own role again.
  button_->SetFocusBehavior(View::FocusBehavior::NEVER);
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            label_accessibility()->GetData().role);
}

TEST_F(ViewAXPlatformNodeDelegateTest, BoundsShouldMatch) {
  gfx::Rect bounds = gfx::ToEnclosingRect(
      button_accessibility()->GetData().relative_bounds.bounds);
  gfx::Rect screen_bounds =
      button_accessibility()->GetUnclippedScreenBoundsRect();

  EXPECT_EQ(button_->GetBoundsInScreen(), bounds);
  EXPECT_EQ(screen_bounds, bounds);
}

TEST_F(ViewAXPlatformNodeDelegateTest, LabelIsChildOfButton) {
  // Disable focus rings for this test: they introduce extra children that can
  // be either before or after the label, which complicates correctness testing.
  button_->SetInstallFocusRingOnFocus(false);

  // |button_| is focusable, so |label_| (as its child) should be ignored.
  EXPECT_EQ(View::FocusBehavior::ACCESSIBLE_ONLY, button_->GetFocusBehavior());
  EXPECT_EQ(1, button_accessibility()->GetChildCount());
  EXPECT_EQ(button_->GetNativeViewAccessible(),
            label_accessibility()->GetParent());
  EXPECT_EQ(ax::mojom::Role::kIgnored, label_accessibility()->GetData().role);

  // If |button_| is no longer focusable, |label_| should show up again.
  button_->SetFocusBehavior(View::FocusBehavior::NEVER);
  EXPECT_EQ(1, button_accessibility()->GetChildCount());
  EXPECT_EQ(label_->GetNativeViewAccessible(),
            button_accessibility()->ChildAtIndex(0));
  EXPECT_EQ(button_->GetNativeViewAccessible(),
            label_accessibility()->GetParent());
  EXPECT_NE(ax::mojom::Role::kIgnored, label_accessibility()->GetData().role);
}

// Verify Views with invisible ancestors have ax::mojom::State::kInvisible.
TEST_F(ViewAXPlatformNodeDelegateTest, InvisibleViews) {
  EXPECT_TRUE(widget_->IsVisible());
  EXPECT_FALSE(
      button_accessibility()->GetData().HasState(ax::mojom::State::kInvisible));
  EXPECT_FALSE(
      label_accessibility()->GetData().HasState(ax::mojom::State::kInvisible));
  button_->SetVisible(false);
  EXPECT_TRUE(
      button_accessibility()->GetData().HasState(ax::mojom::State::kInvisible));
  EXPECT_TRUE(
      label_accessibility()->GetData().HasState(ax::mojom::State::kInvisible));
}

TEST_F(ViewAXPlatformNodeDelegateTest, WritableFocus) {
  // Make |button_| focusable, and focus/unfocus it via
  // ViewAXPlatformNodeDelegate.
  button_->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  EXPECT_EQ(nullptr, button_->GetFocusManager()->GetFocusedView());
  EXPECT_EQ(nullptr, button_accessibility()->GetFocus());
  EXPECT_TRUE(SetFocused(button_accessibility(), true));
  EXPECT_EQ(button_, button_->GetFocusManager()->GetFocusedView());
  EXPECT_EQ(button_->GetNativeViewAccessible(),
            button_accessibility()->GetFocus());
  EXPECT_TRUE(SetFocused(button_accessibility(), false));
  EXPECT_EQ(nullptr, button_->GetFocusManager()->GetFocusedView());
  EXPECT_EQ(nullptr, button_accessibility()->GetFocus());

  // If not focusable at all, SetFocused() should return false.
  button_->SetEnabled(false);
  EXPECT_FALSE(SetFocused(button_accessibility(), true));
}

TEST_F(ViewAXPlatformNodeDelegateTest, GetAuthorUniqueIdDefault) {
  ASSERT_EQ(base::WideToUTF16(L""), label_accessibility()->GetAuthorUniqueId());
}

TEST_F(ViewAXPlatformNodeDelegateTest, GetAuthorUniqueIdNonDefault) {
  ASSERT_EQ(base::WideToUTF16(L"view_1"),
            button_accessibility()->GetAuthorUniqueId());
}

TEST_F(ViewAXPlatformNodeDelegateTest, IsOrderedSet) {
  std::vector<View*> group_ids;
  SetUpExtraViewsWithGroups(group_ids);
  // Only last element has no group id.
  EXPECT_TRUE(view_accessibility(group_ids[0])->IsOrderedSet());
  EXPECT_TRUE(view_accessibility(group_ids[1])->IsOrderedSet());
  EXPECT_TRUE(view_accessibility(group_ids[2])->IsOrderedSet());
  EXPECT_TRUE(view_accessibility(group_ids[3])->IsOrderedSet());
  EXPECT_FALSE(view_accessibility(group_ids[4])->IsOrderedSet());

  EXPECT_TRUE(view_accessibility(group_ids[0])->IsOrderedSetItem());
  EXPECT_TRUE(view_accessibility(group_ids[1])->IsOrderedSetItem());
  EXPECT_TRUE(view_accessibility(group_ids[2])->IsOrderedSetItem());
  EXPECT_TRUE(view_accessibility(group_ids[3])->IsOrderedSetItem());
  EXPECT_FALSE(view_accessibility(group_ids[4])->IsOrderedSetItem());

  std::vector<View*> overrides;
  SetUpExtraViewsWithSetOverrides(overrides);
  // Only overrides[3] has no override values for setSize/ posInSet.
  EXPECT_TRUE(view_accessibility(overrides[0])->IsOrderedSet());
  EXPECT_TRUE(view_accessibility(overrides[1])->IsOrderedSet());
  EXPECT_TRUE(view_accessibility(overrides[2])->IsOrderedSet());
  EXPECT_FALSE(view_accessibility(overrides[3])->IsOrderedSet());
  EXPECT_TRUE(view_accessibility(overrides[4])->IsOrderedSet());

  EXPECT_TRUE(view_accessibility(overrides[0])->IsOrderedSetItem());
  EXPECT_TRUE(view_accessibility(overrides[1])->IsOrderedSetItem());
  EXPECT_TRUE(view_accessibility(overrides[2])->IsOrderedSetItem());
  EXPECT_FALSE(view_accessibility(overrides[3])->IsOrderedSetItem());
  EXPECT_TRUE(view_accessibility(overrides[4])->IsOrderedSetItem());
}

TEST_F(ViewAXPlatformNodeDelegateTest, SetSizeAndPosition) {
  // Test Views with group ids.
  std::vector<View*> group_ids;
  SetUpExtraViewsWithGroups(group_ids);
  EXPECT_EQ(view_accessibility(group_ids[0])->GetSetSize(), 3);
  EXPECT_EQ(view_accessibility(group_ids[0])->GetPosInSet(), 1);
  EXPECT_EQ(view_accessibility(group_ids[1])->GetSetSize(), 3);
  EXPECT_EQ(view_accessibility(group_ids[1])->GetPosInSet(), 2);
  EXPECT_EQ(view_accessibility(group_ids[2])->GetSetSize(), 3);
  EXPECT_EQ(view_accessibility(group_ids[2])->GetPosInSet(), 3);

  EXPECT_EQ(view_accessibility(group_ids[3])->GetSetSize(), 1);
  EXPECT_EQ(view_accessibility(group_ids[3])->GetPosInSet(), 1);

  EXPECT_FALSE(view_accessibility(group_ids[4])->GetSetSize().has_value());
  EXPECT_FALSE(view_accessibility(group_ids[4])->GetPosInSet().has_value());

  // Check if a View is ignored, it is not counted in SetSize or PosInSet
  group_ids[1]->GetViewAccessibility().OverrideIsIgnored(true);
  group_ids[2]->GetViewAccessibility().OverrideIsIgnored(true);
  EXPECT_EQ(view_accessibility(group_ids[0])->GetSetSize(), 1);
  EXPECT_EQ(view_accessibility(group_ids[0])->GetPosInSet(), 1);
  EXPECT_FALSE(view_accessibility(group_ids[1])->GetSetSize().has_value());
  EXPECT_FALSE(view_accessibility(group_ids[1])->GetPosInSet().has_value());
  EXPECT_FALSE(view_accessibility(group_ids[2])->GetSetSize().has_value());
  EXPECT_FALSE(view_accessibility(group_ids[2])->GetPosInSet().has_value());
  group_ids[1]->GetViewAccessibility().OverrideIsIgnored(false);
  group_ids[2]->GetViewAccessibility().OverrideIsIgnored(false);

  // Test Views with setSize/ posInSet override values set.
  std::vector<View*> overrides;
  SetUpExtraViewsWithSetOverrides(overrides);
  EXPECT_EQ(view_accessibility(overrides[0])->GetSetSize(), 4);
  EXPECT_EQ(view_accessibility(overrides[0])->GetPosInSet(), 4);
  EXPECT_EQ(view_accessibility(overrides[1])->GetSetSize(), 4);
  EXPECT_EQ(view_accessibility(overrides[1])->GetPosInSet(), 3);
  EXPECT_EQ(view_accessibility(overrides[2])->GetSetSize(), 4);
  EXPECT_EQ(view_accessibility(overrides[2])->GetPosInSet(), 2);

  EXPECT_FALSE(view_accessibility(overrides[3])->GetSetSize().has_value());
  EXPECT_FALSE(view_accessibility(overrides[3])->GetPosInSet().has_value());

  EXPECT_EQ(view_accessibility(overrides[4])->GetSetSize(), 4);
  EXPECT_EQ(view_accessibility(overrides[4])->GetPosInSet(), 1);

  // Test Views with both group ids and setSize/ posInSet override values set.
  // Make sure the override values take precedence when both are set.
  // Add setSize/ posInSet overrides to the Views with group ids.
  SetUpExtraViewsWithSetOverrides(group_ids);
  EXPECT_EQ(view_accessibility(group_ids[0])->GetSetSize(), 4);
  EXPECT_EQ(view_accessibility(group_ids[0])->GetPosInSet(), 4);
  EXPECT_EQ(view_accessibility(group_ids[1])->GetSetSize(), 4);
  EXPECT_EQ(view_accessibility(group_ids[1])->GetPosInSet(), 3);
  EXPECT_EQ(view_accessibility(group_ids[2])->GetSetSize(), 4);
  EXPECT_EQ(view_accessibility(group_ids[2])->GetPosInSet(), 2);

  EXPECT_EQ(view_accessibility(group_ids[3])->GetSetSize(), 1);
  EXPECT_EQ(view_accessibility(group_ids[3])->GetPosInSet(), 1);

  EXPECT_EQ(view_accessibility(group_ids[4])->GetSetSize(), 4);
  EXPECT_EQ(view_accessibility(group_ids[4])->GetPosInSet(), 1);
}

TEST_F(ViewAXPlatformNodeDelegateTest, Navigation) {
  std::vector<View*> view_ids = SetUpExtraViews();

  EXPECT_EQ(view_accessibility(view_ids[0])->GetNextSibling(), nullptr);
  EXPECT_EQ(view_accessibility(view_ids[0])->GetPreviousSibling(),
            view_accessibility(button_)->GetNativeObject());
  EXPECT_EQ(view_accessibility(view_ids[0])->GetIndexInParent(), 3);

  EXPECT_EQ(view_accessibility(view_ids[1])->GetNextSibling(),
            view_accessibility(view_ids[2])->GetNativeObject());
  EXPECT_EQ(view_accessibility(view_ids[1])->GetPreviousSibling(), nullptr);
  EXPECT_EQ(view_accessibility(view_ids[1])->GetIndexInParent(), 0);

  EXPECT_EQ(view_accessibility(view_ids[2])->GetNextSibling(),
            view_accessibility(view_ids[3])->GetNativeObject());
  EXPECT_EQ(view_accessibility(view_ids[2])->GetPreviousSibling(),
            view_accessibility(view_ids[1])->GetNativeObject());
  EXPECT_EQ(view_accessibility(view_ids[2])->GetIndexInParent(), 1);

  EXPECT_EQ(view_accessibility(view_ids[3])->GetNextSibling(),
            view_accessibility(view_ids[4])->GetNativeObject());
  EXPECT_EQ(view_accessibility(view_ids[3])->GetPreviousSibling(),
            view_accessibility(view_ids[2])->GetNativeObject());
  EXPECT_EQ(view_accessibility(view_ids[3])->GetIndexInParent(), 2);

  EXPECT_EQ(view_accessibility(view_ids[4])->GetNextSibling(), nullptr);
  EXPECT_EQ(view_accessibility(view_ids[4])->GetPreviousSibling(),
            view_accessibility(view_ids[3])->GetNativeObject());
  EXPECT_EQ(view_accessibility(view_ids[4])->GetIndexInParent(), 3);
}

TEST_F(ViewAXPlatformNodeDelegateTest, OverrideHasPopup) {
  std::vector<View*> view_ids = SetUpExtraViews();

  view_ids[1]->GetViewAccessibility().OverrideHasPopup(
      ax::mojom::HasPopup::kTrue);
  view_ids[2]->GetViewAccessibility().OverrideHasPopup(
      ax::mojom::HasPopup::kMenu);

  ui::AXNodeData node_data_0;
  view_ids[0]->GetViewAccessibility().GetAccessibleNodeData(&node_data_0);
  EXPECT_EQ(node_data_0.GetHasPopup(), ax::mojom::HasPopup::kFalse);

  ui::AXNodeData node_data_1;
  view_ids[1]->GetViewAccessibility().GetAccessibleNodeData(&node_data_1);
  EXPECT_EQ(node_data_1.GetHasPopup(), ax::mojom::HasPopup::kTrue);

  ui::AXNodeData node_data_2;
  view_ids[2]->GetViewAccessibility().GetAccessibleNodeData(&node_data_2);
  EXPECT_EQ(node_data_2.GetHasPopup(), ax::mojom::HasPopup::kMenu);
}

#if defined(USE_AURA)
class DerivedTestView : public View {
 public:
  DerivedTestView() = default;
  ~DerivedTestView() override = default;

  void OnBlur() override { SetVisible(false); }
};

class TestAXEventObserver : public AXEventObserver {
 public:
  explicit TestAXEventObserver(AXAuraObjCache* cache) : cache_(cache) {
    AXEventManager::Get()->AddObserver(this);
  }
  TestAXEventObserver(const TestAXEventObserver&) = delete;
  TestAXEventObserver& operator=(const TestAXEventObserver&) = delete;
  ~TestAXEventObserver() override {
    AXEventManager::Get()->RemoveObserver(this);
  }

  // AXEventObserver:
  void OnViewEvent(View* view, ax::mojom::Event event_type) override {
    std::vector<AXAuraObjWrapper*> out_children;
    AXAuraObjWrapper* ax_obj = cache_->GetOrCreate(view->GetWidget());
    ax_obj->GetChildren(&out_children);
  }

 private:
  AXAuraObjCache* cache_;
};

using ViewAccessibilityTest = ViewsTestBase;

// Check if the destruction of the widget ends successfully if |view|'s
// visibility changed during destruction.
TEST_F(ViewAccessibilityTest, LayoutCalledInvalidateRootView) {
  // TODO: Construct a real AutomationManagerAura rather than using this
  // observer to simulate it.
  AXAuraObjCache cache;
  TestAXEventObserver observer(&cache);
  std::unique_ptr<Widget> widget(new Widget);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget->Init(std::move(params));
  widget->Show();

  View* root = widget->GetRootView();
  DerivedTestView* parent = new DerivedTestView();
  DerivedTestView* child = new DerivedTestView();
  root->AddChildView(parent);
  parent->AddChildView(child);
  child->SetFocusBehavior(DerivedTestView::FocusBehavior::ALWAYS);
  parent->SetFocusBehavior(DerivedTestView::FocusBehavior::ALWAYS);
  root->SetFocusBehavior(DerivedTestView::FocusBehavior::ALWAYS);
  parent->RequestFocus();
  // During the destruction of parent, OnBlur will be called and change the
  // visibility to false.
  parent->SetVisible(true);

  cache.GetOrCreate(widget.get());
}
#endif

}  // namespace test
}  // namespace views
