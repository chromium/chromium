// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_dialog_delegate_view.h"

#include <stddef.h>

#include "base/i18n/rtl.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/base/hit_test.h"
#include "ui/events/event_utils.h"
#include "ui/views/animation/test/ink_drop_host_view_test_api.h"
#include "ui/views/animation/test/test_ink_drop.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/test_widget_observer.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

#if defined(OS_WIN)
#include "ui/base/win/shell.h"
#endif

namespace views {

using test::TestInkDrop;

namespace {

constexpr gfx::Size kContentSize = gfx::Size(200, 200);

class TestBubbleDialogDelegateView : public BubbleDialogDelegateView {
 public:
  explicit TestBubbleDialogDelegateView(View* anchor_view)
      : BubbleDialogDelegateView(anchor_view, BubbleBorder::TOP_LEFT) {
    view_->SetFocusBehavior(FocusBehavior::ALWAYS);
    AddChildView(view_);
  }
  ~TestBubbleDialogDelegateView() override = default;

  using BubbleDialogDelegateView::SetAnchorView;

  // BubbleDialogDelegateView overrides:
  View* GetInitiallyFocusedView() override { return view_; }
  gfx::Size CalculatePreferredSize() const override { return kContentSize; }
  void AddedToWidget() override {
    if (title_view_)
      GetBubbleFrameView()->SetTitleView(std::move(title_view_));
  }

  base::string16 GetWindowTitle() const override {
    return base::ASCIIToUTF16("TITLE TITLE TITLE");
  }

  bool ShouldShowWindowTitle() const override {
    return should_show_window_title_;
  }

  bool ShouldShowCloseButton() const override {
    return should_show_close_button_;
  }

  int GetDialogButtons() const override { return buttons_; }

  void set_title_view(View* title_view) { title_view_.reset(title_view); }
  void show_close_button() { should_show_close_button_ = true; }
  void hide_buttons() {
    should_show_close_button_ = false;
    buttons_ = ui::DIALOG_BUTTON_NONE;
  }
  void set_should_show_window_title(bool should_show_window_title) {
    should_show_window_title_ = should_show_window_title;
  }

  using BubbleDialogDelegateView::SetAnchorRect;
  using BubbleDialogDelegateView::GetBubbleFrameView;
  using BubbleDialogDelegateView::SizeToContents;

 private:
  View* view_ = new View;
  std::unique_ptr<View> title_view_;
  int buttons_ = ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
  bool should_show_close_button_ = false;
  bool should_show_window_title_ = true;

  DISALLOW_COPY_AND_ASSIGN(TestBubbleDialogDelegateView);
};

class BubbleDialogDelegateViewTest : public ViewsTestBase {
 public:
  BubbleDialogDelegateViewTest() = default;
  ~BubbleDialogDelegateViewTest() override = default;

  // Creates and shows a test widget that owns its native widget.
  Widget* CreateTestWidget() {
    Widget* widget = new Widget();
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget->Init(std::move(params));
    widget->Show();
    return widget;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BubbleDialogDelegateViewTest);
};

}  // namespace

TEST_F(BubbleDialogDelegateViewTest, CreateDelegate) {
  std::unique_ptr<Widget> anchor_widget(CreateTestWidget());
  TestBubbleDialogDelegateView* bubble_delegate =
      new TestBubbleDialogDelegateView(anchor_widget->GetContentsView());
  bubble_delegate->set_color(SK_ColorGREEN);
  Widget* bubble_widget =
      BubbleDialogDelegateView::CreateBubble(bubble_delegate);
  EXPECT_EQ(bubble_delegate, bubble_widget->widget_delegate());
  EXPECT_EQ(bubble_widget, bubble_delegate->GetWidget());
  test::TestWidgetObserver bubble_observer(bubble_widget);
  bubble_widget->Show();

  BubbleBorder* border = bubble_delegate->GetBubbleFrameView()->bubble_border_;
  EXPECT_EQ(bubble_delegate->color(), border->background_color());

  EXPECT_FALSE(bubble_observer.widget_closed());
  bubble_widget->CloseNow();
  EXPECT_TRUE(bubble_observer.widget_closed());
}

TEST_F(BubbleDialogDelegateViewTest, CloseAnchorWidget) {
  std::unique_ptr<Widget> anchor_widget(CreateTestWidget());
  BubbleDialogDelegateView* bubble_delegate =
      new TestBubbleDialogDelegateView(anchor_widget->GetContentsView());
  // Preventing close on deactivate should not prevent closing with the anchor.
  bubble_delegate->set_close_on_deactivate(false);
  Widget* bubble_widget =
      BubbleDialogDelegateView::CreateBubble(bubble_delegate);
  EXPECT_EQ(bubble_delegate, bubble_widget->widget_delegate());
  EXPECT_EQ(bubble_widget, bubble_delegate->GetWidget());
  EXPECT_EQ(anchor_widget.get(), bubble_delegate->anchor_widget());
  test::TestWidgetObserver bubble_observer(bubble_widget);
  EXPECT_FALSE(bubble_observer.widget_closed());

  bubble_widget->Show();
  EXPECT_EQ(anchor_widget.get(), bubble_delegate->anchor_widget());
  EXPECT_FALSE(bubble_observer.widget_closed());

  // TODO(msw): Remove activation hack to prevent bookkeeping errors in:
  //            aura::test::TestActivationClient::OnWindowDestroyed().
  std::unique_ptr<Widget> smoke_and_mirrors_widget(CreateTestWidget());
  EXPECT_FALSE(bubble_observer.widget_closed());

  // Ensure that closing the anchor widget also closes the bubble itself.
  anchor_widget->CloseNow();
  EXPECT_TRUE(bubble_observer.widget_closed());
}

// This test checks that the bubble delegate is capable to handle an early
// destruction of the used anchor view. (Animations and delayed closure of the
// bubble will call upon the anchor view to get its location).
TEST_F(BubbleDialogDelegateViewTest, CloseAnchorViewTest) {
  // Create an anchor widget and add a view to be used as an anchor view.
  std::unique_ptr<Widget> anchor_widget(CreateTestWidget());
  std::unique_ptr<View> anchor_view(new View());
  anchor_widget->GetContentsView()->AddChildView(anchor_view.get());
  TestBubbleDialogDelegateView* bubble_delegate =
      new TestBubbleDialogDelegateView(anchor_view.get());
  // Prevent flakes by avoiding closing on activation changes.
  bubble_delegate->set_close_on_deactivate(false);
  Widget* bubble_widget =
      BubbleDialogDelegateView::CreateBubble(bubble_delegate);

  // Check that the anchor view is correct and set up an anchor view rect.
  // Make sure that this rect will get ignored (as long as the anchor view is
  // attached).
  EXPECT_EQ(anchor_view.get(), bubble_delegate->GetAnchorView());
  const gfx::Rect set_anchor_rect = gfx::Rect(10, 10, 100, 100);
  bubble_delegate->SetAnchorRect(set_anchor_rect);
  const gfx::Rect view_rect = bubble_delegate->GetAnchorRect();
  EXPECT_NE(view_rect.ToString(), set_anchor_rect.ToString());

  // Create the bubble.
  bubble_widget->Show();
  EXPECT_EQ(anchor_widget.get(), bubble_delegate->anchor_widget());

  // Remove now the anchor view and make sure that the original found rect
  // is still kept, so that the bubble does not jump when the view gets deleted.
  anchor_widget->GetContentsView()->RemoveChildView(anchor_view.get());
  anchor_view.reset();
  EXPECT_EQ(nullptr, bubble_delegate->GetAnchorView());
  EXPECT_EQ(view_rect.ToString(), bubble_delegate->GetAnchorRect().ToString());
}

// Testing that a move of the anchor view will lead to new bubble locations.
TEST_F(BubbleDialogDelegateViewTest, TestAnchorRectMovesWithViewTest) {
  // Create an anchor widget and add a view to be used as anchor view.
  std::unique_ptr<Widget> anchor_widget(CreateTestWidget());
  TestBubbleDialogDelegateView* bubble_delegate =
      new TestBubbleDialogDelegateView(anchor_widget->GetContentsView());
  BubbleDialogDelegateView::CreateBubble(bubble_delegate);

  anchor_widget->GetContentsView()->SetBounds(10, 10, 100, 100);
  const gfx::Rect view_rect = bubble_delegate->GetAnchorRect();

  anchor_widget->GetContentsView()->SetBounds(20, 10, 100, 100);
  const gfx::Rect view_rect_2 = bubble_delegate->GetAnchorRect();
  EXPECT_NE(view_rect.ToString(), view_rect_2.ToString());
}

TEST_F(BubbleDialogDelegateViewTest, ResetAnchorWidget) {
  std::unique_ptr<Widget> anchor_widget(CreateTestWidget());
  BubbleDialogDelegateView* bubble_delegate =
      new TestBubbleDialogDelegateView(anchor_widget->GetContentsView());

  // Make sure the bubble widget is parented to a widget other than the anchor
  // widget so that closing the anchor widget does not close the bubble widget.
  std::unique_ptr<Widget> parent_widget(CreateTestWidget());
  bubble_delegate->set_parent_window(parent_widget->GetNativeView());
  // Preventing close on deactivate should not prevent closing with the parent.
  bubble_delegate->set_close_on_deactivate(false);
  Widget* bubble_widget =
      BubbleDialogDelegateView::CreateBubble(bubble_delegate);
  EXPECT_EQ(bubble_delegate, bubble_widget->widget_delegate());
  EXPECT_EQ(bubble_widget, bubble_delegate->GetWidget());
  EXPECT_EQ(anchor_widget.get(), bubble_delegate->anchor_widget());
  test::TestWidgetObserver bubble_observer(bubble_widget);
  EXPECT_FALSE(bubble_observer.widget_closed());

  // Showing and hiding the bubble widget should have no effect on its anchor.
  bubble_widget->Show();
  EXPECT_EQ(anchor_widget.get(), bubble_delegate->anchor_widget());
  bubble_widget->Hide();
  EXPECT_EQ(anchor_widget.get(), bubble_delegate->anchor_widget());

  // Ensure that closing the anchor widget clears the bubble's reference to that
  // anchor widget, but the bubble itself does not close.
  anchor_widget->CloseNow();
  EXPECT_NE(anchor_widget.get(), bubble_delegate->anchor_widget());
  EXPECT_FALSE(bubble_observer.widget_closed());

  // TODO(msw): Remove activation hack to prevent bookkeeping errors in:
  //            aura::test::TestActivationClient::OnWindowDestroyed().
  std::unique_ptr<Widget> smoke_and_mirrors_widget(CreateTestWidget());
  EXPECT_FALSE(bubble_observer.widget_closed());

  // Ensure that closing the parent widget also closes the bubble itself.
  parent_widget->CloseNow();
  EXPECT_TRUE(bubble_observer.widget_closed());
}

TEST_F(BubbleDialogDelegateViewTest, InitiallyFocusedView) {
  std::unique_ptr<Widget> anchor_widget(CreateTestWidget());
  BubbleDialogDelegateView* bubble_delegate =
      new TestBubbleDialogDelegateView(anchor_widget->GetContentsView());
  Widget* bubble_widget =
      BubbleDialogDelegateView::CreateBubble(bubble_delegate);
  bubble_widget->Show();
  EXPECT_EQ(bubble_delegate->GetInitiallyFocusedView(),
            bubble_widget->GetFocusManager()->GetFocusedView());
  bubble_widget->CloseNow();
}

TEST_F(BubbleDialogDelegateViewTest, NonClientHitTest) {
  std::unique_ptr<Widget> anchor_widget(CreateTestWidget());
  TestBubbleDialogDelegateView* bubble_delegate =
      new TestBubbleDialogDelegateView(anchor_widget->GetContentsView());
  BubbleDialogDelegateView::CreateBubble(bubble_delegate);
  BubbleFrameView* frame = bubble_delegate->GetBubbleFrameView();

#if defined(OS_WIN)
  bool is_aero_glass_enabled = ui::win::IsAeroGlassEnabled();
#endif

  struct {
    const int point;
    const int hit;
  } kTestCases[] = {
#if defined(OS_WIN)
    {0, is_aero_glass_enabled ? HTTRANSPARENT : HTNOWHERE},
#else
    {0, HTTRANSPARENT},
#endif
    {60, HTCLIENT},
    {1000, HTNOWHERE},
  };

  for (const auto& test_case : kTestCases) {
    gfx::Point point(test_case.point, test_case.point);
    EXPECT_EQ(test_case.hit, frame->NonClientHitTest(point))
        << " at point " << test_case.point;
  }
}

TEST_F(BubbleDialogDelegateViewTest, VisibleWhenAnchorWidgetBoundsChanged) {
  std::unique_ptr<Widget> anchor_widget(CreateTestWidget());
  BubbleDialogDelegateView* bubble_delegate =
      new TestBubbleDialogDelegateView(anchor_widget->GetContentsView());
  Widget* bubble_widget =
      BubbleDialogDelegateView::CreateBubble(bubble_delegate);
  test::TestWidgetObserver bubble_observer(bubble_widget);
  EXPECT_FALSE(bubble_observer.widget_closed());

  bubble_widget->Show();
  EXPECT_TRUE(bubble_widget->IsVisible());
  anchor_widget->SetBounds(gfx::Rect(10, 10, 100, 100));
  EXPECT_TRUE(bubble_widget->IsVisible());
}

// Test that setting WidgetDelegate::SetCanActivate() to false makes the
// widget created via BubbleDialogDelegateView::CreateBubble() not activatable.
TEST_F(BubbleDialogDelegateViewTest, NotActivatable) {
  std::unique_ptr<Widget> anchor_widget(CreateTestWidget());
  BubbleDialogDelegateView* bubble_delegate =
      new TestBubbleDialogDelegateView(anchor_widget->GetContentsView());
  bubble_delegate->SetCanActivate(false);
  Widget* bubble_widget =
      BubbleDialogDelegateView::CreateBubble(bubble_delegate);
  bubble_widget->Show();
  EXPECT_FALSE(bubble_widget->CanActivate());
}

TEST_F(BubbleDialogDelegateViewTest, CloseMethods) {
  {
    std::unique_ptr<Widget> anchor_widget(CreateTestWidget());
    BubbleDialogDelegateView* bubble_delegate =
        new TestBubbleDialogDelegateView(anchor_widget->GetContentsView());
    bubble_delegate->set_close_on_deactivate(true);
    Widget* bubble_widget =
        BubbleDialogDelegateView::CreateBubble(bubble_delegate);
    bubble_widget->Show();
    anchor_widget->Activate();
    EXPECT_TRUE(bubble_widget->IsClosed());
  }

  {
    std::unique_ptr<Widget> anchor_widget(CreateTestWidget());
    BubbleDialogDelegateView* bubble_delegate =
        new TestBubbleDialogDelegateView(anchor_widget->GetContentsView());
    Widget* bubble_widget =
        BubbleDialogDelegateView::CreateBubble(bubble_delegate);
    bubble_widget->Show();

    ui::KeyEvent escape_event(ui::ET_KEY_PRESSED, ui::VKEY_ESCAPE, ui::EF_NONE);
    bubble_widget->OnKeyEvent(&escape_event);
    EXPECT_TRUE(bubble_widget->IsClosed());
  }

  {
    std::unique_ptr<Widget> anchor_widget(CreateTestWidget());
    TestBubbleDialogDelegateView* bubble_delegate =
        new TestBubbleDialogDelegateView(anchor_widget->GetContentsView());
    Widget* bubble_widget =
        BubbleDialogDelegateView::CreateBubble(bubble_delegate);
    bubble_widget->Show();
    BubbleFrameView* frame_view = bubble_delegate->GetBubbleFrameView();
    frame_view->ResetViewShownTimeStampForTesting();
    Button* close_button = frame_view->close_;
    ASSERT_TRUE(close_button);
    frame_view->ButtonPressed(
        close_button,
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE));
    EXPECT_TRUE(bubble_widget->IsClosed());
  }
}

TEST_F(BubbleDialogDelegateViewTest, CustomTitle) {
  std::unique_ptr<Widget> anchor_widget(CreateTestWidget());
  TestBubbleDialogDelegateView* bubble_delegate =
      new TestBubbleDialogDelegateView(anchor_widget->GetContentsView());
  constexpr int kTitleHeight = 20;
  View* title_view = new StaticSizedView(gfx::Size(10, kTitleHeight));
  bubble_delegate->set_title_view(title_view);
  Widget* bubble_widget =
      BubbleDialogDelegateView::CreateBubble(bubble_delegate);
  bubble_widget->Show();

  BubbleFrameView* bubble_frame = static_cast<BubbleFrameView*>(
      bubble_widget->non_client_view()->frame_view());
  EXPECT_EQ(title_view, bubble_frame->title());
  EXPECT_EQ(bubble_frame, title_view->parent());
  // Title takes up the whole bubble width when there's no icon or close button.
  EXPECT_EQ(bubble_delegate->width(), title_view->size().width());
  EXPECT_EQ(kTitleHeight, title_view->size().height());

  bubble_delegate->show_close_button();
  bubble_frame->ResetWindowControls();
  bubble_frame->Layout();

  Button* close_button = bubble_frame->GetCloseButtonForTesting();
  // Title moves over for the close button.
  EXPECT_EQ(close_button->x() - LayoutProvider::Get()->GetDistanceMetric(
                                    DISTANCE_CLOSE_BUTTON_MARGIN),
            title_view->bounds().right());

  LayoutProvider* provider = LayoutProvider::Get();
  const gfx::Insets content_margins =
      provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT);
  const gfx::Insets title_margins =
      provider->GetInsetsMetric(INSETS_DIALOG_TITLE);
  EXPECT_EQ(content_margins, bubble_delegate->margins());
  // Note there is no title_margins() accessor (it should not be customizable).

  // To perform checks on the precise size, first hide the dialog buttons so the
  // calculations are simpler (e.g. platform font discrepancies can be ignored).
  bubble_delegate->hide_buttons();
  bubble_frame->ResetWindowControls();
  bubble_delegate->DialogModelChanged();
  bubble_delegate->SizeToContents();

  // Use GetContentsBounds() to exclude the bubble border, which can change per
  // platform.
  gfx::Rect frame_size = bubble_frame->GetContentsBounds();
  EXPECT_EQ(content_margins.height() + kContentSize.height() +
                title_margins.height() + kTitleHeight,
            frame_size.height());
  EXPECT_EQ(content_margins.width() + kContentSize.width(), frame_size.width());

  // Set the title preferred size to 0. The bubble frame makes fewer assumptions
  // about custom title views, so there should still be margins for it while the
  // WidgetDelegate says it should be shown, even if its preferred size is zero.
  title_view->SetPreferredSize(gfx::Size());
  bubble_widget->UpdateWindowTitle();
  bubble_delegate->SizeToContents();
  frame_size = bubble_frame->GetContentsBounds();
  EXPECT_EQ(
      content_margins.height() + kContentSize.height() + title_margins.height(),
      frame_size.height());
  EXPECT_EQ(content_margins.width() + kContentSize.width(), frame_size.width());

  // Now hide the title properly. The margins should also disappear.
  bubble_delegate->set_should_show_window_title(false);
  bubble_widget->UpdateWindowTitle();
  bubble_delegate->SizeToContents();
  frame_size = bubble_frame->GetContentsBounds();
  EXPECT_EQ(content_margins.height() + kContentSize.height(),
            frame_size.height());
  EXPECT_EQ(content_margins.width() + kContentSize.width(), frame_size.width());
}

// Ensure the BubbleFrameView correctly resizes when the title is provided by a
// StyledLabel.
TEST_F(BubbleDialogDelegateViewTest, StyledLabelTitle) {
  std::unique_ptr<Widget> anchor_widget(CreateTestWidget());
  TestBubbleDialogDelegateView* bubble_delegate =
      new TestBubbleDialogDelegateView(anchor_widget->GetContentsView());
  StyledLabel* title_view = new StyledLabel(base::ASCIIToUTF16("123"), nullptr);
  bubble_delegate->set_title_view(title_view);

  Widget* bubble_widget =
      BubbleDialogDelegateView::CreateBubble(bubble_delegate);
  bubble_widget->Show();

  const gfx::Size size_before_new_title =
      bubble_widget->GetWindowBoundsInScreen().size();
  title_view->SetText(base::ASCIIToUTF16("12"));
  bubble_delegate->SizeToContents();

  // A shorter title should change nothing, since both will be within the
  // minimum dialog width.
  EXPECT_EQ(size_before_new_title,
            bubble_widget->GetWindowBoundsInScreen().size());

  title_view->SetText(base::UTF8ToUTF16(std::string(200, '0')));
  bubble_delegate->SizeToContents();

  // A (much) longer title should increase the height, but not the width.
  EXPECT_EQ(size_before_new_title.width(),
            bubble_widget->GetWindowBoundsInScreen().width());
  EXPECT_LT(size_before_new_title.height(),
            bubble_widget->GetWindowBoundsInScreen().height());
}

// Ensure associated buttons are highlighted or unhighlighted when the bubble
// widget is shown or hidden respectively.
TEST_F(BubbleDialogDelegateViewTest, AttachedWidgetShowsInkDropWhenVisible) {
  std::unique_ptr<Widget> anchor_widget(CreateTestWidget());
  LabelButton* button = new LabelButton(nullptr, base::string16());
  anchor_widget->GetContentsView()->AddChildView(button);
  TestInkDrop* ink_drop = new TestInkDrop();
  test::InkDropHostViewTestApi(button).SetInkDrop(base::WrapUnique(ink_drop));
  TestBubbleDialogDelegateView* bubble_delegate =
      new TestBubbleDialogDelegateView(nullptr);
  bubble_delegate->set_parent_window(anchor_widget->GetNativeView());

  Widget* bubble_widget =
      BubbleDialogDelegateView::CreateBubble(bubble_delegate);
  bubble_delegate->SetHighlightedButton(button);
  bubble_widget->Show();
  // Explicitly calling OnWidgetVisibilityChanging to test functionality for
  // OS_WIN. Outside of the test environment this happens automatically by way
  // of HWNDMessageHandler.
  bubble_delegate->OnWidgetVisibilityChanging(bubble_widget, true);
  EXPECT_EQ(InkDropState::ACTIVATED, ink_drop->GetTargetInkDropState());

  bubble_widget->Close();
  bubble_delegate->OnWidgetVisibilityChanging(bubble_widget, false);
  EXPECT_EQ(InkDropState::DEACTIVATED, ink_drop->GetTargetInkDropState());
}

// Ensure associated buttons are highlighted or unhighlighted when the bubble
// widget is shown or hidden respectively when highlighted button is set after
// widget is shown.
TEST_F(BubbleDialogDelegateViewTest, VisibleWidgetShowsInkDropOnAttaching) {
  std::unique_ptr<Widget> anchor_widget(CreateTestWidget());
  LabelButton* button = new LabelButton(nullptr, base::string16());
  anchor_widget->GetContentsView()->AddChildView(button);
  TestInkDrop* ink_drop = new TestInkDrop();
  test::InkDropHostViewTestApi(button).SetInkDrop(base::WrapUnique(ink_drop));
  TestBubbleDialogDelegateView* bubble_delegate =
      new TestBubbleDialogDelegateView(nullptr);
  bubble_delegate->set_parent_window(anchor_widget->GetNativeView());

  Widget* bubble_widget =
      BubbleDialogDelegateView::CreateBubble(bubble_delegate);
  bubble_widget->Show();
  // Explicitly calling OnWidgetVisibilityChanging to test functionality for
  // OS_WIN. Outside of the test environment this happens automatically by way
  // of HWNDMessageHandler.
  bubble_delegate->OnWidgetVisibilityChanging(bubble_widget, true);
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());
  bubble_delegate->SetHighlightedButton(button);
  EXPECT_EQ(InkDropState::ACTIVATED, ink_drop->GetTargetInkDropState());

  bubble_widget->Close();
  bubble_delegate->OnWidgetVisibilityChanging(bubble_widget, false);
  EXPECT_EQ(InkDropState::DEACTIVATED, ink_drop->GetTargetInkDropState());
}

TEST_F(BubbleDialogDelegateViewTest, VisibleAnchorChanges) {
  std::unique_ptr<Widget> anchor_widget(CreateTestWidget());
  TestBubbleDialogDelegateView* bubble_delegate =
      new TestBubbleDialogDelegateView(nullptr);
  bubble_delegate->set_parent_window(anchor_widget->GetNativeView());

  Widget* bubble_widget =
      BubbleDialogDelegateView::CreateBubble(bubble_delegate);
  bubble_widget->Show();
  EXPECT_FALSE(anchor_widget->ShouldPaintAsActive());
  bubble_delegate->SetAnchorView(anchor_widget->GetContentsView());
  EXPECT_TRUE(anchor_widget->ShouldPaintAsActive());

  bubble_widget->Hide();
}

TEST_F(BubbleDialogDelegateViewTest, GetThemeProvider_FromAnchorWidget) {
  std::unique_ptr<Widget> anchor_widget(CreateTestWidget());
  TestBubbleDialogDelegateView* bubble_delegate =
      new TestBubbleDialogDelegateView(nullptr);
  bubble_delegate->set_parent_window(anchor_widget->GetNativeView());

  Widget* bubble_widget =
      BubbleDialogDelegateView::CreateBubble(bubble_delegate);
  bubble_widget->Show();
  EXPECT_NE(bubble_widget->GetThemeProvider(),
            anchor_widget->GetThemeProvider());

  bubble_delegate->SetAnchorView(anchor_widget->GetRootView());
  EXPECT_EQ(bubble_widget->GetThemeProvider(),
            anchor_widget->GetThemeProvider());
}

// Anchoring Tests -------------------------------------------------------------

namespace {

class AnchorTestBubbleDialogDelegateView : public BubbleDialogDelegateView {
 public:
  explicit AnchorTestBubbleDialogDelegateView(View* anchor_view)
      : BubbleDialogDelegateView(anchor_view, BubbleBorder::TOP_LEFT) {}
  ~AnchorTestBubbleDialogDelegateView() override = default;

  // DialogDelegate:
  // Avoid engaging the AX focus system on bubble activation. The AX focus
  // system is not fully initialized for these tests and can cause crashes.
  View* GetInitiallyFocusedView() override { return nullptr; }

  using BubbleDialogDelegateView::SetAnchorView;

 private:
  DISALLOW_COPY_AND_ASSIGN(AnchorTestBubbleDialogDelegateView);
};

// Provides functionality for testing bubble anchoring logic.
// Deriving from widget test provides some nice out-of-the-box functionality for
// creating and managing widgets.
class BubbleDialogDelegateViewAnchorTest : public test::WidgetTest {
 public:
  BubbleDialogDelegateViewAnchorTest() = default;
  ~BubbleDialogDelegateViewAnchorTest() override = default;

  // Anchors a bubble widget to another widget.
  void Anchor(Widget* bubble_widget, Widget* anchor_to) {
    static_cast<AnchorTestBubbleDialogDelegateView*>(
        bubble_widget->widget_delegate())
        ->SetAnchorView(GetAnchorView(anchor_to));
  }

  // Creates a test bubble dialog widget. If |anchor_to| is not specified, uses
  // dummy_widget().
  Widget* CreateBubble(Widget* anchor_to = nullptr) {
    if (!anchor_to)
      anchor_to = dummy_widget();
    View* const anchor_view = anchor_to ? GetAnchorView(anchor_to) : nullptr;
    return BubbleDialogDelegateView::CreateBubble(
        new AnchorTestBubbleDialogDelegateView(anchor_view));
  }

  WidgetAutoclosePtr CreateTopLevelWidget() {
    return WidgetAutoclosePtr(CreateTopLevelPlatformWidget());
  }

  // test::WidgetTest:
  void SetUp() override {
    WidgetTest::SetUp();
    dummy_widget_.reset(CreateTopLevelPlatformWidget());
    dummy_widget_->Show();
  }

  void TearDown() override {
    dummy_widget_.reset();
    WidgetTest::TearDown();
  }

 protected:
  // Provides a widget that can be used to anchor bubbles or take focus away
  // from the widgets actually being used in each test.
  Widget* dummy_widget() const { return dummy_widget_.get(); }

 private:
  View* GetAnchorView(Widget* widget) {
    View* const contents_view = widget->GetContentsView();
    DCHECK(contents_view);
    return contents_view;
  }

  WidgetAutoclosePtr dummy_widget_;

  DISALLOW_COPY_AND_ASSIGN(BubbleDialogDelegateViewAnchorTest);
};

}  // namespace

TEST_F(BubbleDialogDelegateViewAnchorTest,
       AnchoredToWidgetShouldPaintAsActive) {
  auto widget = CreateTopLevelWidget();
  widget->ShowInactive();
  EXPECT_FALSE(widget->ShouldPaintAsActive());

  auto* bubble = CreateBubble();
  bubble->Show();
  EXPECT_FALSE(widget->ShouldPaintAsActive());
  EXPECT_TRUE(bubble->ShouldPaintAsActive());

  Anchor(bubble, widget.get());
  EXPECT_TRUE(widget->ShouldPaintAsActive());
  EXPECT_TRUE(bubble->ShouldPaintAsActive());
}

TEST_F(BubbleDialogDelegateViewAnchorTest,
       AnchoredToWidgetBecomesActiveWhenBubbleIsShown) {
  auto widget = CreateTopLevelWidget();
  widget->ShowInactive();
  EXPECT_FALSE(widget->ShouldPaintAsActive());

  auto* bubble = CreateBubble(widget.get());
  bubble->Show();
  EXPECT_TRUE(bubble->ShouldPaintAsActive());
  EXPECT_TRUE(widget->ShouldPaintAsActive());
}

TEST_F(BubbleDialogDelegateViewAnchorTest,
       ActiveStatePersistsAcrossAnchorWidgetAndBubbleActivation) {
  auto widget = CreateTopLevelWidget();
  widget->ShowInactive();
  auto* bubble = CreateBubble(widget.get());
  bubble->ShowInactive();
  EXPECT_FALSE(widget->ShouldPaintAsActive());
  EXPECT_FALSE(bubble->ShouldPaintAsActive());

  // Toggle activation back and forth between widgets.

  bubble->Activate();
  EXPECT_TRUE(widget->ShouldPaintAsActive());
  EXPECT_TRUE(bubble->ShouldPaintAsActive());

  widget->Activate();
  EXPECT_TRUE(widget->ShouldPaintAsActive());
  EXPECT_FALSE(bubble->ShouldPaintAsActive());

  bubble->Activate();
  EXPECT_TRUE(widget->ShouldPaintAsActive());
  EXPECT_TRUE(bubble->ShouldPaintAsActive());

  dummy_widget()->Show();
  EXPECT_FALSE(widget->ShouldPaintAsActive());
  EXPECT_FALSE(bubble->ShouldPaintAsActive());
}

TEST_F(BubbleDialogDelegateViewAnchorTest,
       AnchoringAlreadyActiveBubbleChangesAnchorWidgetState) {
  auto widget = CreateTopLevelWidget();
  auto* bubble = CreateBubble();
  widget->Show();
  bubble->ShowInactive();
  EXPECT_TRUE(widget->ShouldPaintAsActive());
  EXPECT_FALSE(bubble->ShouldPaintAsActive());

  Anchor(bubble, widget.get());
  EXPECT_TRUE(widget->ShouldPaintAsActive());
  EXPECT_FALSE(bubble->ShouldPaintAsActive());

  bubble->Close();
  EXPECT_TRUE(widget->ShouldPaintAsActive());
}

TEST_F(BubbleDialogDelegateViewAnchorTest,
       ActivationPassesToRemainingWidgetOnBubbleClose) {
  auto widget = CreateTopLevelWidget();
  auto* bubble = CreateBubble();
  widget->ShowInactive();
  bubble->Show();
  EXPECT_FALSE(widget->ShouldPaintAsActive());
  EXPECT_TRUE(bubble->ShouldPaintAsActive());

  Anchor(bubble, widget.get());
  EXPECT_TRUE(widget->ShouldPaintAsActive());
  EXPECT_TRUE(bubble->ShouldPaintAsActive());

  widget->Activate();
  EXPECT_TRUE(widget->ShouldPaintAsActive());
  EXPECT_FALSE(bubble->ShouldPaintAsActive());

  bubble->Close();
  EXPECT_TRUE(widget->ShouldPaintAsActive());
}

TEST_F(BubbleDialogDelegateViewAnchorTest,
       ActivationPassesToOtherWidgetOnReanchor) {
  auto widget = CreateTopLevelWidget();
  auto* bubble = CreateBubble();
  widget->Show();
  bubble->ShowInactive();
  EXPECT_TRUE(widget->ShouldPaintAsActive());
  EXPECT_FALSE(bubble->ShouldPaintAsActive());

  Anchor(bubble, widget.get());
  EXPECT_TRUE(widget->ShouldPaintAsActive());
  EXPECT_FALSE(bubble->ShouldPaintAsActive());

  bubble->Activate();
  EXPECT_TRUE(widget->ShouldPaintAsActive());
  EXPECT_TRUE(bubble->ShouldPaintAsActive());

  Anchor(bubble, dummy_widget());
  EXPECT_FALSE(widget->ShouldPaintAsActive());
}

TEST_F(BubbleDialogDelegateViewAnchorTest,
       ActivationPassesAcrossChainOfAnchoredBubbles) {
  auto widget = CreateTopLevelWidget();
  auto* bubble = CreateBubble();
  auto* bubble2 = CreateBubble();
  widget->ShowInactive();
  bubble->ShowInactive();
  bubble2->Show();
  EXPECT_FALSE(widget->ShouldPaintAsActive());
  EXPECT_FALSE(bubble->ShouldPaintAsActive());
  EXPECT_TRUE(bubble2->ShouldPaintAsActive());

  Anchor(bubble, widget.get());
  EXPECT_FALSE(widget->ShouldPaintAsActive());
  EXPECT_FALSE(bubble->ShouldPaintAsActive());
  EXPECT_TRUE(bubble2->ShouldPaintAsActive());

  Anchor(bubble2, bubble);
  EXPECT_TRUE(widget->ShouldPaintAsActive());
  EXPECT_TRUE(bubble->ShouldPaintAsActive());
  EXPECT_TRUE(bubble2->ShouldPaintAsActive());

  dummy_widget()->Activate();
  EXPECT_FALSE(widget->ShouldPaintAsActive());
  EXPECT_FALSE(bubble->ShouldPaintAsActive());
  EXPECT_FALSE(bubble2->ShouldPaintAsActive());
}

TEST_F(BubbleDialogDelegateViewAnchorTest,
       DestroyingAnchoredToWidgetDoesNotCrash) {
  auto widget = CreateTopLevelWidget();
  auto* bubble = CreateBubble(widget.get());
  widget->Show();
  bubble->Show();
  widget.reset();
}

TEST_F(BubbleDialogDelegateViewAnchorTest,
       DestroyingMiddleWidgetOfAnchorChainDoesNotCrash) {
  auto widget = CreateTopLevelWidget();
  auto* bubble = CreateBubble();
  auto* bubble2 = CreateBubble();
  widget->Show();
  bubble->Show();
  bubble2->Show();
  Anchor(bubble, widget.get());
  Anchor(bubble2, bubble);
  bubble->Close();
}

}  // namespace views
