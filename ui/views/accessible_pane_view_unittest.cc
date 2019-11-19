// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessible_pane_view.h"

#include "base/macros.h"
#include "build/build_config.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace views {

// TODO(alicet): bring pane rotation into views and add tests.
//               See browser_view.cc for details.

using AccessiblePaneViewTest = ViewsTestBase;

class TestBarView : public AccessiblePaneView,
                    public ButtonListener {
 public:
  TestBarView();
  ~TestBarView() override;

  void ButtonPressed(Button* sender, const ui::Event& event) override;
  LabelButton* child_button() const { return child_button_.get(); }
  LabelButton* second_child_button() const {
    return second_child_button_.get();
  }
  LabelButton* third_child_button() const { return third_child_button_.get(); }
  LabelButton* not_child_button() const { return not_child_button_.get(); }

  View* GetDefaultFocusableChild() override;

 private:
  void Init();

  std::unique_ptr<LabelButton> child_button_;
  std::unique_ptr<LabelButton> second_child_button_;
  std::unique_ptr<LabelButton> third_child_button_;
  std::unique_ptr<LabelButton> not_child_button_;

  DISALLOW_COPY_AND_ASSIGN(TestBarView);
};

TestBarView::TestBarView() {
  Init();
  set_allow_deactivate_on_esc(true);
}

TestBarView::~TestBarView() = default;

void TestBarView::ButtonPressed(Button* sender, const ui::Event& event) {
}

void TestBarView::Init() {
  SetLayoutManager(std::make_unique<FillLayout>());
  base::string16 label;
  child_button_ = std::make_unique<LabelButton>(this, label);
  AddChildView(child_button_.get());
  second_child_button_ = std::make_unique<LabelButton>(this, label);
  AddChildView(second_child_button_.get());
  third_child_button_ = std::make_unique<LabelButton>(this, label);
  AddChildView(third_child_button_.get());
  not_child_button_ = std::make_unique<LabelButton>(this, label);
}

View* TestBarView::GetDefaultFocusableChild() {
  return child_button_.get();
}

TEST_F(AccessiblePaneViewTest, SimpleSetPaneFocus) {
  TestBarView* test_view = new TestBarView();
  std::unique_ptr<Widget> widget(new Widget());
  Widget::InitParams params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(std::move(params));
  View* root = widget->GetRootView();
  root->AddChildView(test_view);
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
  View* test_view_main = new View();
  std::unique_ptr<Widget> widget_main(new Widget());
  Widget::InitParams params_main =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params_main.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params_main.bounds = gfx::Rect(0, 0, 20, 20);
  widget_main->Init(std::move(params_main));
  View* root_main = widget_main->GetRootView();
  root_main->AddChildView(test_view_main);
  widget_main->Show();
  widget_main->Activate();
  test_view_main->GetFocusManager()->SetFocusedView(test_view_main);
  EXPECT_TRUE(widget_main->IsActive());
  EXPECT_TRUE(test_view_main->HasFocus());

  TestBarView* test_view_bar = new TestBarView();
  std::unique_ptr<Widget> widget_bar(new Widget());
  Widget::InitParams params_bar =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params_bar.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params_bar.bounds = gfx::Rect(50, 50, 650, 650);
  widget_bar->Init(std::move(params_bar));
  View* root_bar = widget_bar->GetRootView();
  root_bar->AddChildView(test_view_bar);
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
#if !defined(OS_MACOSX) || defined(OS_CHROMEOS)
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
  TestBarView* test_view = new TestBarView();
  TestBarView* test_view_2 = new TestBarView();
  std::unique_ptr<Widget> widget(new Widget());
  Widget::InitParams params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(std::move(params));
  View* root = widget->GetRootView();
  root->AddChildView(test_view);
  root->AddChildView(test_view_2);
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
  TestBarView* test_view = new TestBarView();
  TestBarView* original_test_view = new TestBarView();
  std::unique_ptr<Widget> widget(new Widget());
  Widget::InitParams params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(std::move(params));
  View* root = widget->GetRootView();
  root->AddChildView(original_test_view);
  root->AddChildView(test_view);
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

TEST_F(AccessiblePaneViewTest, DoesntCrashOnEscapeWithRemovedView) {
  TestBarView* test_view1 = new TestBarView();
  TestBarView* test_view2 = new TestBarView();
  Widget widget;
  Widget::InitParams params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget.Init(std::move(params));
  View* root = widget.GetRootView();
  root->AddChildView(test_view1);
  root->AddChildView(test_view2);
  widget.Show();
  widget.Activate();

  View* v1 = test_view1->child_button();
  View* v2 = test_view2->child_button();
  // Do the following:
  // 1. focus |v1|.
  // 2. focus |v2|. This makes |test_view2| remember |v1| as having focus.
  // 3. Removes |v1| from it's parent.
  // 4. Presses escape on |test_view2|. Escape attempts to revert focus to |v1|
  //    (because of step 2). Because |v1| is not in a widget this should not
  //    attempt to focus anything.
  EXPECT_TRUE(test_view1->SetPaneFocus(v1));
  EXPECT_TRUE(test_view2->SetPaneFocus(v2));
  v1->parent()->RemoveChildView(v1);
  // This shouldn't hit a CHECK in the FocusManager.
  EXPECT_TRUE(test_view2->AcceleratorPressed(test_view2->escape_key()));
}

}  // namespace views
