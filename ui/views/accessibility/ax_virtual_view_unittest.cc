// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_virtual_view.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_for_test.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/accessibility/view_ax_platform_node_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_WIN)
#include "ui/views/win/hwnd_util.h"
#endif

namespace views::test {

namespace {

class TestButton : public Button {
  METADATA_HEADER(TestButton, Button)

 public:
  TestButton() : Button(Button::PressedCallback()) {}
  TestButton(const TestButton&) = delete;
  TestButton& operator=(const TestButton&) = delete;
  ~TestButton() override = default;
};

BEGIN_METADATA(TestButton)
END_METADATA

}  // namespace

class AXVirtualViewTest : public ViewsTestBase {
 public:
  AXVirtualViewTest() : ax_mode_setter_(ui::kAXModeComplete) {}
  AXVirtualViewTest(const AXVirtualViewTest&) = delete;
  AXVirtualViewTest& operator=(const AXVirtualViewTest&) = delete;
  ~AXVirtualViewTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    widget_ = std::make_unique<Widget>();
    Widget::InitParams params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW);
    params.bounds = gfx::Rect(0, 0, 200, 200);
    widget_->Init(std::move(params));
    auto button = std::make_unique<TestButton>();
    button->SetSize(gfx::Size(20, 20));
    button->GetViewAccessibility().SetName(u"Button");
    button_ = widget_->GetContentsView()->AddChildView(std::move(button));
    auto virtual_label = std::make_unique<AXVirtualView>();
    virtual_label->GetCustomData().role = ax::mojom::Role::kStaticText;
    virtual_label->GetCustomData().SetNameChecked("Label");
    virtual_label_ = virtual_label.get();
    button_->GetViewAccessibility().AddVirtualChildView(
        std::move(virtual_label));
    widget_->Show();

    ViewAccessibility::AccessibilityEventsCallback
        accessibility_events_callback = base::BindRepeating(
            [](std::vector<std::pair<const ui::AXPlatformNodeDelegate*,
                                     const ax::mojom::Event>>*
                   accessibility_events,
               const ui::AXPlatformNodeDelegate* delegate,
               const ax::mojom::Event event_type) {
              DCHECK(accessibility_events);
              accessibility_events->push_back({delegate, event_type});
            },
            &accessibility_events_);
    button_->GetViewAccessibility().set_accessibility_events_callback(
        std::move(accessibility_events_callback));
  }

  void TearDown() override {
    virtual_label_ = nullptr;
    button_ = nullptr;
    if (!widget_->IsClosed())
      widget_->Close();
    widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  ViewAXPlatformNodeDelegate* GetButtonAccessibility() const {
    return static_cast<ViewAXPlatformNodeDelegate*>(
        &button_->GetViewAccessibility());
  }

#if defined(USE_AURA)
  void SetCache(AXVirtualView& virtual_view, AXAuraObjCache& cache) const {
    virtual_view.set_cache(&cache);
  }
#endif  // defined(USE_AURA)

  void ExpectReceivedAccessibilityEvents(
      const std::vector<std::pair<const ui::AXPlatformNodeDelegate*,
                                  const ax::mojom::Event>>& expected_events) {
    EXPECT_THAT(accessibility_events_, testing::ContainerEq(expected_events));
    accessibility_events_.clear();
  }

  std::unique_ptr<Widget> widget_;
  raw_ptr<Button> button_ = nullptr;
  raw_ptr<AXVirtualView> virtual_label_ = nullptr;

 private:
  std::vector<
      std::pair<const ui::AXPlatformNodeDelegate*, const ax::mojom::Event>>
      accessibility_events_;
  ::ui::ScopedAXModeSetter ax_mode_setter_;
};

TEST_F(AXVirtualViewTest, AccessibilityRoleAndName) {
  EXPECT_EQ(ax::mojom::Role::kButton, GetButtonAccessibility()->GetRole());
  EXPECT_EQ(ax::mojom::Role::kStaticText, virtual_label_->GetRole());
  EXPECT_EQ("Label", virtual_label_->GetStringAttribute(
                         ax::mojom::StringAttribute::kName));
}

// The focusable state of a virtual view should not depend on the focusable
// state of the real view ancestor, however the enabled state should.
TEST_F(AXVirtualViewTest, FocusableAndEnabledState) {
  virtual_label_->GetCustomData().AddState(ax::mojom::State::kFocusable);
  EXPECT_TRUE(GetButtonAccessibility()->HasState(ax::mojom::State::kFocusable));
  EXPECT_TRUE(virtual_label_->HasState(ax::mojom::State::kFocusable));
  EXPECT_EQ(ax::mojom::Restriction::kNone,
            GetButtonAccessibility()->GetData().GetRestriction());
  EXPECT_EQ(ax::mojom::Restriction::kNone,
            virtual_label_->GetData().GetRestriction());

  button_->SetFocusBehavior(View::FocusBehavior::NEVER);
  EXPECT_FALSE(
      GetButtonAccessibility()->HasState(ax::mojom::State::kFocusable));
  EXPECT_TRUE(virtual_label_->HasState(ax::mojom::State::kFocusable));
  EXPECT_EQ(ax::mojom::Restriction::kNone,
            GetButtonAccessibility()->GetData().GetRestriction());
  EXPECT_EQ(ax::mojom::Restriction::kNone,
            virtual_label_->GetData().GetRestriction());

  button_->SetEnabled(false);
  EXPECT_FALSE(
      GetButtonAccessibility()->HasState(ax::mojom::State::kFocusable));
  EXPECT_TRUE(virtual_label_->HasState(ax::mojom::State::kFocusable));
  EXPECT_EQ(ax::mojom::Restriction::kDisabled,
            GetButtonAccessibility()->GetData().GetRestriction());
  EXPECT_EQ(ax::mojom::Restriction::kDisabled,
            virtual_label_->GetData().GetRestriction());

  button_->SetEnabled(true);
  button_->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  virtual_label_->GetCustomData().RemoveState(ax::mojom::State::kFocusable);
  EXPECT_TRUE(GetButtonAccessibility()->HasState(ax::mojom::State::kFocusable));
  EXPECT_FALSE(virtual_label_->HasState(ax::mojom::State::kFocusable));
  EXPECT_EQ(ax::mojom::Restriction::kNone,
            GetButtonAccessibility()->GetData().GetRestriction());
  EXPECT_EQ(ax::mojom::Restriction::kNone,
            virtual_label_->GetData().GetRestriction());
}

TEST_F(AXVirtualViewTest, VirtualLabelIsChildOfButton) {
  EXPECT_EQ(1u, GetButtonAccessibility()->GetChildCount());
  EXPECT_EQ(0u, virtual_label_->GetChildCount());
  ASSERT_NE(nullptr, virtual_label_->GetParent());
  EXPECT_EQ(button_->GetNativeViewAccessible(), virtual_label_->GetParent());
  ASSERT_NE(nullptr, GetButtonAccessibility()->ChildAtIndex(0));
  EXPECT_EQ(virtual_label_->GetNativeObject(),
            GetButtonAccessibility()->ChildAtIndex(0));
}

TEST_F(AXVirtualViewTest, VirtualViewsPruned) {
  auto v_label = std::make_unique<AXVirtualView>();
  virtual_label_->AddChildView(std::move(v_label));
  button_->GetViewAccessibility().SetIsLeaf(true);
  EXPECT_TRUE(virtual_label_->GetData().HasState(ax::mojom::State::kIgnored));
  EXPECT_TRUE(virtual_label_->children()[0].get()->GetData().HasState(
      ax::mojom::State::kIgnored));
}

TEST_F(AXVirtualViewTest, RemoveFromParentView) {
  ASSERT_EQ(1u, GetButtonAccessibility()->GetChildCount());
  std::unique_ptr<AXVirtualView> removed_label =
      std::exchange(virtual_label_, nullptr)->RemoveFromParentView();
  EXPECT_EQ(nullptr, removed_label->GetParent());
  EXPECT_TRUE(GetButtonAccessibility()->virtual_children().empty());

  AXVirtualView* virtual_child_1 = new AXVirtualView;
  removed_label->AddChildView(base::WrapUnique(virtual_child_1));
  ASSERT_EQ(1u, removed_label->GetChildCount());
  ASSERT_NE(nullptr, virtual_child_1->GetParent());
  std::unique_ptr<AXVirtualView> removed_child_1 =
      virtual_child_1->RemoveFromParentView();
  EXPECT_EQ(nullptr, removed_child_1->GetParent());
  EXPECT_EQ(0u, removed_label->GetChildCount());
}

#if defined(USE_AURA)
TEST_F(AXVirtualViewTest, MultipleCaches) {
  // This test ensures that AXVirtualView objects remove themselves from an
  // existing cache (if present) when |set_cache| is called.
  std::unique_ptr<AXAuraObjCache> cache = std::make_unique<AXAuraObjCache>();
  std::unique_ptr<AXAuraObjCache> second_cache =
      std::make_unique<AXAuraObjCache>();
  // Store |virtual_label_| in |cache|.
  SetCache(*virtual_label_, *cache);

  AXVirtualViewWrapper* wrapper =
      virtual_label_->GetOrCreateWrapper(cache.get());
  EXPECT_NE(wrapper, nullptr);
  EXPECT_NE(wrapper->GetUniqueId(), ui::kInvalidAXNodeID);
  EXPECT_NE(wrapper->GetParent(), nullptr);
  EXPECT_NE(cache->GetID(virtual_label_.get()), ui::kInvalidAXNodeID);

  // Store |virtual_label_| in |second_cache|.
  SetCache(*virtual_label_, *second_cache);
  AXVirtualViewWrapper* second_wrapper =
      virtual_label_->GetOrCreateWrapper(second_cache.get());
  EXPECT_NE(second_wrapper, nullptr);
  EXPECT_NE(second_wrapper->GetUniqueId(), ui::kInvalidAXNodeID);

  // |virtual_label_| should only exist in |second_cache|.
  EXPECT_NE(second_cache->GetID(virtual_label_.get()), ui::kInvalidAXNodeID);
  EXPECT_EQ(cache->GetID(virtual_label_.get()), ui::kInvalidAXNodeID);
}
#endif  // defined(USE_AURA)

TEST_F(AXVirtualViewTest, AddingAndRemovingVirtualChildren) {
  ASSERT_EQ(0u, virtual_label_->GetChildCount());
  ExpectReceivedAccessibilityEvents({});

  AXVirtualView* virtual_child_1 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_1));
  EXPECT_EQ(1u, virtual_label_->GetChildCount());
  ASSERT_NE(nullptr, virtual_child_1->GetParent());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_1->GetParent());
  ASSERT_NE(nullptr, virtual_label_->ChildAtIndex(0));
  EXPECT_EQ(virtual_child_1->GetNativeObject(),
            virtual_label_->ChildAtIndex(0));
  ExpectReceivedAccessibilityEvents({std::make_pair(
      GetButtonAccessibility(), ax::mojom::Event::kChildrenChanged)});

  AXVirtualView* virtual_child_2 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_2));
  EXPECT_EQ(2u, virtual_label_->GetChildCount());
  ASSERT_NE(nullptr, virtual_child_2->GetParent());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_2->GetParent());
  ASSERT_NE(nullptr, virtual_label_->ChildAtIndex(1));
  EXPECT_EQ(virtual_child_2->GetNativeObject(),
            virtual_label_->ChildAtIndex(1));
  ExpectReceivedAccessibilityEvents({std::make_pair(
      GetButtonAccessibility(), ax::mojom::Event::kChildrenChanged)});

  AXVirtualView* virtual_child_3 = new AXVirtualView;
  virtual_child_2->AddChildView(base::WrapUnique(virtual_child_3));
  EXPECT_EQ(2u, virtual_label_->GetChildCount());
  EXPECT_EQ(0u, virtual_child_1->GetChildCount());
  EXPECT_EQ(1u, virtual_child_2->GetChildCount());
  ASSERT_NE(nullptr, virtual_child_3->GetParent());
  EXPECT_EQ(virtual_child_2->GetNativeObject(), virtual_child_3->GetParent());
  ASSERT_NE(nullptr, virtual_child_2->ChildAtIndex(0));
  EXPECT_EQ(virtual_child_3->GetNativeObject(),
            virtual_child_2->ChildAtIndex(0));
  ExpectReceivedAccessibilityEvents({std::make_pair(
      GetButtonAccessibility(), ax::mojom::Event::kChildrenChanged)});

  virtual_child_2->RemoveChildView(virtual_child_3);
  EXPECT_EQ(0u, virtual_child_2->GetChildCount());
  EXPECT_EQ(2u, virtual_label_->GetChildCount());
  ExpectReceivedAccessibilityEvents({std::make_pair(
      GetButtonAccessibility(), ax::mojom::Event::kChildrenChanged)});

  virtual_label_->RemoveAllChildViews();
  EXPECT_EQ(0u, virtual_label_->GetChildCount());
  // There should be two "kChildrenChanged" events because Two virtual child
  // views are removed in total.
  ExpectReceivedAccessibilityEvents(
      {std::make_pair(GetButtonAccessibility(),
                      ax::mojom::Event::kChildrenChanged),
       std::make_pair(GetButtonAccessibility(),
                      ax::mojom::Event::kChildrenChanged)});
}

TEST_F(AXVirtualViewTest, ReorderingVirtualChildren) {
  ASSERT_EQ(0u, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_1 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_1));
  ASSERT_EQ(1u, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_2 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_2));
  ASSERT_EQ(2u, virtual_label_->GetChildCount());

  virtual_label_->ReorderChildView(virtual_child_1, 100);
  ASSERT_EQ(2u, virtual_label_->GetChildCount());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_2->GetParent());
  ASSERT_NE(nullptr, virtual_label_->ChildAtIndex(0));
  EXPECT_EQ(virtual_child_2->GetNativeObject(),
            virtual_label_->ChildAtIndex(0));
  ASSERT_NE(nullptr, virtual_label_->ChildAtIndex(1));
  EXPECT_EQ(virtual_child_1->GetNativeObject(),
            virtual_label_->ChildAtIndex(1));

  virtual_label_->ReorderChildView(virtual_child_1, 0);
  ASSERT_EQ(2u, virtual_label_->GetChildCount());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_1->GetParent());
  ASSERT_NE(nullptr, virtual_label_->ChildAtIndex(0));
  EXPECT_EQ(virtual_child_1->GetNativeObject(),
            virtual_label_->ChildAtIndex(0));
  ASSERT_NE(nullptr, virtual_label_->ChildAtIndex(1));
  EXPECT_EQ(virtual_child_2->GetNativeObject(),
            virtual_label_->ChildAtIndex(1));

  virtual_label_->RemoveAllChildViews();
  ASSERT_EQ(0u, virtual_label_->GetChildCount());
}

TEST_F(AXVirtualViewTest, ContainsVirtualChild) {
  ASSERT_EQ(0u, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_1 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_1));
  ASSERT_EQ(1u, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_2 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_2));
  ASSERT_EQ(2u, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_3 = new AXVirtualView;
  virtual_child_2->AddChildView(base::WrapUnique(virtual_child_3));
  ASSERT_EQ(1u, virtual_child_2->GetChildCount());

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
  ASSERT_EQ(0u, virtual_label_->GetChildCount());
}

TEST_F(AXVirtualViewTest, GetIndexOfVirtualChild) {
  ASSERT_EQ(0u, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_1 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_1));
  ASSERT_EQ(1u, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_2 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_2));
  ASSERT_EQ(2u, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_3 = new AXVirtualView;
  virtual_child_2->AddChildView(base::WrapUnique(virtual_child_3));
  ASSERT_EQ(1u, virtual_child_2->GetChildCount());

  EXPECT_FALSE(virtual_label_->GetIndexOf(virtual_label_).has_value());
  EXPECT_EQ(0u, virtual_label_->GetIndexOf(virtual_child_1).value());
  EXPECT_EQ(1u, virtual_label_->GetIndexOf(virtual_child_2).value());
  EXPECT_FALSE(virtual_label_->GetIndexOf(virtual_child_3).has_value());
  EXPECT_EQ(0u, virtual_child_2->GetIndexOf(virtual_child_3).value());

  virtual_label_->RemoveAllChildViews();
  ASSERT_EQ(0u, virtual_label_->GetChildCount());
}

// Verify that virtual views with invisible ancestors inherit the
// ax::mojom::State::kInvisible state.
TEST_F(AXVirtualViewTest, InvisibleVirtualViews) {
  EXPECT_TRUE(widget_->IsVisible());
  EXPECT_FALSE(
      GetButtonAccessibility()->HasState(ax::mojom::State::kInvisible));
  EXPECT_FALSE(virtual_label_->HasState(ax::mojom::State::kInvisible));

  button_->SetVisible(false);
  EXPECT_TRUE(GetButtonAccessibility()->HasState(ax::mojom::State::kInvisible));
  EXPECT_TRUE(virtual_label_->HasState(ax::mojom::State::kInvisible));
  button_->SetVisible(true);
}

TEST_F(AXVirtualViewTest, OverrideFocus) {
  ViewAccessibility& button_accessibility = button_->GetViewAccessibility();
  ASSERT_NE(nullptr, button_accessibility.GetNativeObject());
  ASSERT_NE(nullptr, virtual_label_->GetNativeObject());
  ExpectReceivedAccessibilityEvents({});

  button_->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  button_->RequestFocus();
  ExpectReceivedAccessibilityEvents(
      {std::make_pair(GetButtonAccessibility(), ax::mojom::Event::kFocus)});

  EXPECT_EQ(button_accessibility.GetNativeObject(),
            button_accessibility.GetFocusedDescendant());
  button_accessibility.OverrideFocus(virtual_label_);
  EXPECT_EQ(virtual_label_->GetNativeObject(),
            button_accessibility.GetFocusedDescendant());
  ExpectReceivedAccessibilityEvents(
      {std::make_pair(virtual_label_, ax::mojom::Event::kFocus)});

  button_accessibility.OverrideFocus(nullptr);
  EXPECT_EQ(button_accessibility.GetNativeObject(),
            button_accessibility.GetFocusedDescendant());
  ExpectReceivedAccessibilityEvents(
      {std::make_pair(GetButtonAccessibility(), ax::mojom::Event::kFocus)});

  ASSERT_EQ(0u, virtual_label_->GetChildCount());
  AXVirtualView* virtual_child_1 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_1));
  ASSERT_EQ(1u, virtual_label_->GetChildCount());
  ExpectReceivedAccessibilityEvents({std::make_pair(
      GetButtonAccessibility(), ax::mojom::Event::kChildrenChanged)});

  AXVirtualView* virtual_child_2 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_2));
  ASSERT_EQ(2u, virtual_label_->GetChildCount());
  ExpectReceivedAccessibilityEvents({std::make_pair(
      GetButtonAccessibility(), ax::mojom::Event::kChildrenChanged)});

  button_accessibility.OverrideFocus(virtual_child_1);
  EXPECT_EQ(virtual_child_1->GetNativeObject(),
            button_accessibility.GetFocusedDescendant());
  ExpectReceivedAccessibilityEvents(
      {std::make_pair(virtual_child_1, ax::mojom::Event::kFocus)});

  AXVirtualView* virtual_child_3 = new AXVirtualView;
  virtual_child_2->AddChildView(base::WrapUnique(virtual_child_3));
  ASSERT_EQ(1u, virtual_child_2->GetChildCount());
  ExpectReceivedAccessibilityEvents({std::make_pair(
      GetButtonAccessibility(), ax::mojom::Event::kChildrenChanged)});

  EXPECT_EQ(virtual_child_1->GetNativeObject(),
            button_accessibility.GetFocusedDescendant());
  button_accessibility.OverrideFocus(virtual_child_3);
  EXPECT_EQ(virtual_child_3->GetNativeObject(),
            button_accessibility.GetFocusedDescendant());
  ExpectReceivedAccessibilityEvents(
      {std::make_pair(virtual_child_3, ax::mojom::Event::kFocus)});

  // Test that calling GetFocus() while the owner view is not focused will
  // return nullptr.
  button_->SetFocusBehavior(View::FocusBehavior::NEVER);
  button_->RequestFocus();
  ExpectReceivedAccessibilityEvents({});
  EXPECT_EQ(nullptr, virtual_label_->GetFocus());
  EXPECT_EQ(nullptr, virtual_child_1->GetFocus());
  EXPECT_EQ(nullptr, virtual_child_2->GetFocus());
  EXPECT_EQ(nullptr, virtual_child_3->GetFocus());

  button_->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  button_->RequestFocus();
  ExpectReceivedAccessibilityEvents(
      {std::make_pair(virtual_child_3, ax::mojom::Event::kFocus)});

  // Test that calling GetFocus() from any object in the tree will return the
  // same result.
  EXPECT_EQ(virtual_child_3->GetNativeObject(), virtual_label_->GetFocus());
  EXPECT_EQ(virtual_child_3->GetNativeObject(), virtual_child_1->GetFocus());
  EXPECT_EQ(virtual_child_3->GetNativeObject(), virtual_child_2->GetFocus());
  EXPECT_EQ(virtual_child_3->GetNativeObject(), virtual_child_3->GetFocus());

  virtual_label_->RemoveChildView(virtual_child_2);
  ASSERT_EQ(1u, virtual_label_->GetChildCount());
  ExpectReceivedAccessibilityEvents(
      {std::make_pair(GetButtonAccessibility(), ax::mojom::Event::kFocus),
       std::make_pair(GetButtonAccessibility(),
                      ax::mojom::Event::kChildrenChanged)});

  EXPECT_EQ(button_accessibility.GetNativeObject(),
            button_accessibility.GetFocusedDescendant());
  EXPECT_EQ(button_accessibility.GetNativeObject(), virtual_label_->GetFocus());
  EXPECT_EQ(button_accessibility.GetNativeObject(),
            virtual_child_1->GetFocus());

  button_accessibility.OverrideFocus(virtual_child_1);
  EXPECT_EQ(virtual_child_1->GetNativeObject(),
            button_accessibility.GetFocusedDescendant());
  ExpectReceivedAccessibilityEvents(
      {std::make_pair(virtual_child_1, ax::mojom::Event::kFocus)});

  virtual_label_->RemoveAllChildViews();
  ASSERT_EQ(0u, virtual_label_->GetChildCount());
  EXPECT_EQ(button_accessibility.GetNativeObject(),
            button_accessibility.GetFocusedDescendant());
  ExpectReceivedAccessibilityEvents(
      {std::make_pair(GetButtonAccessibility(), ax::mojom::Event::kFocus),
       std::make_pair(GetButtonAccessibility(),
                      ax::mojom::Event::kChildrenChanged)});
}

TEST_F(AXVirtualViewTest, TreeNavigation) {
  ASSERT_EQ(0u, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_1 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_1));

  AXVirtualView* virtual_child_2 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_2));

  AXVirtualView* virtual_child_3 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_3));

  AXVirtualView* virtual_child_4 = new AXVirtualView;
  virtual_child_2->AddChildView(base::WrapUnique(virtual_child_4));

  EXPECT_EQ(button_->GetNativeViewAccessible(), virtual_label_->GetParent());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_1->GetParent());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_2->GetParent());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_3->GetParent());
  EXPECT_EQ(virtual_child_2->GetNativeObject(), virtual_child_4->GetParent());

  EXPECT_EQ(0u, virtual_label_->GetIndexInParent());
  EXPECT_EQ(0u, virtual_child_1->GetIndexInParent());
  EXPECT_EQ(1u, virtual_child_2->GetIndexInParent());
  EXPECT_EQ(2u, virtual_child_3->GetIndexInParent());
  EXPECT_EQ(0u, virtual_child_4->GetIndexInParent());

  EXPECT_EQ(3u, virtual_label_->GetChildCount());
  EXPECT_EQ(0u, virtual_child_1->GetChildCount());
  EXPECT_EQ(1u, virtual_child_2->GetChildCount());
  EXPECT_EQ(0u, virtual_child_3->GetChildCount());
  EXPECT_EQ(0u, virtual_child_4->GetChildCount());

  EXPECT_EQ(virtual_child_1->GetNativeObject(),
            virtual_label_->ChildAtIndex(0));
  EXPECT_EQ(virtual_child_2->GetNativeObject(),
            virtual_label_->ChildAtIndex(1));
  EXPECT_EQ(virtual_child_3->GetNativeObject(),
            virtual_label_->ChildAtIndex(2));
  EXPECT_EQ(virtual_child_4->GetNativeObject(),
            virtual_child_2->ChildAtIndex(0));

  EXPECT_EQ(virtual_child_1->GetNativeObject(),
            virtual_label_->GetFirstChild());
  EXPECT_EQ(virtual_child_3->GetNativeObject(), virtual_label_->GetLastChild());
  EXPECT_EQ(nullptr, virtual_child_1->GetFirstChild());
  EXPECT_EQ(nullptr, virtual_child_1->GetLastChild());
  EXPECT_EQ(virtual_child_4->GetNativeObject(),
            virtual_child_2->GetFirstChild());
  EXPECT_EQ(virtual_child_4->GetNativeObject(),
            virtual_child_2->GetLastChild());
  EXPECT_EQ(nullptr, virtual_child_4->GetFirstChild());
  EXPECT_EQ(nullptr, virtual_child_4->GetLastChild());

  EXPECT_EQ(nullptr, virtual_label_->GetNextSibling());
  EXPECT_EQ(nullptr, virtual_label_->GetPreviousSibling());

  EXPECT_EQ(virtual_child_2->GetNativeObject(),
            virtual_child_1->GetNextSibling());
  EXPECT_EQ(nullptr, virtual_child_1->GetPreviousSibling());

  EXPECT_EQ(virtual_child_3->GetNativeObject(),
            virtual_child_2->GetNextSibling());
  EXPECT_EQ(virtual_child_1->GetNativeObject(),
            virtual_child_2->GetPreviousSibling());

  EXPECT_EQ(nullptr, virtual_child_3->GetNextSibling());
  EXPECT_EQ(virtual_child_2->GetNativeObject(),
            virtual_child_3->GetPreviousSibling());

  EXPECT_EQ(nullptr, virtual_child_4->GetNextSibling());
  EXPECT_EQ(nullptr, virtual_child_4->GetPreviousSibling());
}

TEST_F(AXVirtualViewTest, TreeNavigationWithIgnoredVirtualViews) {
  ASSERT_EQ(0u, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_1 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_1));
  virtual_child_1->GetCustomData().AddState(ax::mojom::State::kIgnored);

  EXPECT_EQ(0u, virtual_label_->GetChildCount());
  EXPECT_EQ(0u, virtual_child_1->GetChildCount());

  AXVirtualView* virtual_child_2 = new AXVirtualView;
  virtual_child_1->AddChildView(base::WrapUnique(virtual_child_2));
  AXVirtualView* virtual_child_3 = new AXVirtualView;
  virtual_child_2->AddChildView(base::WrapUnique(virtual_child_3));
  AXVirtualView* virtual_child_4 = new AXVirtualView;
  virtual_child_2->AddChildView(base::WrapUnique(virtual_child_4));

  // While ignored nodes should not be accessible via any of the tree navigation
  // methods, their descendants should be.
  EXPECT_EQ(button_->GetNativeViewAccessible(), virtual_label_->GetParent());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_1->GetParent());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_2->GetParent());
  EXPECT_EQ(virtual_child_2->GetNativeObject(), virtual_child_3->GetParent());
  EXPECT_EQ(virtual_child_2->GetNativeObject(), virtual_child_4->GetParent());

  EXPECT_EQ(0u, virtual_label_->GetIndexInParent());
  EXPECT_FALSE(virtual_child_1->GetIndexInParent().has_value());
  EXPECT_EQ(0u, virtual_child_2->GetIndexInParent());
  EXPECT_EQ(0u, virtual_child_3->GetIndexInParent());
  EXPECT_EQ(1u, virtual_child_4->GetIndexInParent());

  EXPECT_EQ(1u, virtual_label_->GetChildCount());
  EXPECT_EQ(1u, virtual_child_1->GetChildCount());
  EXPECT_EQ(2u, virtual_child_2->GetChildCount());
  EXPECT_EQ(0u, virtual_child_3->GetChildCount());
  EXPECT_EQ(0u, virtual_child_4->GetChildCount());

  EXPECT_EQ(virtual_child_2->GetNativeObject(),
            virtual_label_->ChildAtIndex(0));
  EXPECT_EQ(virtual_child_2->GetNativeObject(),
            virtual_child_1->ChildAtIndex(0));
  EXPECT_EQ(virtual_child_3->GetNativeObject(),
            virtual_child_2->ChildAtIndex(0));
  EXPECT_EQ(virtual_child_4->GetNativeObject(),
            virtual_child_2->ChildAtIndex(1));

  // Try ignoring a node by changing its role, instead of its state.
  virtual_child_2->GetCustomData().role = ax::mojom::Role::kNone;

  EXPECT_EQ(button_->GetNativeViewAccessible(), virtual_label_->GetParent());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_1->GetParent());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_2->GetParent());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_3->GetParent());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_4->GetParent());

  EXPECT_EQ(2u, virtual_label_->GetChildCount());
  EXPECT_EQ(2u, virtual_child_1->GetChildCount());
  EXPECT_EQ(2u, virtual_child_2->GetChildCount());
  EXPECT_EQ(0u, virtual_child_3->GetChildCount());
  EXPECT_EQ(0u, virtual_child_4->GetChildCount());

  EXPECT_EQ(0u, virtual_label_->GetIndexInParent());
  EXPECT_FALSE(virtual_child_1->GetIndexInParent().has_value());
  EXPECT_FALSE(virtual_child_2->GetIndexInParent().has_value());
  EXPECT_EQ(0u, virtual_child_3->GetIndexInParent());
  EXPECT_EQ(1u, virtual_child_4->GetIndexInParent());

  EXPECT_EQ(virtual_child_3->GetNativeObject(),
            virtual_label_->ChildAtIndex(0));
  EXPECT_EQ(virtual_child_4->GetNativeObject(),
            virtual_label_->ChildAtIndex(1));
  EXPECT_EQ(virtual_child_3->GetNativeObject(),
            virtual_child_1->ChildAtIndex(0));
  EXPECT_EQ(virtual_child_4->GetNativeObject(),
            virtual_child_1->ChildAtIndex(1));
  EXPECT_EQ(virtual_child_3->GetNativeObject(),
            virtual_child_2->ChildAtIndex(0));
  EXPECT_EQ(virtual_child_4->GetNativeObject(),
            virtual_child_2->ChildAtIndex(1));

  // Test for mixed ignored and unignored virtual children.
  AXVirtualView* virtual_child_5 = new AXVirtualView;
  virtual_child_1->AddChildView(base::WrapUnique(virtual_child_5));

  EXPECT_EQ(button_->GetNativeViewAccessible(), virtual_label_->GetParent());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_1->GetParent());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_2->GetParent());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_3->GetParent());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_4->GetParent());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_5->GetParent());

  EXPECT_EQ(3u, virtual_label_->GetChildCount());
  EXPECT_EQ(3u, virtual_child_1->GetChildCount());
  EXPECT_EQ(2u, virtual_child_2->GetChildCount());
  EXPECT_EQ(0u, virtual_child_3->GetChildCount());
  EXPECT_EQ(0u, virtual_child_4->GetChildCount());
  EXPECT_EQ(0u, virtual_child_5->GetChildCount());

  EXPECT_EQ(0u, virtual_label_->GetIndexInParent());
  EXPECT_FALSE(virtual_child_1->GetIndexInParent().has_value());
  EXPECT_FALSE(virtual_child_2->GetIndexInParent().has_value());
  EXPECT_EQ(0u, virtual_child_3->GetIndexInParent());
  EXPECT_EQ(1u, virtual_child_4->GetIndexInParent());
  EXPECT_EQ(2u, virtual_child_5->GetIndexInParent());

  EXPECT_EQ(virtual_child_3->GetNativeObject(),
            virtual_label_->ChildAtIndex(0));
  EXPECT_EQ(virtual_child_4->GetNativeObject(),
            virtual_label_->ChildAtIndex(1));
  EXPECT_EQ(virtual_child_5->GetNativeObject(),
            virtual_label_->ChildAtIndex(2));

  // An ignored root node should not be exposed.
  virtual_label_->GetCustomData().AddState(ax::mojom::State::kIgnored);

  EXPECT_EQ(button_->GetNativeViewAccessible(), virtual_label_->GetParent());
  EXPECT_EQ(button_->GetNativeViewAccessible(), virtual_child_1->GetParent());
  EXPECT_EQ(button_->GetNativeViewAccessible(), virtual_child_2->GetParent());
  EXPECT_EQ(button_->GetNativeViewAccessible(), virtual_child_3->GetParent());
  EXPECT_EQ(button_->GetNativeViewAccessible(), virtual_child_4->GetParent());
  EXPECT_EQ(button_->GetNativeViewAccessible(), virtual_child_5->GetParent());

  EXPECT_EQ(3u, GetButtonAccessibility()->GetChildCount());

  EXPECT_EQ(0u, virtual_child_3->GetIndexInParent());
  EXPECT_EQ(1u, virtual_child_4->GetIndexInParent());
  EXPECT_EQ(2u, virtual_child_5->GetIndexInParent());

  EXPECT_EQ(virtual_child_3->GetNativeObject(),
            GetButtonAccessibility()->ChildAtIndex(0));
  EXPECT_EQ(virtual_child_4->GetNativeObject(),
            GetButtonAccessibility()->ChildAtIndex(1));
  EXPECT_EQ(virtual_child_5->GetNativeObject(),
            GetButtonAccessibility()->ChildAtIndex(2));

  // Test for mixed ignored and unignored root nodes.
  AXVirtualView* virtual_label_2 = new AXVirtualView;
  virtual_label_2->GetCustomData().role = ax::mojom::Role::kStaticText;
  virtual_label_2->GetCustomData().SetNameChecked("Label");
  button_->GetViewAccessibility().AddVirtualChildView(
      base::WrapUnique(virtual_label_2));

  EXPECT_EQ(button_->GetNativeViewAccessible(), virtual_label_2->GetParent());

  EXPECT_EQ(4u, GetButtonAccessibility()->GetChildCount());
  EXPECT_EQ(0u, virtual_label_2->GetChildCount());

  EXPECT_EQ(virtual_label_2->GetNativeObject(),
            GetButtonAccessibility()->ChildAtIndex(3));

  // A focusable node should not be ignored.
  virtual_child_1->GetCustomData().AddState(ax::mojom::State::kFocusable);

  EXPECT_EQ(2u, GetButtonAccessibility()->GetChildCount());
  EXPECT_EQ(1u, virtual_label_->GetChildCount());

  EXPECT_EQ(virtual_child_1->GetNativeObject(),
            GetButtonAccessibility()->ChildAtIndex(0));
  EXPECT_EQ(virtual_label_2->GetNativeObject(),
            GetButtonAccessibility()->ChildAtIndex(1));
}

TEST_F(AXVirtualViewTest, HitTesting) {
  ASSERT_EQ(0u, virtual_label_->GetChildCount());

  const gfx::Vector2d offset_from_origin =
      button_->GetBoundsInScreen().OffsetFromOrigin();

  // Test that hit testing is recursive.
  AXVirtualView* virtual_child_1 = new AXVirtualView;
  virtual_child_1->GetCustomData().relative_bounds.bounds =
      gfx::RectF(0, 0, 10, 10);
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_1));
  AXVirtualView* virtual_child_2 = new AXVirtualView;
  virtual_child_2->GetCustomData().relative_bounds.bounds =
      gfx::RectF(5, 5, 5, 5);
  virtual_child_1->AddChildView(base::WrapUnique(virtual_child_2));
  gfx::Point point_1 = gfx::Point(2, 2) + offset_from_origin;
  EXPECT_EQ(virtual_child_1->GetNativeObject(),
            virtual_child_1->HitTestSync(point_1.x(), point_1.y()));
  gfx::Point point_2 = gfx::Point(7, 7) + offset_from_origin;
  EXPECT_EQ(virtual_child_2->GetNativeObject(),
            virtual_label_->HitTestSync(point_2.x(), point_2.y()));

  // Test that hit testing follows the z-order.
  AXVirtualView* virtual_child_3 = new AXVirtualView;
  virtual_child_3->GetCustomData().relative_bounds.bounds =
      gfx::RectF(5, 5, 10, 10);
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_3));
  AXVirtualView* virtual_child_4 = new AXVirtualView;
  virtual_child_4->GetCustomData().relative_bounds.bounds =
      gfx::RectF(10, 10, 10, 10);
  virtual_child_3->AddChildView(base::WrapUnique(virtual_child_4));
  EXPECT_EQ(virtual_child_3->GetNativeObject(),
            virtual_label_->HitTestSync(point_2.x(), point_2.y()));
  gfx::Point point_3 = gfx::Point(12, 12) + offset_from_origin;
  EXPECT_EQ(virtual_child_4->GetNativeObject(),
            virtual_label_->HitTestSync(point_3.x(), point_3.y()));

  // Test that hit testing skips ignored nodes but not their descendants.
  virtual_child_3->GetCustomData().AddState(ax::mojom::State::kIgnored);
  EXPECT_EQ(virtual_child_2->GetNativeObject(),
            virtual_label_->HitTestSync(point_2.x(), point_2.y()));
  EXPECT_EQ(virtual_child_4->GetNativeObject(),
            virtual_label_->HitTestSync(point_3.x(), point_3.y()));
}

// Test for GetTargetForNativeAccessibilityEvent().
#if BUILDFLAG(IS_WIN)
TEST_F(AXVirtualViewTest, GetTargetForEvents) {
  EXPECT_EQ(button_, virtual_label_->GetOwnerView());
  EXPECT_NE(nullptr, HWNDForView(virtual_label_->GetOwnerView()));
  EXPECT_EQ(HWNDForView(button_),
            virtual_label_->GetTargetForNativeAccessibilityEvent());
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace views::test
