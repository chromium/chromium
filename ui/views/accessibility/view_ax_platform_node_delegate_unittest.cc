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
  TestButton() : Button(NULL) {}
  ~TestButton() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestButton);
};

}  // namespace

class ViewAXPlatformNodeDelegateTest : public ViewsTestBase {
 public:
  ViewAXPlatformNodeDelegateTest() = default;
  ~ViewAXPlatformNodeDelegateTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    widget_ = new Widget;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.bounds = gfx::Rect(0, 0, 200, 200);
    widget_->Init(params);

    button_ = new TestButton();
    button_->SetSize(gfx::Size(20, 20));

    label_ = new Label();
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

  bool SetFocused(ViewAXPlatformNodeDelegate* ax_delegate, bool focused) {
    ui::AXActionData data;
    data.action =
        focused ? ax::mojom::Action::kFocus : ax::mojom::Action::kBlur;
    return ax_delegate->AccessibilityPerformAction(data);
  }

 protected:
  Widget* widget_ = nullptr;
  Button* button_ = nullptr;
  Label* label_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(ViewAXPlatformNodeDelegateTest);
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
  gfx::Rect bounds =
      gfx::ToEnclosingRect(button_accessibility()->GetData().location);
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
  EXPECT_EQ(View::FocusBehavior::ACCESSIBLE_ONLY, button_->focus_behavior());
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

#if defined(USE_AURA)
class DerivedTestView : public View {
 public:
  DerivedTestView() : View() {}
  ~DerivedTestView() override = default;

  void OnBlur() override { SetVisible(false); }
};

class AxTestViewsDelegate : public TestViewsDelegate {
 public:
  AxTestViewsDelegate() = default;
  ~AxTestViewsDelegate() override = default;

  void NotifyAccessibilityEvent(View* view,
                                ax::mojom::Event event_type) override {
    AXAuraObjCache* ax = AXAuraObjCache::GetInstance();
    std::vector<AXAuraObjWrapper*> out_children;
    AXAuraObjWrapper* ax_obj = ax->GetOrCreate(view->GetWidget());
    ax_obj->GetChildren(&out_children);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AxTestViewsDelegate);
};

class ViewAccessibilityTest : public ViewsTestBase {
 public:
  ViewAccessibilityTest() = default;
  ~ViewAccessibilityTest() override = default;
  void SetUp() override {
    std::unique_ptr<TestViewsDelegate> views_delegate(
        new AxTestViewsDelegate());
    set_views_delegate(std::move(views_delegate));
    ViewsTestBase::SetUp();
  }
};

// Check if the destruction of the widget ends successfully if |view|'s
// visibility changed during destruction.
TEST_F(ViewAccessibilityTest, LayoutCalledInvalidateRootView) {
  std::unique_ptr<Widget> widget(new Widget);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget->Init(params);
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
  AXAuraObjCache* ax = AXAuraObjCache::GetInstance();
  ax->GetOrCreate(widget.get());
}
#endif

}  // namespace test
}  // namespace views
