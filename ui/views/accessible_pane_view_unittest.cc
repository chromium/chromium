// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessible_pane_view.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/icu_test_util.h"
#include "build/build_config.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace views {

// TODO(alicet): bring pane rotation into views and add tests.
//               See browser_view.cc for details.

using AccessiblePaneViewTest = ViewsTestBase;

class TestBarView : public AccessiblePaneView {
  METADATA_HEADER(TestBarView, AccessiblePaneView)

 public:
  TestBarView();

  TestBarView(const TestBarView&) = delete;
  TestBarView& operator=(const TestBarView&) = delete;

  ~TestBarView() override;

  LabelButton* child_button() const { return child_button_; }
  LabelButton* second_child_button() const { return second_child_button_; }
  LabelButton* third_child_button() const { return third_child_button_; }
  LabelButton* not_child_button() const { return not_child_button_.get(); }

  View* GetDefaultFocusableChild() override;

  // Removes `child_button` from the view and returns ownership of it.
  std::unique_ptr<LabelButton> RemoveChildButton();

 private:
  void Init();

  raw_ptr<LabelButton> child_button_;
  raw_ptr<LabelButton> second_child_button_;
  raw_ptr<LabelButton> third_child_button_;
  std::unique_ptr<LabelButton> not_child_button_;
  IgnoreMissingWidgetForTestingScopedSetter a11y_ignore_missing_widget_;
};

TestBarView::TestBarView()
    : a11y_ignore_missing_widget_(GetViewAccessibility()) {
  Init();
  set_allow_deactivate_on_esc(true);
}

TestBarView::~TestBarView() = default;

std::unique_ptr<LabelButton> TestBarView::RemoveChildButton() {
  return RemoveChildViewT<LabelButton>(child_button_.ExtractAsDangling());
}

void TestBarView::Init() {
  SetLayoutManager(std::make_unique<FillLayout>());
  std::u16string label;
  child_button_ = AddChildView(std::make_unique<LabelButton>());
  second_child_button_ = AddChildView(std::make_unique<LabelButton>());
  third_child_button_ = AddChildView(std::make_unique<LabelButton>());
  not_child_button_ = std::make_unique<LabelButton>();
}

View* TestBarView::GetDefaultFocusableChild() {
  return child_button_;
}

BEGIN_METADATA(TestBarView)
END_METADATA

TEST_F(AccessiblePaneViewTest, SimpleSetPaneFocus) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params =
      CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                   Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(std::move(params));
  View* root = widget->GetRootView();
  auto* test_view = root->AddChildView(std::make_unique<TestBarView>());
  widget->Show();
  widget->Activate();

  // Set pane focus succeeds, focus on child.
  EXPECT_TRUE(test_view->SetPaneFocusAndFocusDefault());
  EXPECT_EQ(test_view, test_view->GetPaneFocusTraversable());
  EXPECT_EQ(test_view->child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());

  // Set focus on non child view, focus failed, stays on pane.
  EXPECT_TRUE(test_view->SetPaneFocus(test_view->not_child_button()));
  EXPECT_FALSE(test_view->not_child_button() ==
               test_view->GetWidget()->GetFocusManager()->GetFocusedView());
  EXPECT_EQ(test_view->child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());
  widget->CloseNow();
  widget.reset();
}

TEST_F(AccessiblePaneViewTest, SetPaneFocusAndRestore) {
  auto widget_main = std::make_unique<Widget>();
  Widget::InitParams params_main =
      CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                   Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params_main.bounds = gfx::Rect(0, 0, 20, 20);
  widget_main->Init(std::move(params_main));
  View* root_main = widget_main->GetRootView();
  auto* test_view_main = root_main->AddChildView(std::make_unique<View>());
  widget_main->Show();
  widget_main->Activate();
  test_view_main->GetFocusManager()->SetFocusedView(test_view_main);
  EXPECT_TRUE(widget_main->IsActive());
  EXPECT_TRUE(test_view_main->HasFocus());

  auto widget_bar = std::make_unique<Widget>();
  Widget::InitParams params_bar =
      CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                   Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params_bar.bounds = gfx::Rect(50, 50, 650, 650);
  widget_bar->Init(std::move(params_bar));
  View* root_bar = widget_bar->GetRootView();
  auto* test_view_bar = root_bar->AddChildView(std::make_unique<TestBarView>());
  widget_bar->Show();
  widget_bar->Activate();

  // Set pane focus succeeds, focus on child.
  EXPECT_TRUE(test_view_bar->SetPaneFocusAndFocusDefault());
  EXPECT_FALSE(test_view_main->HasFocus());
  EXPECT_FALSE(widget_main->IsActive());
  EXPECT_EQ(test_view_bar, test_view_bar->GetPaneFocusTraversable());
  EXPECT_EQ(test_view_bar->child_button(),
            test_view_bar->GetWidget()->GetFocusManager()->GetFocusedView());

  // Deactivate() is only reliable on Ash. On Windows it uses
  // ::GetNextWindow() to simply activate another window, and which one is not
  // predictable. On Mac, Deactivate() is not implemented. Note that
  // TestBarView calls set_allow_deactivate_on_esc(true), which is only
  // otherwise used in Ash.
#if !BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
  // Esc should deactivate the widget.
  test_view_bar->AcceleratorPressed(test_view_bar->escape_key());
  EXPECT_TRUE(widget_main->IsActive());
  EXPECT_FALSE(widget_bar->IsActive());
#endif

  widget_bar->CloseNow();
  widget_bar.reset();

  widget_main->CloseNow();
  widget_main.reset();
}

TEST_F(AccessiblePaneViewTest, TwoSetPaneFocus) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params =
      CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                   Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(std::move(params));
  View* root = widget->GetRootView();
  auto* test_view = root->AddChildView(std::make_unique<TestBarView>());
  auto* test_view_2 = root->AddChildView(std::make_unique<TestBarView>());
  widget->Show();
  widget->Activate();

  // Set pane focus succeeds, focus on child.
  EXPECT_TRUE(test_view->SetPaneFocusAndFocusDefault());
  EXPECT_EQ(test_view, test_view->GetPaneFocusTraversable());
  EXPECT_EQ(test_view->child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());

  // Set focus on another test_view, focus move to that pane.
  EXPECT_TRUE(test_view_2->SetPaneFocus(test_view_2->second_child_button()));
  EXPECT_FALSE(test_view->child_button() ==
               test_view->GetWidget()->GetFocusManager()->GetFocusedView());
  EXPECT_EQ(test_view_2->second_child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());
  widget->CloseNow();
  widget.reset();
}

TEST_F(AccessiblePaneViewTest, PaneFocusTraversal) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params =
      CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                   Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(std::move(params));
  View* root = widget->GetRootView();
  auto* original_test_view =
      root->AddChildView(std::make_unique<TestBarView>());
  auto* test_view = root->AddChildView(std::make_unique<TestBarView>());
  widget->Show();
  widget->Activate();

  // Set pane focus on first view.
  EXPECT_TRUE(original_test_view->SetPaneFocus(
      original_test_view->third_child_button()));

  // Test traversal in second view.
  // Set pane focus on second child.
  EXPECT_TRUE(test_view->SetPaneFocus(test_view->second_child_button()));
  // home
  test_view->AcceleratorPressed(test_view->home_key());
  EXPECT_EQ(test_view->child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());
  // end
  test_view->AcceleratorPressed(test_view->end_key());
  EXPECT_EQ(test_view->third_child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());
  // left
  test_view->AcceleratorPressed(test_view->left_key());
  EXPECT_EQ(test_view->second_child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());
  // right, right
  test_view->AcceleratorPressed(test_view->right_key());
  test_view->AcceleratorPressed(test_view->right_key());
  EXPECT_EQ(test_view->child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());

  // ESC
  test_view->AcceleratorPressed(test_view->escape_key());
  EXPECT_EQ(original_test_view->third_child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());
  widget->CloseNow();
  widget.reset();
}

TEST_F(AccessiblePaneViewTest, PaneFocusTraversalRespectsRTL) {
  base::test::ScopedRestoreICUDefaultLocale scoped_locale("he");
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params =
      CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                   Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(std::move(params));
  View* root = widget->GetRootView();
  auto* test_view = root->AddChildView(std::make_unique<TestBarView>());
  widget->Show();
  widget->Activate();

  // Set pane focus on middle button of view.
  EXPECT_TRUE(test_view->SetPaneFocus(test_view->second_child_button()));

  // Home should go to the logical first item regardless of direction.
  test_view->AcceleratorPressed(test_view->home_key());
  EXPECT_EQ(test_view->child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());
  // End should go to the logical last item regardless of direction.
  test_view->AcceleratorPressed(test_view->end_key());
  EXPECT_EQ(test_view->third_child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());

  // Set pane focus back on middle button of view.
  EXPECT_TRUE(test_view->SetPaneFocus(test_view->second_child_button()));

  // Left should go to the logical next item in RTL locales.
  test_view->AcceleratorPressed(test_view->left_key());
  EXPECT_EQ(test_view->third_child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());
  // Right should go to the logical previous item in RTL locales.
  test_view->AcceleratorPressed(test_view->right_key());
  test_view->AcceleratorPressed(test_view->right_key());
  EXPECT_EQ(test_view->child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());
}

TEST_F(AccessiblePaneViewTest, DoesntCrashOnEscapeWithRemovedView) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params =
      CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                   Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(std::move(params));
  View* root = widget->GetRootView();
  auto* test_view1 = root->AddChildView(std::make_unique<TestBarView>());
  auto* test_view2 = root->AddChildView(std::make_unique<TestBarView>());
  widget->Show();
  widget->Activate();

  // Do the following:
  // 1. focus |child_button| in |test_view1|.
  // 2. focus |child_button| in |test_view2|. This makes |test_view2| remember
  //    |test_view1|'s button as having focus.
  // 3. Removes |child_button| from |test_view1|.
  // 4. Presses escape on |test_view2|. Escape attempts to revert focus to the
  //    button in |test_view1| (because of step 2). Because it is not in a
  //    widget this should not attempt to focus anything.
  EXPECT_TRUE(test_view1->SetPaneFocus(test_view1->child_button()));
  EXPECT_TRUE(test_view2->SetPaneFocus(test_view2->child_button()));
  auto orphan = test_view1->RemoveChildButton();
  // This shouldn't hit a CHECK in the FocusManager.
  EXPECT_TRUE(test_view2->AcceleratorPressed(test_view2->escape_key()));
}

TEST_F(AccessiblePaneViewTest, AccessibleProperties) {
  std::unique_ptr<TestBarView> test_view = std::make_unique<TestBarView>();
  test_view->GetViewAccessibility().SetName(u"Name");
  test_view->GetViewAccessibility().SetDescription(u"Description");
  EXPECT_EQ(test_view->GetViewAccessibility().GetCachedName(), u"Name");
  EXPECT_EQ(test_view->GetViewAccessibility().GetCachedDescription(),
            u"Description");
  EXPECT_EQ(test_view->GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kPane);

  ui::AXNodeData data;
  test_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"Name");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kDescription),
            u"Description");
  EXPECT_EQ(data.role, ax::mojom::Role::kPane);

  data = ui::AXNodeData();
  test_view->GetViewAccessibility().SetRole(ax::mojom::Role::kToolbar);
  test_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"Name");
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kDescription),
            u"Description");
  EXPECT_EQ(data.role, ax::mojom::Role::kToolbar);
}

}  // namespace views
