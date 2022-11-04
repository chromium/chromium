// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/views_ax_tree_manager.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace views::test {

namespace {

class TestButton : public Button {
 public:
  TestButton() : Button(Button::PressedCallback()) {}
  TestButton(const TestButton&) = delete;
  TestButton& operator=(const TestButton&) = delete;
  ~TestButton() override = default;
};

class ViewsAXTreeManagerTest : public ViewsTestBase,
                               public ::testing::WithParamInterface<bool> {
 public:
  ViewsAXTreeManagerTest() = default;
  ViewsAXTreeManagerTest(const ViewsAXTreeManagerTest&) = delete;
  ViewsAXTreeManagerTest& operator=(const ViewsAXTreeManagerTest&) = delete;

 protected:
  void SetUp() override;
  void TearDown() override;
  void CloseWidget();
  ui::AXNode* FindNode(const ax::mojom::Role role,
                       const std::string& name_or_value) const;
  void WaitFor(const ui::AXEventGenerator::Event event);

  Widget* widget() const { return widget_.get(); }
  Button* button() const { return button_; }
  Label* label() const { return label_; }
  ViewsAXTreeManager* manager() const {
    return features::IsAccessibilityTreeForViewsEnabled()
               ? absl::get<raw_ptr<ViewsAXTreeManager>>(manager_).get()
               : absl::get<std::unique_ptr<ViewsAXTreeManager>>(manager_).get();
  }

 private:
  using TestOwnedManager = std::unique_ptr<ViewsAXTreeManager>;
  using WidgetOwnedManager = raw_ptr<ViewsAXTreeManager>;

  ui::AXNode* FindNodeInSubtree(ui::AXNode* root,
                                const ax::mojom::Role role,
                                const std::string& name_or_value) const;
  void OnGeneratedEvent(Widget* widget,
                        ui::AXEventGenerator::Event event,
                        ui::AXNodeID node_id);

  UniqueWidgetPtr widget_;
  raw_ptr<Button> button_ = nullptr;
  raw_ptr<Label> label_ = nullptr;
  absl::variant<TestOwnedManager, WidgetOwnedManager> manager_;
  ui::AXEventGenerator::Event event_to_wait_for_;
  std::unique_ptr<base::RunLoop> loop_runner_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

void ViewsAXTreeManagerTest::SetUp() {
  ViewsTestBase::SetUp();

  if (GetParam()) {
    scoped_feature_list_.InitWithFeatures(
        {features::kEnableAccessibilityTreeForViews}, {});
  }

  widget_ = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  widget_->Init(std::move(params));

  button_ = new TestButton();
  button_->SetSize(gfx::Size(20, 20));

  label_ = new Label();
  button_->AddChildView(label_.get());

  widget_->GetContentsView()->AddChildView(button_.get());
  widget_->Show();

  // AccessibilityTreeForViewsEnabled will create and manage its own
  // ViewsAXTreeManager, so we don't need to create one for testing.
  if (features::IsAccessibilityTreeForViewsEnabled()) {
    manager_ = widget_->GetRootView()->GetViewAccessibility().AXTreeManager();
  } else {
    manager_ = std::make_unique<ViewsAXTreeManager>(widget_.get());
  }

  ASSERT_NE(nullptr, manager());
  manager()->SetGeneratedEventCallbackForTesting(base::BindRepeating(
      &ViewsAXTreeManagerTest::OnGeneratedEvent, base::Unretained(this)));
  WaitFor(ui::AXEventGenerator::Event::SUBTREE_CREATED);
}

void ViewsAXTreeManagerTest::TearDown() {
  if (manager())
    manager()->UnsetGeneratedEventCallbackForTesting();
  if (features::IsAccessibilityTreeForViewsEnabled())
    manager_.emplace<WidgetOwnedManager>(nullptr);
  else
    manager_.emplace<TestOwnedManager>(nullptr);

  CloseWidget();
  ViewsTestBase::TearDown();
}

void ViewsAXTreeManagerTest::CloseWidget() {
  if (!widget_->IsClosed())
    widget_->CloseNow();

  RunPendingMessages();
}

ui::AXNode* ViewsAXTreeManagerTest::FindNode(
    const ax::mojom::Role role,
    const std::string& name_or_value) const {
  ui::AXNode* root = manager()->GetRoot();

  // If the manager has been closed, it will return nullptr as root.
  if (!root)
    return nullptr;

  return FindNodeInSubtree(root, role, name_or_value);
}

void ViewsAXTreeManagerTest::WaitFor(const ui::AXEventGenerator::Event event) {
  ASSERT_FALSE(loop_runner_ && loop_runner_->IsRunningOnCurrentThread())
      << "Waiting for multiple events is currently not supported.";
  loop_runner_ = std::make_unique<base::RunLoop>();
  event_to_wait_for_ = event;
  loop_runner_->Run();
}

ui::AXNode* ViewsAXTreeManagerTest::FindNodeInSubtree(
    ui::AXNode* root,
    const ax::mojom::Role role,
    const std::string& name_or_value) const {
  EXPECT_NE(nullptr, root);
  const std::string& name =
      root->GetStringAttribute(ax::mojom::StringAttribute::kName);
  const std::string& value =
      root->GetStringAttribute(ax::mojom::StringAttribute::kValue);
  if (root->GetRole() == role &&
      (name == name_or_value || value == name_or_value)) {
    return root;
  }

  for (ui::AXNode* child : root->children()) {
    ui::AXNode* result = FindNodeInSubtree(child, role, name_or_value);
    if (result)
      return result;
  }
  return nullptr;
}

void ViewsAXTreeManagerTest::OnGeneratedEvent(Widget* widget,
                                              ui::AXEventGenerator::Event event,
                                              ui::AXNodeID node_id) {
  ASSERT_NE(nullptr, manager()) << "Should not be called after TearDown().";
  if (loop_runner_ && event == event_to_wait_for_)
    loop_runner_->Quit();
}

}  // namespace

INSTANTIATE_TEST_SUITE_P(All, ViewsAXTreeManagerTest, testing::Bool());

TEST_P(ViewsAXTreeManagerTest, MirrorInitialTree) {
  ui::AXNodeData button_data;
  button()->GetViewAccessibility().GetAccessibleNodeData(&button_data);
  ui::AXNode* ax_button = FindNode(ax::mojom::Role::kButton, "");
  ASSERT_NE(nullptr, ax_button);
  EXPECT_EQ(button_data.role, ax_button->GetRole());
  EXPECT_EQ(
      button_data.GetStringAttribute(ax::mojom::StringAttribute::kDescription),
      ax_button->GetStringAttribute(ax::mojom::StringAttribute::kDescription));
  EXPECT_EQ(
      button_data.GetIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb),
      ax_button->GetIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb));
  EXPECT_TRUE(ax_button->HasState(ax::mojom::State::kFocusable));
}

TEST_P(ViewsAXTreeManagerTest, PerformAction) {
  ui::AXNode* ax_button = FindNode(ax::mojom::Role::kButton, "");
  ASSERT_NE(nullptr, ax_button);
  ASSERT_FALSE(
      ax_button->HasIntAttribute(ax::mojom::IntAttribute::kCheckedState));
  button()->SetState(TestButton::STATE_PRESSED);
  button()->NotifyAccessibilityEvent(ax::mojom::Event::kCheckedStateChanged,
                                     true);
  WaitFor(ui::AXEventGenerator::Event::CHECKED_STATE_CHANGED);
}

TEST_P(ViewsAXTreeManagerTest, MultipleTopLevelWidgets) {
  // This test is only relevant when IsAccessibilityTreeForViewsEnabled is set,
  // as it tests the lifetime management of ViewsAXTreeManager when a Widget is
  // closed.
  if (!features::IsAccessibilityTreeForViewsEnabled())
    return;

  UniqueWidgetPtr second_widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  second_widget->Init(std::move(params));

  std::unique_ptr<TestButton> second_button = std::make_unique<TestButton>();
  second_button->SetSize(gfx::Size(20, 20));

  std::unique_ptr<Label> second_label = std::make_unique<Label>();
  second_button->AddChildView(second_label.get());

  // If the load complete event is fired synchronously from
  // |ViewsAXtreeManager|, creating a second widget will inadvertently create
  // another ViewsAXTreeManager and hit DCHECK's due to the cache not being
  // up to date.
  second_widget->GetContentsView()->AddChildView(second_button.get());
  second_widget->Show();
}

}  // namespace views::test
