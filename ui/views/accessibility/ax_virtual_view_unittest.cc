// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_virtual_view.h"

#include "base/memory/ptr_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_ax_platform_node_delegate.h"
#include "ui/views/controls/button/button.h"
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

class AXVirtualViewTest : public ViewsTestBase {
 public:
  AXVirtualViewTest() = default;
  ~AXVirtualViewTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    widget_ = new Widget;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.bounds = gfx::Rect(0, 0, 200, 200);
    widget_->Init(params);
    button_ = new TestButton;
    button_->SetSize(gfx::Size(20, 20));
    widget_->GetContentsView()->AddChildView(button_);
    virtual_label_ = new AXVirtualView;
    virtual_label_->OverrideRole(ax::mojom::Role::kStaticText);
    virtual_label_->OverrideName("Label");
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

 private:
  DISALLOW_COPY_AND_ASSIGN(AXVirtualViewTest);
};

TEST_F(AXVirtualViewTest, AccessibilityRoleAndName) {
  EXPECT_EQ(ax::mojom::Role::kButton, GetButtonAccessibility()->GetData().role);
  EXPECT_EQ(ax::mojom::Role::kStaticText, virtual_label_->GetData().role);
  EXPECT_EQ("Label", virtual_label_->GetData().GetStringAttribute(
                         ax::mojom::StringAttribute::kName));
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

}  // namespace test
}  // namespace views
