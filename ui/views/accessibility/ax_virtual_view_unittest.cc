// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_virtual_view.h"

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_ax_platform_node_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "ui/views/win/hwnd_util.h"
#endif

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

class AXVirtualViewTest : public ViewsTestBase {
 public:
  AXVirtualViewTest() = default;
  AXVirtualViewTest(const AXVirtualViewTest&) = delete;
  AXVirtualViewTest& operator=(const AXVirtualViewTest&) = delete;
  ~AXVirtualViewTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    widget_ = new Widget;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.bounds = gfx::Rect(0, 0, 200, 200);
    widget_->Init(std::move(params));
    button_ = new TestButton;
    button_->SetSize(gfx::Size(20, 20));
    widget_->GetContentsView()->AddChildView(button_);
    virtual_label_ = new AXVirtualView;
    virtual_label_->GetCustomData().role = ax::mojom::Role::kStaticText;
    virtual_label_->GetCustomData().SetName("Label");
    button_->GetViewAccessibility().AddVirtualChildView(
        base::WrapUnique(virtual_label_));
    widget_->Show();
  }

  void TearDown() override {
    if (!widget_->IsClosed())
      widget_->Close();
    ViewsTestBase::TearDown();
  }

  ViewAXPlatformNodeDelegate* GetButtonAccessibility() {
    return static_cast<ViewAXPlatformNodeDelegate*>(
        &button_->GetViewAccessibility());
  }

 protected:
  Widget* widget_;
  Button* button_;
  // Weak, |button_| owns this.
  AXVirtualView* virtual_label_;
};

TEST_F(AXVirtualViewTest, AccessibilityRoleAndName) {
  EXPECT_EQ(ax::mojom::Role::kButton, GetButtonAccessibility()->GetData().role);
  EXPECT_EQ(ax::mojom::Role::kStaticText, virtual_label_->GetData().role);
  EXPECT_EQ("Label", virtual_label_->GetData().GetStringAttribute(
                         ax::mojom::StringAttribute::kName));
}

// The focusable state of a virtual view should not depend on the focusable
// state of the real view ancestor, however the enabled state should.
TEST_F(AXVirtualViewTest, FocusableAndEnabledState) {
  virtual_label_->GetCustomData().AddState(ax::mojom::State::kFocusable);
  EXPECT_TRUE(GetButtonAccessibility()->GetData().HasState(
      ax::mojom::State::kFocusable));
  EXPECT_TRUE(virtual_label_->GetData().HasState(ax::mojom::State::kFocusable));
  EXPECT_EQ(ax::mojom::Restriction::kNone,
            GetButtonAccessibility()->GetData().GetRestriction());
  EXPECT_EQ(ax::mojom::Restriction::kNone,
            virtual_label_->GetData().GetRestriction());

  button_->SetFocusBehavior(View::FocusBehavior::NEVER);
  EXPECT_FALSE(GetButtonAccessibility()->GetData().HasState(
      ax::mojom::State::kFocusable));
  EXPECT_TRUE(virtual_label_->GetData().HasState(ax::mojom::State::kFocusable));
  EXPECT_EQ(ax::mojom::Restriction::kNone,
            GetButtonAccessibility()->GetData().GetRestriction());
  EXPECT_EQ(ax::mojom::Restriction::kNone,
            virtual_label_->GetData().GetRestriction());

  button_->SetEnabled(false);
  EXPECT_FALSE(GetButtonAccessibility()->GetData().HasState(
      ax::mojom::State::kFocusable));
  EXPECT_TRUE(virtual_label_->GetData().HasState(ax::mojom::State::kFocusable));
  EXPECT_EQ(ax::mojom::Restriction::kDisabled,
            GetButtonAccessibility()->GetData().GetRestriction());
  EXPECT_EQ(ax::mojom::Restriction::kDisabled,
            virtual_label_->GetData().GetRestriction());

  button_->SetEnabled(true);
  button_->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  virtual_label_->GetCustomData().RemoveState(ax::mojom::State::kFocusable);
  EXPECT_TRUE(GetButtonAccessibility()->GetData().HasState(
      ax::mojom::State::kFocusable));
  EXPECT_FALSE(
      virtual_label_->GetData().HasState(ax::mojom::State::kFocusable));
  EXPECT_EQ(ax::mojom::Restriction::kNone,
            GetButtonAccessibility()->GetData().GetRestriction());
  EXPECT_EQ(ax::mojom::Restriction::kNone,
            virtual_label_->GetData().GetRestriction());
}

TEST_F(AXVirtualViewTest, VirtualLabelIsChildOfButton) {
  EXPECT_EQ(1, GetButtonAccessibility()->GetChildCount());
  EXPECT_EQ(0, virtual_label_->GetChildCount());
  ASSERT_NE(nullptr, virtual_label_->GetParent());
  EXPECT_EQ(button_->GetNativeViewAccessible(), virtual_label_->GetParent());
  ASSERT_NE(nullptr, GetButtonAccessibility()->ChildAtIndex(0));
  EXPECT_EQ(virtual_label_->GetNativeObject(),
            GetButtonAccessibility()->ChildAtIndex(0));
}

TEST_F(AXVirtualViewTest, AddingAndRemovingVirtualChildren) {
  ASSERT_EQ(0, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_1 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_1));
  EXPECT_EQ(1, virtual_label_->GetChildCount());
  ASSERT_NE(nullptr, virtual_child_1->GetParent());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_1->GetParent());
  ASSERT_NE(nullptr, virtual_label_->ChildAtIndex(0));
  EXPECT_EQ(virtual_child_1->GetNativeObject(),
            virtual_label_->ChildAtIndex(0));

  AXVirtualView* virtual_child_2 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_2));
  EXPECT_EQ(2, virtual_label_->GetChildCount());
  ASSERT_NE(nullptr, virtual_child_2->GetParent());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_2->GetParent());
  ASSERT_NE(nullptr, virtual_label_->ChildAtIndex(1));
  EXPECT_EQ(virtual_child_2->GetNativeObject(),
            virtual_label_->ChildAtIndex(1));

  AXVirtualView* virtual_child_3 = new AXVirtualView;
  virtual_child_2->AddChildView(base::WrapUnique(virtual_child_3));
  EXPECT_EQ(2, virtual_label_->GetChildCount());
  EXPECT_EQ(0, virtual_child_1->GetChildCount());
  EXPECT_EQ(1, virtual_child_2->GetChildCount());
  ASSERT_NE(nullptr, virtual_child_3->GetParent());
  EXPECT_EQ(virtual_child_2->GetNativeObject(), virtual_child_3->GetParent());
  ASSERT_NE(nullptr, virtual_child_2->ChildAtIndex(0));
  EXPECT_EQ(virtual_child_3->GetNativeObject(),
            virtual_child_2->ChildAtIndex(0));

  virtual_child_2->RemoveChildView(virtual_child_3);
  EXPECT_EQ(0, virtual_child_2->GetChildCount());
  EXPECT_EQ(2, virtual_label_->GetChildCount());

  virtual_label_->RemoveAllChildViews();
  EXPECT_EQ(0, virtual_label_->GetChildCount());
}

TEST_F(AXVirtualViewTest, ReorderingVirtualChildren) {
  ASSERT_EQ(0, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_1 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_1));
  ASSERT_EQ(1, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_2 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_2));
  ASSERT_EQ(2, virtual_label_->GetChildCount());

  virtual_label_->ReorderChildView(virtual_child_1, -1);
  ASSERT_EQ(2, virtual_label_->GetChildCount());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_2->GetParent());
  ASSERT_NE(nullptr, virtual_label_->ChildAtIndex(0));
  EXPECT_EQ(virtual_child_2->GetNativeObject(),
            virtual_label_->ChildAtIndex(0));
  ASSERT_NE(nullptr, virtual_label_->ChildAtIndex(1));
  EXPECT_EQ(virtual_child_1->GetNativeObject(),
            virtual_label_->ChildAtIndex(1));

  virtual_label_->ReorderChildView(virtual_child_1, 0);
  ASSERT_EQ(2, virtual_label_->GetChildCount());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_1->GetParent());
  ASSERT_NE(nullptr, virtual_label_->ChildAtIndex(0));
  EXPECT_EQ(virtual_child_1->GetNativeObject(),
            virtual_label_->ChildAtIndex(0));
  ASSERT_NE(nullptr, virtual_label_->ChildAtIndex(1));
  EXPECT_EQ(virtual_child_2->GetNativeObject(),
            virtual_label_->ChildAtIndex(1));

  virtual_label_->RemoveAllChildViews();
  ASSERT_EQ(0, virtual_label_->GetChildCount());
}

TEST_F(AXVirtualViewTest, ContainsVirtualChild) {
  ASSERT_EQ(0, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_1 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_1));
  ASSERT_EQ(1, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_2 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_2));
  ASSERT_EQ(2, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_3 = new AXVirtualView;
  virtual_child_2->AddChildView(base::WrapUnique(virtual_child_3));
  ASSERT_EQ(1, virtual_child_2->GetChildCount());

  EXPECT_TRUE(button_->GetViewAccessibility().Contains(virtual_label_));
  EXPECT_TRUE(virtual_label_->Contains(virtual_label_));
  EXPECT_TRUE(virtual_label_->Contains(virtual_child_1));
  EXPECT_TRUE(virtual_label_->Contains(virtual_child_2));
  EXPECT_TRUE(virtual_label_->Contains(virtual_child_3));
  EXPECT_TRUE(virtual_child_2->Contains(virtual_child_2));
  EXPECT_TRUE(virtual_child_2->Contains(virtual_child_3));

  EXPECT_FALSE(virtual_child_1->Contains(virtual_label_));
  EXPECT_FALSE(virtual_child_2->Contains(virtual_label_));
  EXPECT_FALSE(virtual_child_3->Contains(virtual_child_2));

  virtual_label_->RemoveAllChildViews();
  ASSERT_EQ(0, virtual_label_->GetChildCount());
}

TEST_F(AXVirtualViewTest, GetIndexOfVirtualChild) {
  ASSERT_EQ(0, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_1 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_1));
  ASSERT_EQ(1, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_2 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_2));
  ASSERT_EQ(2, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_3 = new AXVirtualView;
  virtual_child_2->AddChildView(base::WrapUnique(virtual_child_3));
  ASSERT_EQ(1, virtual_child_2->GetChildCount());

  EXPECT_EQ(-1, virtual_label_->GetIndexOf(virtual_label_));
  EXPECT_EQ(0, virtual_label_->GetIndexOf(virtual_child_1));
  EXPECT_EQ(1, virtual_label_->GetIndexOf(virtual_child_2));
  EXPECT_EQ(-1, virtual_label_->GetIndexOf(virtual_child_3));
  EXPECT_EQ(0, virtual_child_2->GetIndexOf(virtual_child_3));

  virtual_label_->RemoveAllChildViews();
  ASSERT_EQ(0, virtual_label_->GetChildCount());
}

// Verify that virtual views with invisible ancestors inherit the
// ax::mojom::State::kInvisible state.
TEST_F(AXVirtualViewTest, InvisibleVirtualViews) {
  EXPECT_TRUE(widget_->IsVisible());
  EXPECT_FALSE(GetButtonAccessibility()->GetData().HasState(
      ax::mojom::State::kInvisible));
  EXPECT_FALSE(
      virtual_label_->GetData().HasState(ax::mojom::State::kInvisible));

  button_->SetVisible(false);
  EXPECT_TRUE(GetButtonAccessibility()->GetData().HasState(
      ax::mojom::State::kInvisible));
  EXPECT_TRUE(virtual_label_->GetData().HasState(ax::mojom::State::kInvisible));
  button_->SetVisible(true);
}

TEST_F(AXVirtualViewTest, OverrideFocus) {
  ViewAccessibility& button_accessibility = button_->GetViewAccessibility();
  ASSERT_NE(nullptr, button_accessibility.GetNativeObject());
  ASSERT_NE(nullptr, virtual_label_->GetNativeObject());

  EXPECT_EQ(button_accessibility.GetNativeObject(),
            button_accessibility.GetFocusedDescendant());
  button_accessibility.OverrideFocus(virtual_label_);
  EXPECT_EQ(virtual_label_->GetNativeObject(),
            button_accessibility.GetFocusedDescendant());
  button_accessibility.OverrideFocus(nullptr);
  EXPECT_EQ(button_accessibility.GetNativeObject(),
            button_accessibility.GetFocusedDescendant());

  ASSERT_EQ(0, virtual_label_->GetChildCount());
  AXVirtualView* virtual_child_1 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_1));
  ASSERT_EQ(1, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_2 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_2));
  ASSERT_EQ(2, virtual_label_->GetChildCount());

  button_accessibility.OverrideFocus(virtual_child_1);
  EXPECT_EQ(virtual_child_1->GetNativeObject(),
            button_accessibility.GetFocusedDescendant());

  AXVirtualView* virtual_child_3 = new AXVirtualView;
  virtual_child_2->AddChildView(base::WrapUnique(virtual_child_3));
  ASSERT_EQ(1, virtual_child_2->GetChildCount());

  EXPECT_EQ(virtual_child_1->GetNativeObject(),
            button_accessibility.GetFocusedDescendant());
  button_accessibility.OverrideFocus(virtual_child_3);
  EXPECT_EQ(virtual_child_3->GetNativeObject(),
            button_accessibility.GetFocusedDescendant());

  // Test that calling GetFocus() from any object in the tree will return the
  // same result.
  EXPECT_EQ(virtual_child_3->GetNativeObject(), virtual_label_->GetFocus());
  EXPECT_EQ(virtual_child_3->GetNativeObject(), virtual_child_1->GetFocus());
  EXPECT_EQ(virtual_child_3->GetNativeObject(), virtual_child_2->GetFocus());
  EXPECT_EQ(virtual_child_3->GetNativeObject(), virtual_child_3->GetFocus());

  virtual_label_->RemoveChildView(virtual_child_2);
  ASSERT_EQ(1, virtual_label_->GetChildCount());
  EXPECT_EQ(button_accessibility.GetNativeObject(),
            button_accessibility.GetFocusedDescendant());
  EXPECT_EQ(button_accessibility.GetNativeObject(), virtual_label_->GetFocus());
  EXPECT_EQ(button_accessibility.GetNativeObject(),
            virtual_child_1->GetFocus());

  button_accessibility.OverrideFocus(virtual_child_1);
  EXPECT_EQ(virtual_child_1->GetNativeObject(),
            button_accessibility.GetFocusedDescendant());
  virtual_label_->RemoveAllChildViews();
  ASSERT_EQ(0, virtual_label_->GetChildCount());
  EXPECT_EQ(button_accessibility.GetNativeObject(),
            button_accessibility.GetFocusedDescendant());
}

TEST_F(AXVirtualViewTest, Navigation) {
  ASSERT_EQ(0, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_1 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_1));
  EXPECT_EQ(1, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_2 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_2));
  EXPECT_EQ(2, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_3 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_3));

  AXVirtualView* virtual_child_4 = new AXVirtualView;
  virtual_child_2->AddChildView(base::WrapUnique(virtual_child_4));

  EXPECT_EQ(nullptr, virtual_label_->GetNextSibling());
  EXPECT_EQ(nullptr, virtual_label_->GetPreviousSibling());
  EXPECT_EQ(0, virtual_label_->GetIndexInParent());

  EXPECT_EQ(virtual_child_2->GetNativeObject(),
            virtual_child_1->GetNextSibling());
  EXPECT_EQ(nullptr, virtual_child_1->GetPreviousSibling());
  EXPECT_EQ(0, virtual_child_1->GetIndexInParent());

  EXPECT_EQ(virtual_child_3->GetNativeObject(),
            virtual_child_2->GetNextSibling());
  EXPECT_EQ(virtual_child_1->GetNativeObject(),
            virtual_child_2->GetPreviousSibling());
  EXPECT_EQ(1, virtual_child_2->GetIndexInParent());

  EXPECT_EQ(nullptr, virtual_child_3->GetNextSibling());
  EXPECT_EQ(virtual_child_2->GetNativeObject(),
            virtual_child_3->GetPreviousSibling());
  EXPECT_EQ(2, virtual_child_3->GetIndexInParent());

  EXPECT_EQ(nullptr, virtual_child_4->GetNextSibling());
  EXPECT_EQ(nullptr, virtual_child_4->GetPreviousSibling());
  EXPECT_EQ(0, virtual_child_4->GetIndexInParent());
}

// Test for GetTargetForNativeAccessibilityEvent().
#if defined(OS_WIN)
TEST_F(AXVirtualViewTest, GetTargetForEvents) {
  EXPECT_EQ(button_, virtual_label_->GetOwnerView());
  EXPECT_NE(nullptr, HWNDForView(virtual_label_->GetOwnerView()));
  EXPECT_EQ(HWNDForView(button_),
            virtual_label_->GetTargetForNativeAccessibilityEvent());
}
#endif

}  // namespace test
}  // namespace views
