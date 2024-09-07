// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_dialog_delegate_view.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/i18n/rtl.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "ui/base/hit_test.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/compositor.h"
#include "ui/display/test/test_screen.h"
#include "ui/events/event_utils.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/test/ink_drop_host_test_api.h"
#include "ui/views/animation/test/test_ink_drop.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/test_widget_observer.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace views {

using test::TestInkDrop;

namespace {

constexpr gfx::Size kContentSize = gfx::Size(200, 200);

class TestBubbleDialogDelegateView : public BubbleDialogDelegateView {
  METADATA_HEADER(TestBubbleDialogDelegateView, BubbleDialogDelegateView)

 public:
  explicit TestBubbleDialogDelegateView(View* anchor_view)
      : BubbleDialogDelegateView(anchor_view,
                                 BubbleBorder::TOP_LEFT,
                                 BubbleBorder::NO_SHADOW,
                                 true) {
    view_->SetFocusBehavior(FocusBehavior::ALWAYS);
    AddChildView(view_.get());
  }
  ~TestBubbleDialogDelegateView() override = default;
  TestBubbleDialogDelegateView(const TestBubbleDialogDelegateView&) = delete;
  TestBubbleDialogDelegateView& operator=(const TestBubbleDialogDelegateView&) =
      delete;

  using BubbleDialogDelegateView::SetAnchorView;

  // BubbleDialogDelegateView overrides:
  View* GetInitiallyFocusedView() override { return view_; }
  gfx::Size CalculatePreferredSize(
      const SizeBounds& /*available_size*/) const override {
    return kContentSize;
  }
  void AddedToWidget() override {
    if (title_view_) {
      GetBubbleFrameView()->SetTitleView(std::move(title_view_));
    }
  }

  std::u16string GetWindowTitle() const override {
    return u"TITLE TITLE TITLE";
  }

  bool ShouldShowWindowTitle() const override {
    return should_show_window_title_;
  }

  bool ShouldShowCloseButton() const override {
    return should_show_close_button_;
  }

  template <typename T>
  T* set_title_view(std::unique_ptr<T> title_view) {
    T* const ret = title_view.get();
    title_view_ = std::move(title_view);
    return ret;
  }
  void show_close_button() {
    should_show_close_button_ = true;
    // It is necessary to keep visibility state synced.
    GetBubbleFrameView()->ResetWindowControls();
  }
  void hide_buttons() {
    should_show_close_button_ = false;
    // Autosize mode will layout() synchronously when SetButtons().
    // The synchronized Layout() requires ResetWindowControls() use
    // SetVisible(ShouldShowCloseButton()) to synchronize the visible state.
    // Otherwise, `DCHECK_EQ(button_area_rect.size(),GetButtonAreaSize());` in
    // BubbleFrameView::Layout() will fail. These two values use GetVisible()
    // and ShouldShowCloseButton() respectively to determine the button area.
    GetBubbleFrameView()->ResetWindowControls();

    DialogDelegate::SetButtons(
        static_cast<int>(ui::mojom::DialogButton::kNone));
  }
  void set_should_show_window_title(bool should_show_window_title) {
    should_show_window_title_ = should_show_window_title;
  }

  using BubbleDialogDelegateView::GetBubbleFrameView;
  using BubbleDialogDelegateView::SetAnchorRect;

 private:
  raw_ptr<View> view_ = new View;
  std::unique_ptr<View> title_view_;
  bool should_show_close_button_ = false;
  bool should_show_window_title_ = true;
};

BEGIN_METADATA(TestBubbleDialogDelegateView)
END_METADATA

class TestAlertBubbleDialogDelegateView : public TestBubbleDialogDelegateView {
  METADATA_HEADER(TestAlertBubbleDialogDelegateView,
                  TestBubbleDialogDelegateView)

 public:
  explicit TestAlertBubbleDialogDelegateView(View* anchor_view)
      : TestBubbleDialogDelegateView(anchor_view) {
    SetAccessibleWindowRole(ax::mojom::Role::kAlertDialog);
  }
  ~TestAlertBubbleDialogDelegateView() override = default;
};

BEGIN_METADATA(TestAlertBubbleDialogDelegateView)
END_METADATA

// A Widget that returns something other than null as its ThemeProvider.  This
// allows us to see whether the theme provider returned by some object came from
// this widget.
class WidgetWithNonNullThemeProvider : public Widget {
 public:
  WidgetWithNonNullThemeProvider() = default;

  // Widget:
  const ui::ThemeProvider* GetThemeProvider() const override {
    return reinterpret_cast<ui::ThemeProvider*>(1);
  }
};

class BubbleDialogDelegateViewTest : public ViewsTestBase {
 public:
  BubbleDialogDelegateViewTest() {
    feature_list_.InitAndEnableFeature(features::kBubbleMetricsApi);
  }

  BubbleDialogDelegateViewTest(const BubbleDialogDelegateViewTest&) = delete;
  BubbleDialogDelegateViewTest& operator=(const BubbleDialogDelegateViewTest&) =
      delete;

  ~BubbleDialogDelegateViewTest() override = default;

  std::unique_ptr<views::Widget> CreateTestWidget(
      views::Widget::InitParams::Ownership ownership,
      views::Widget::InitParams::Type type) override {
    Widget::InitParams params = CreateParamsForTestWidget(ownership, type);
    auto widget = std::make_unique<WidgetWithNonNullThemeProvider>();
    widget->Init(std::move(params));
    widget->Show();
    return widget;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class BubbleUmaLoggerTest : public ViewsTestBase {
 public:
  BubbleUmaLoggerTest() {
    feature_list_.InitAndEnableFeature(features::kBubbleMetricsApi);
  }

  BubbleUmaLoggerTest(const BubbleUmaLoggerTest&) = delete;
  BubbleUmaLoggerTest& operator=(const BubbleUmaLoggerTest&) = delete;

  ~BubbleUmaLoggerTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

TEST_F(BubbleDialogDelegateViewTest, CreateDelegate) {
  std::unique_ptr<Widget> anchor_widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
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
  EXPECT_EQ(bubble_delegate->color(), border->color());
  EXPECT_EQ(anchor_widget.get(), bubble_widget->parent());

  EXPECT_FALSE(bubble_observer.widget_closed());
  bubble_widget->CloseNow();
  EXPECT_TRUE(bubble_observer.widget_closed());
}

TEST_F(BubbleDialogDelegateViewTest, CloseAnchorWidget) {
  std::unique_ptr<Widget> anchor_widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
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
  std::unique_ptr<Widget> smoke_and_mirrors_widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                       Widget::InitParams::TYPE_WINDOW_FRAMELESS);
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
  std::unique_ptr<Widget> anchor_widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                       Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  View* anchor_view = anchor_widget->SetContentsView(std::make_unique<View>());
  TestBubbleDialogDelegateView* bubble_delegate =
      new TestBubbleDialogDelegateView(anchor_view);
  // Prevent flakes by avoiding closing on activation changes.
  bubble_delegate->set_close_on_deactivate(false);
  Widget* bubble_widget =
      BubbleDialogDelegateView::CreateBubble(bubble_delegate);

  // Check that the anchor view is correct and set up an anchor view rect.
  // Make sure that this rect will get ignored (as long as the anchor view is
  // attached).
  EXPECT_EQ(anchor_view, bubble_delegate->GetAnchorView());
  const gfx::Rect set_anchor_rect = gfx::Rect(10, 10, 100, 100);
  bubble_delegate->SetAnchorRect(set_anchor_rect);
  const gfx::Rect view_rect = bubble_delegate->GetAnchorRect();
  EXPECT_NE(view_rect.ToString(), set_anchor_rect.ToString());

  // Create the bubble.
  bubble_widget->Show();
  EXPECT_EQ(anchor_widget.get(), bubble_delegate->anchor_widget());

  // Remove now the anchor view and make sure that the original found rect
  // is still kept, so that the bubble does not jump when the view gets deleted.
  anchor_view->parent()->RemoveChildViewT(anchor_view);
  EXPECT_EQ(nullptr, bubble_delegate->GetAnchorView());
  EXPECT_EQ(view_rect.ToString(), bubble_delegate->GetAnchorRect().ToString());
}

// Testing that a move of the anchor view will lead to new bubble locations.
TEST_F(BubbleDialogDelegateViewTest, TestAnchorRectMovesWithViewTest) {
  // Create an anchor widget and add a view to be used as anchor view.
  std::unique_ptr<Widget> anchor_widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
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
  std::unique_ptr<Widget> anchor_widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  BubbleDialogDelegateView* bubble_delegate =
      new TestBubbleDialogDelegateView(anchor_widget->GetContentsView());

  // Make sure the bubble widget is parented to a widget other than the anchor
  // widget so that closing the anchor widget does not close the bubble widget.
  std::unique_ptr<Widget> parent_widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                       Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  bubble_delegate->set_parent_window(parent_widget->GetNativeView());
  // Preventing close on deactivate should not prevent closing with the parent.
  bubble_delegate->set_close_on_deactivate(false);
  Widget* bubble_widget =
      BubbleDialogDelegateView::CreateBubble(bubble_delegate);
  EXPECT_EQ(bubble_delegate, bubble_widget->widget_delegate());
  EXPECT_EQ(bubble_widget, bubble_delegate->GetWidget());
  EXPECT_EQ(anchor_widget.get(), bubble_delegate->anchor_widget());
  EXPECT_EQ(parent_widget.get(), bubble_widget->parent());
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
  std::unique_ptr<Widget> smoke_and_mirrors_widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                       Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  EXPECT_FALSE(bubble_observer.widget_closed());

  // Ensure that closing the parent widget also closes the bubble itself.
  parent_widget->CloseNow();
  EXPECT_TRUE(bubble_observer.widget_closed());
}

TEST_F(BubbleDialogDelegateViewTest, MultipleBubbleAnchorHighlightTestInOrder) {
  std::unique_ptr<Widget> anchor_widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                       Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  LabelButton* button =
      anchor_widget->SetContentsView(std::make_unique<LabelButton>(
          Button::PressedCallback(), std::u16string()));
  TestInkDrop* ink_drop = new TestInkDrop();
  test::InkDropHostTestApi(InkDrop::Get(button))
      .SetInkDrop(base::WrapUnique(ink_drop));
  TestBubbleDialogDelegateView* bubble_delegate_first =
      new TestBubbleDialogDelegateView(button);
  bubble_delegate_first->set_parent_window(anchor_widget->GetNativeView());
  bubble_delegate_first->set_close_on_deactivate(false);

  Widget* bubble_widget_first =
      BubbleDialogDelegateView::CreateBubble(bubble_delegate_first);
  bubble_widget_first->Show();
  bubble_delegate_first->OnBubbleWidgetVisibilityChanged(true);
  ASSERT_EQ(InkDropState::ACTIVATED, ink_drop->GetTargetInkDropState());

  TestBubbleDialogDelegateView* bubble_delegate_second =
      new TestBubbleDialogDelegateView(button);
  bubble_delegate_second->set_parent_window(anchor_widget->GetNativeView());
  Widget* bubble_widget_second =
      BubbleDialogDelegateView::CreateBubble(bubble_delegate_second);
  bubble_widget_second->Show();
  bubble_delegate_second->OnBubbleWidgetVisibilityChanged(true);
  ASSERT_EQ(InkDropState::ACTIVATED, ink_drop->GetTargetInkDropState());

  bubble_delegate_second->OnBubbleWidgetVisibilityChanged(false);
  bubble_widget_second->CloseNow();

  EXPECT_EQ(InkDropState::ACTIVATED, ink_drop->GetTargetInkDropState());
  bubble_widget_first->Close();
  bubble_delegate_first->OnBubbleWidgetVisibilityChanged(false);
  EXPECT_EQ(InkDropState::DEACTIVATED, ink_drop->GetTargetInkDropState());
}

TEST_F(BubbleDialogDelegateViewTest,
       MultipleBubbleAnchorHighlightTestOutOfOrder) {
  std::unique_ptr<Widget> anchor_widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                       Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  LabelButton* button =
      anchor_widget->SetContentsView(std::make_unique<LabelButton>(
          Button::PressedCallback(), std::u16string()));
  TestInkDrop* ink_drop = new TestInkDrop();
  test::InkDropHostTestApi(InkDrop::Get(button))
      .SetInkDrop(base::WrapUnique(ink_drop));
  TestBubbleDialogDelegateView* bubble_delegate_first =
      new TestBubbleDialogDelegateView(button);
  bubble_delegate_first->set_parent_window(anchor_widget->GetNativeView());
  bubble_delegate_first->set_close_on_deactivate(false);

  Widget* bubble_widget_first =
      BubbleDialogDelegateView::CreateBubble(bubble_delegate_first);
  bubble_widget_first->Show();
  bubble_delegate_first->OnBubbleWidgetVisibilityChanged(true);
  ASSERT_EQ(InkDropState::ACTIVATED, ink_drop->GetTargetInkDropState());

  TestBubbleDialogDelegateView* bubble_delegate_second =
      new TestBubbleDialogDelegateView(button);
  bubble_delegate_second->set_parent_window(anchor_widget->GetNativeView());
  Widget* bubble_widget_second =
      BubbleDialogDelegateView::CreateBubble(bubble_delegate_second);
  bubble_widget_second->Show();
  bubble_delegate_second->OnBubbleWidgetVisibilityChanged(true);
  ASSERT_EQ(InkDropState::ACTIVATED, ink_drop->GetTargetInkDropState());

  bubble_widget_first->CloseNow();

  EXPECT_EQ(InkDropState::ACTIVATED, ink_drop->GetTargetInkDropState());
  bubble_widget_second->Close();
  bubble_delegate_second->OnBubbleWidgetVisibilityChanged(false);
  EXPECT_EQ(InkDropState::DEACTIVATED, ink_drop->GetTargetInkDropState());
}

TEST_F(BubbleDialogDelegateViewTest, NoParentWidget) {
  test_views_delegate()->set_use_desktop_native_widgets(true);
#if BUILDFLAG(IS_CHROMEOS)
  test_views_delegate()->set_context(GetContext());
#endif
  BubbleDialogDelegateView* bubble_delegate =
      new TestBubbleDialogDelegateView(nullptr);
  bubble_delegate->set_has_parent(false);
  WidgetAutoclosePtr bubble_widget(
      BubbleDialogDelegateView::CreateBubble(bubble_delegate));
  EXPECT_EQ(bubble_delegate, bubble_widget->widget_delegate());
  EXPECT_EQ(bubble_widget.get(), bubble_delegate->GetWidget());
  EXPECT_EQ(nullptr, bubble_widget->parent());
}

TEST_F(BubbleDialogDelegateViewTest, InitiallyFocusedView) {
  std::unique_ptr<Widget> anchor_widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
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
  std::unique_ptr<Widget> anchor_widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  TestBubbleDialogDelegateView* bubble_delegate =
      new TestBubbleDialogDelegateView(anchor_widget->GetContentsView());
  BubbleDialogDelegateView::CreateBubble(bubble_delegate);
  BubbleFrameView* frame = bubble_delegate->GetBubbleFrameView();

  struct {
    const int point;
    const int hit;
  } kTestCases[] = {
      {0, HTTRANSPARENT},
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
  std::unique_ptr<Widget> anchor_widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
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

TEST_F(BubbleDialogDelegateViewTest, GetPrimaryWindowWidget) {
  std::unique_ptr<Widget> anchor_widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  BubbleDialogDelegateView* bubble_delegate =
      new TestBubbleDialogDelegateView(anchor_widget->GetContentsView());
  Widget* bubble_widget =
      BubbleDialogDelegateView::CreateBubble(bubble_delegate);
  EXPECT_EQ(anchor_widget.get(), anchor_widget->GetPrimaryWindowWidget());
  EXPECT_EQ(anchor_widget.get(), bubble_widget->GetPrimaryWindowWidget());
}

// Test that setting WidgetDelegate::SetCanActivate() to false makes the
// widget created via BubbleDialogDelegateView::CreateBubble() not activatable.
TEST_F(BubbleDialogDelegateViewTest, NotActivatable) {
  std::unique_ptr<Widget> anchor_widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
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
    std::unique_ptr<Widget> anchor_widget =
        CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                         Widget::InitParams::TYPE_WINDOW);
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
    std::unique_ptr<Widget> anchor_widget =
        CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                         Widget::InitParams::TYPE_WINDOW);
    BubbleDialogDelegateView* bubble_delegate =
        new TestBubbleDialogDelegateView(anchor_widget->GetContentsView());
    Widget* bubble_widget =
        BubbleDialogDelegateView::CreateBubble(bubble_delegate);
    bubble_widget->Show();

    ui::KeyEvent escape_event(ui::EventType::kKeyPressed, ui::VKEY_ESCAPE,
                              ui::EF_NONE);
    bubble_widget->OnKeyEvent(&escape_event);
    EXPECT_TRUE(bubble_widget->IsClosed());
  }

  {
    std::unique_ptr<Widget> anchor_widget =
        CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                         Widget::InitParams::TYPE_WINDOW);
    TestBubbleDialogDelegateView* bubble_delegate =
        new TestBubbleDialogDelegateView(anchor_widget->GetContentsView());
    Widget* bubble_widget =
        BubbleDialogDelegateView::CreateBubble(bubble_delegate);
    bubble_widget->Show();
    BubbleFrameView* frame_view = bubble_delegate->GetBubbleFrameView();
    frame_view->ResetViewShownTimeStampForTesting();
    Button* close_button = frame_view->close_;
    ASSERT_TRUE(close_button);
    test::ButtonTestApi(close_button)
        .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                    gfx::Point(), ui::EventTimeForNow(),
                                    ui::EF_NONE, ui::EF_NONE));
    EXPECT_TRUE(bubble_widget->IsClosed());
  }
}

TEST_F(BubbleDialogDelegateViewTest, PinBlocksCloseOnDeactivate) {
  std::unique_ptr<Widget> anchor_widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  BubbleDialogDelegateView* bubble_delegate =
      new TestBubbleDialogDelegateView(anchor_widget->GetContentsView());
  bubble_delegate->set_close_on_deactivate(true);
  Widget* bubble_widget =
      BubbleDialogDelegateView::CreateBubble(bubble_delegate);

  // Pin the bubble so it does not go away on loss of focus.
  auto pin = bubble_delegate->PreventCloseOnDeactivate();
  bubble_widget->Show();
  anchor_widget->Activate();
  EXPECT_FALSE(bubble_widget->IsClosed());

  // Unpin the window. The next time the bubble loses activation, it should
  // close as expected.
  pin.reset();
  bubble_widget->Activate();
  EXPECT_FALSE(bubble_widget->IsClosed());
  anchor_widget->Activate();
  EXPECT_TRUE(bubble_widget->IsClosed());
}

TEST_F(BubbleDialogDelegateViewTest, CloseOnDeactivatePinCanOutliveBubble) {
  std::unique_ptr<Widget> anchor_widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  BubbleDialogDelegateView* bubble_delegate =
      new TestBubbleDialogDelegateView(anchor_widget->GetContentsView());
  bubble_delegate->set_close_on_deactivate(true);
  Widget* bubble_widget =
      BubbleDialogDelegateView::CreateBubble(bubble_delegate);

  // Pin the bubble so it does not go away on loss of focus.
  auto pin = bubble_delegate->PreventCloseOnDeactivate();
  bubble_widget->Show();
  anchor_widget->Activate();
  EXPECT_FALSE(bubble_widget->IsClosed());
  bubble_widget->CloseNow();
  pin.reset();
}

TEST_F(BubbleDialogDelegateViewTest, MultipleCloseOnDeactivatePins) {
  std::unique_ptr<Widget> anchor_widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  BubbleDialogDelegateView* bubble_delegate =
      new TestBubbleDialogDelegateView(anchor_widget->GetContentsView());
  bubble_delegate->set_close_on_deactivate(true);
  Widget* bubble_widget =
      BubbleDialogDelegateView::CreateBubble(bubble_delegate);

  // Pin the bubble so it does not go away on loss of focus.
  auto pin = bubble_delegate->PreventCloseOnDeactivate();
  auto pin2 = bubble_delegate->PreventCloseOnDeactivate();
  bubble_widget->Show();
  anchor_widget->Activate();

  // Unpinning one pin does not reset the state; both must be unpinned.
  pin.reset();
  bubble_widget->Activate();
  EXPECT_FALSE(bubble_widget->IsClosed());
  anchor_widget->Activate();
  EXPECT_FALSE(bubble_widget->IsClosed());

  // Fully unpin the window. The next time the bubble loses activation,
  // it should close as expected.
  pin2.reset();
  bubble_widget->Activate();
  EXPECT_FALSE(bubble_widget->IsClosed());
  anchor_widget->Activate();
  EXPECT_TRUE(bubble_widget->IsClosed());
}

TEST_F(BubbleDialogDelegateViewTest, CustomTitle) {
  std::unique_ptr<Widget> anchor_widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  TestBubbleDialogDelegateView* bubble_delegate =
      new TestBubbleDialogDelegateView(anchor_widget->GetContentsView());
  constexpr int kTitleHeight = 20;
  View* title_view = bubble_delegate->set_title_view(
      std::make_unique<StaticSizedView>(gfx::Size(10, kTitleHeight)));
  Widget* bubble_widget =
      BubbleDialogDelegateView::CreateBubble(bubble_delegate);
  bubble_widget->Show();

  BubbleFrameView* bubble_frame = static_cast<BubbleFrameView*>(
      bubble_widget->non_client_view()->frame_view());
  EXPECT_EQ(title_view, bubble_frame->title());

  View* title_container = title_view->parent();
  EXPECT_EQ(bubble_frame, title_container->parent());
  // Title takes up the whole bubble width when there's no icon or close button.
  EXPECT_EQ(bubble_delegate->width(), title_view->size().width());
  EXPECT_EQ(kTitleHeight, title_view->size().height());

  bubble_delegate->show_close_button();

  views::test::RunScheduledLayout(bubble_frame);

  Button* close_button = bubble_frame->close_button();
  // Title moves over for the close button.
  EXPECT_GT(close_button->x(), title_container->bounds().right());

  LayoutProvider* provider = LayoutProvider::Get();
  const gfx::Insets content_margins = provider->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText);
  const gfx::Insets title_margins =
      provider->GetInsetsMetric(INSETS_DIALOG_TITLE);
  EXPECT_EQ(content_margins, bubble_delegate->margins());
  // Note there is no title_margins() accessor (it should not be customizable).

  // To perform checks on the precise size, first hide the dialog buttons so the
  // calculations are simpler (e.g. platform font discrepancies can be ignored).
  bubble_delegate->hide_buttons();

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
  frame_size = bubble_frame->GetContentsBounds();
  EXPECT_EQ(
      content_margins.height() + kContentSize.height() + title_margins.height(),
      frame_size.height());
  EXPECT_EQ(content_margins.width() + kContentSize.width(), frame_size.width());

  // Now hide the title properly. The margins should also disappear.
  bubble_delegate->set_should_show_window_title(false);
  bubble_widget->UpdateWindowTitle();
  // UpdateWindowTitle() will not trigger InvalidateLayout() when window_title
  // not changed.
  // TODO(crbug.com/330198011) Remove this InvalidateLayout() once this bug
  // fixed.
  bubble_frame->InvalidateLayout();
  frame_size = bubble_frame->GetContentsBounds();
  EXPECT_EQ(content_margins.height() + kContentSize.height(),
            frame_size.height());
  EXPECT_EQ(content_margins.width() + kContentSize.width(), frame_size.width());
}

// Ensure the BubbleFrameView correctly resizes when the title is provided by a
// StyledLabel.
TEST_F(BubbleDialogDelegateViewTest, StyledLabelTitle) {
  std::unique_ptr<Widget> anchor_widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  TestBubbleDialogDelegateView* bubble_delegate =
      new TestBubbleDialogDelegateView(anchor_widget->GetContentsView());
  StyledLabel* title_view =
      bubble_delegate->set_title_view(std::make_unique<StyledLabel>());
  title_view->SetText(u"123");

  Widget* bubble_widget =
      BubbleDialogDelegateView::CreateBubble(bubble_delegate);
  bubble_widget->Show();

  const gfx::Size size_before_new_title =
      bubble_widget->GetWindowBoundsInScreen().size();
  title_view->SetText(u"12");

  // A shorter title should change nothing, since both will be within the
  // minimum dialog width.
  EXPECT_EQ(size_before_new_title,
            bubble_widget->GetWindowBoundsInScreen().size());

  title_view->SetText(base::UTF8ToUTF16(std::string(200, '0')));

  // A (much) longer title should increase the height, but not the width.
  EXPECT_EQ(size_before_new_title.width(),
            bubble_widget->GetWindowBoundsInScreen().width());
  EXPECT_LT(size_before_new_title.height(),
            bubble_widget->GetWindowBoundsInScreen().height());
}

// Ensure associated buttons are highlighted or unhighlighted when the bubble
// widget is shown or hidden respectively.
TEST_F(BubbleDialogDelegateViewTest, AttachedWidgetShowsInkDropWhenVisible) {
  std::unique_ptr<Widget> anchor_widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                       Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  LabelButton* button =
      anchor_widget->SetContentsView(std::make_unique<LabelButton>(
          Button::PressedCallback(), std::u16string()));
  TestInkDrop* ink_drop = new TestInkDrop();
  test::InkDropHostTestApi(InkDrop::Get(button))
      .SetInkDrop(base::WrapUnique(ink_drop));
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
  bubble_delegate->OnBubbleWidgetVisibilityChanged(true);
  EXPECT_EQ(InkDropState::ACTIVATED, ink_drop->GetTargetInkDropState());

  bubble_widget->Close();
  bubble_delegate->OnBubbleWidgetVisibilityChanged(false);
  EXPECT_EQ(InkDropState::DEACTIVATED, ink_drop->GetTargetInkDropState());
}

// Ensure associated buttons are highlighted or unhighlighted when the bubble
// widget is shown or hidden respectively when highlighted button is set after
// widget is shown.
TEST_F(BubbleDialogDelegateViewTest, VisibleWidgetShowsInkDropOnAttaching) {
  std::unique_ptr<Widget> anchor_widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                       Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  LabelButton* button =
      anchor_widget->SetContentsView(std::make_unique<LabelButton>(
          Button::PressedCallback(), std::u16string()));
  TestInkDrop* ink_drop = new TestInkDrop();
  test::InkDropHostTestApi(InkDrop::Get(button))
      .SetInkDrop(base::WrapUnique(ink_drop));
  TestBubbleDialogDelegateView* bubble_delegate =
      new TestBubbleDialogDelegateView(nullptr);
  bubble_delegate->set_parent_window(anchor_widget->GetNativeView());

  Widget* bubble_widget =
      BubbleDialogDelegateView::CreateBubble(bubble_delegate);
  bubble_widget->Show();
  // Explicitly calling OnWidgetVisibilityChanged to test functionality for
  // OS_WIN. Outside of the test environment this happens automatically by way
  // of HWNDMessageHandler.
  bubble_delegate->OnBubbleWidgetVisibilityChanged(true);
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());
  bubble_delegate->SetHighlightedButton(button);
  EXPECT_EQ(InkDropState::ACTIVATED, ink_drop->GetTargetInkDropState());

  bubble_widget->Close();
  bubble_delegate->OnBubbleWidgetVisibilityChanged(false);
  EXPECT_EQ(InkDropState::DEACTIVATED, ink_drop->GetTargetInkDropState());
}

TEST_F(BubbleDialogDelegateViewTest, VisibleAnchorChanges) {
  std::unique_ptr<Widget> anchor_widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  TestBubbleDialogDelegateView* bubble_delegate =
      new TestBubbleDialogDelegateView(nullptr);
  bubble_delegate->set_parent_window(anchor_widget->GetNativeView());

  Widget* bubble_widget =
      BubbleDialogDelegateView::CreateBubble(bubble_delegate);
  bubble_widget->Show();
  EXPECT_TRUE(anchor_widget->ShouldPaintAsActive());

  bubble_widget->Hide();
}

TEST_F(BubbleDialogDelegateViewTest, GetThemeProvider_FromAnchorWidget) {
  std::unique_ptr<Widget> anchor_widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                       Widget::InitParams::TYPE_WINDOW_FRAMELESS);
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

const int kScreenWidth = 1024;
const int kScreenHeight = 768;

struct ArrowTestParameters {
  views::BubbleBorder::Arrow arrow;
  bool adjust_if_offscreen;
  gfx::Rect anchor_rect;
  views::BubbleBorder::Arrow expected_arrow;

  gfx::Size ExpectedSpace() const {
    gfx::Rect adjusted_anchor_rect = anchor_rect;
    adjusted_anchor_rect.Offset(
        0, ViewsTestBase::GetSystemReservedHeightAtTopOfScreen());
    gfx::Rect screen_rect = gfx::Rect(0, 0, kScreenWidth, kScreenHeight);

    return BubbleDialogDelegate::GetAvailableSpaceToPlaceBubble(
        expected_arrow, adjusted_anchor_rect, screen_rect);
  }
};

class BubbleDialogDelegateViewArrowTest
    : public BubbleDialogDelegateViewTest,
      public testing::WithParamInterface<ArrowTestParameters> {
 public:
  BubbleDialogDelegateViewArrowTest() { SetUpTestScreen(); }

  BubbleDialogDelegateViewArrowTest(const BubbleDialogDelegateViewArrowTest&) =
      delete;
  BubbleDialogDelegateViewArrowTest& operator=(
      const BubbleDialogDelegateViewArrowTest&) = delete;

  ~BubbleDialogDelegateViewArrowTest() override {
    display::Screen::SetScreenInstance(nullptr);
  }

 private:
  void SetUpTestScreen() {
    DCHECK(!display::test::TestScreen::Get());
    test_screen_ = std::make_unique<display::test::TestScreen>();
    display::Screen::SetScreenInstance(test_screen_.get());
    const display::Display test_display = test_screen_->GetPrimaryDisplay();
    display::Display display(test_display);
    display.set_id(0x2);
    display.set_bounds(gfx::Rect(0, 0, kScreenWidth, kScreenHeight));
    display.set_work_area(gfx::Rect(0, 0, kScreenWidth, kScreenHeight));
    test_screen_->display_list().RemoveDisplay(test_display.id());
    test_screen_->display_list().AddDisplay(
        display, display::DisplayList::Type::PRIMARY);
  }

  std::unique_ptr<display::test::TestScreen> test_screen_;
};

TEST_P(BubbleDialogDelegateViewArrowTest, AvailableScreenSpaceTest) {
#if BUILDFLAG(IS_OZONE)
  if (!ui::OzonePlatform::GetInstance()
           ->GetPlatformProperties()
           .supports_global_screen_coordinates) {
    GTEST_SKIP() << "Global screen coordinates unavailable";
  }
#endif
  std::unique_ptr<Widget> anchor_widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  auto* bubble_delegate =
      new TestBubbleDialogDelegateView(anchor_widget->GetContentsView());
  BubbleDialogDelegateView::CreateBubble(bubble_delegate);
  EXPECT_EQ(bubble_delegate->adjust_if_offscreen(),
            PlatformStyle::kAdjustBubbleIfOffscreen);

  const ArrowTestParameters kParam = GetParam();

  bubble_delegate->SetArrow(kParam.arrow);
  bubble_delegate->set_adjust_if_offscreen(kParam.adjust_if_offscreen);
  anchor_widget->GetContentsView()->SetBounds(
      kParam.anchor_rect.x(), kParam.anchor_rect.y(),
      kParam.anchor_rect.width(), kParam.anchor_rect.height());
  gfx::Size available_space =
      BubbleDialogDelegate::GetMaxAvailableScreenSpaceToPlaceBubble(
          bubble_delegate->GetAnchorView(), bubble_delegate->arrow(),
          bubble_delegate->adjust_if_offscreen(),
          BubbleFrameView::PreferredArrowAdjustment::kMirror);
  EXPECT_EQ(available_space, kParam.ExpectedSpace());
}

const int kAnchorFarRightX = 840;
const int kAnchorFarLeftX = 75;
const int kAnchorFarTopY = 57;
const int kAnchorFarBottomY = 730;
const int kAnchorLength = 28;

// When the anchor rect is positioned at different places Rect(x, y,
// kAnchorLength, kAnchorLength) with (x,y) set to far corners of the screen,
// available space to position the bubble should vary acc. to
// |adjust_if_offscreen_|.
const ArrowTestParameters kAnchorAtFarScreenCornersParams[] = {
    // Arrow at Top Right, no arrow adjustment needed
    {views::BubbleBorder::Arrow::TOP_RIGHT, true,
     gfx::Rect(kAnchorFarRightX, kAnchorFarTopY, kAnchorLength, kAnchorLength),
     views::BubbleBorder::Arrow::TOP_RIGHT},
    // Max size available when arrow is changed to Bottom Right
    {views::BubbleBorder::Arrow::TOP_RIGHT, true,
     gfx::Rect(kAnchorFarRightX,
               kAnchorFarBottomY,
               kAnchorLength,
               kAnchorLength),
     views::BubbleBorder::Arrow::BOTTOM_RIGHT},
    // Max size available when arrow is changed to Bottom Left
    {views::BubbleBorder::Arrow::TOP_RIGHT, true,
     gfx::Rect(kAnchorFarLeftX,
               kAnchorFarBottomY,
               kAnchorLength,
               kAnchorLength),
     views::BubbleBorder::Arrow::BOTTOM_LEFT},
    // Max size available when arrow is changed to Top Left
    {views::BubbleBorder::Arrow::TOP_RIGHT, true,
     gfx::Rect(kAnchorFarLeftX, kAnchorFarTopY, kAnchorLength, kAnchorLength),
     views::BubbleBorder::Arrow::TOP_LEFT},
    // Offscreen adjustment is off, available size is per Bottom Left
    // arrow
    {views::BubbleBorder::Arrow::BOTTOM_LEFT, false,
     gfx::Rect(kAnchorFarRightX, kAnchorFarTopY, kAnchorLength, kAnchorLength),
     views::BubbleBorder::Arrow::BOTTOM_LEFT},
    // Offscreen adjustment is off, available size is per Top Left arrow
    {views::BubbleBorder::Arrow::TOP_LEFT, false,
     gfx::Rect(kAnchorFarRightX,
               kAnchorFarBottomY,
               kAnchorLength,
               kAnchorLength),
     views::BubbleBorder::Arrow::TOP_LEFT},
    // Offscreen adjustment is off, available size is per Top Right arrow
    {views::BubbleBorder::Arrow::TOP_RIGHT, false,
     gfx::Rect(kAnchorFarLeftX,
               kAnchorFarBottomY,
               kAnchorLength,
               kAnchorLength),
     views::BubbleBorder::Arrow::TOP_RIGHT},
    // Offscreen adjustment is off, available size is per Bottom Right
    // arrow
    {views::BubbleBorder::Arrow::BOTTOM_RIGHT, false,
     gfx::Rect(kAnchorFarLeftX, kAnchorFarTopY, kAnchorLength, kAnchorLength),
     views::BubbleBorder::Arrow::BOTTOM_RIGHT}};

INSTANTIATE_TEST_SUITE_P(AnchorAtFarScreenCorners,
                         BubbleDialogDelegateViewArrowTest,
                         testing::ValuesIn(kAnchorAtFarScreenCornersParams));

// Tests whether the BubbleDialogDelegateView will create a layer backed
// ClientView when SetPaintClientToLayer is set to true.
TEST_F(BubbleDialogDelegateViewTest, WithClientLayerTest) {
  std::unique_ptr<Widget> anchor_widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  auto bubble_delegate = std::make_unique<BubbleDialogDelegateView>(
      nullptr, BubbleBorder::TOP_LEFT);
  bubble_delegate->SetPaintClientToLayer(true);
  bubble_delegate->set_parent_window(anchor_widget->GetNativeView());

  WidgetAutoclosePtr bubble_widget(
      BubbleDialogDelegateView::CreateBubble(std::move(bubble_delegate)));

  EXPECT_NE(nullptr, bubble_widget->client_view()->layer());
}

// Tests to ensure BubbleDialogDelegateView does not create a layer backed
// ClientView when SetPaintClientToLayer is set to false.
TEST_F(BubbleDialogDelegateViewTest, WithoutClientLayerTest) {
  std::unique_ptr<Widget> anchor_widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  auto bubble_delegate = std::make_unique<BubbleDialogDelegateView>(
      nullptr, BubbleBorder::TOP_LEFT);
  bubble_delegate->SetPaintClientToLayer(false);
  bubble_delegate->set_parent_window(anchor_widget->GetNativeView());

  WidgetAutoclosePtr bubble_widget(
      BubbleDialogDelegateView::CreateBubble(std::move(bubble_delegate)));

  EXPECT_EQ(nullptr, bubble_widget->client_view()->layer());
}

TEST_F(BubbleDialogDelegateViewTest, AlertAccessibleEvent) {
  views::test::AXEventCounter counter(views::AXEventManager::Get());
  std::unique_ptr<Widget> anchor_widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  auto bubble_delegate = std::make_unique<TestBubbleDialogDelegateView>(
      anchor_widget->GetContentsView());

  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kAlert));
  Widget* bubble_widget =
      BubbleDialogDelegateView::CreateBubble(std::move(bubble_delegate));
  bubble_widget->Show();
  // Bubbles with kDialog accessible role don't produce this event
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kAlert));

  auto alert_bubble_delegate =
      std::make_unique<TestAlertBubbleDialogDelegateView>(
          anchor_widget->GetContentsView());
  Widget* alert_bubble_widget =
      BubbleDialogDelegateView::CreateBubble(std::move(alert_bubble_delegate));
  alert_bubble_widget->Show();
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kAlert));
}

// Anchoring Tests -------------------------------------------------------------

namespace {

class AnchorTestBubbleDialogDelegateView : public BubbleDialogDelegateView {
 public:
  explicit AnchorTestBubbleDialogDelegateView(View* anchor_view)
      : BubbleDialogDelegateView(anchor_view, BubbleBorder::TOP_LEFT) {}

  AnchorTestBubbleDialogDelegateView(
      const AnchorTestBubbleDialogDelegateView&) = delete;
  AnchorTestBubbleDialogDelegateView& operator=(
      const AnchorTestBubbleDialogDelegateView&) = delete;

  ~AnchorTestBubbleDialogDelegateView() override = default;

  // DialogDelegate:
  // Avoid engaging the AX focus system on bubble activation. The AX focus
  // system is not fully initialized for these tests and can cause crashes.
  View* GetInitiallyFocusedView() override { return nullptr; }

  using BubbleDialogDelegateView::SetAnchorView;
};

// Provides functionality for testing bubble anchoring logic.
// Deriving from widget test provides some nice out-of-the-box functionality for
// creating and managing widgets.
class BubbleDialogDelegateViewAnchorTest : public test::WidgetTest {
 public:
  BubbleDialogDelegateViewAnchorTest() = default;

  BubbleDialogDelegateViewAnchorTest(
      const BubbleDialogDelegateViewAnchorTest&) = delete;
  BubbleDialogDelegateViewAnchorTest& operator=(
      const BubbleDialogDelegateViewAnchorTest&) = delete;

  ~BubbleDialogDelegateViewAnchorTest() override = default;

  // Anchors a bubble widget to another widget.
  void Anchor(Widget* bubble_widget, Widget* anchor_to) {
    Widget::ReparentNativeView(bubble_widget->GetNativeView(),
                               anchor_to->GetNativeView());
    static_cast<AnchorTestBubbleDialogDelegateView*>(
        bubble_widget->widget_delegate())
        ->SetAnchorView(GetAnchorView(anchor_to));
  }

  // Creates a test bubble dialog widget. If |anchor_to| is not specified, uses
  // dummy_widget().
  Widget* CreateBubble(Widget* anchor_to = nullptr) {
    if (!anchor_to) {
      anchor_to = dummy_widget();
    }
    View* const anchor_view = anchor_to ? GetAnchorView(anchor_to) : nullptr;
    auto* bubble_delegate = new AnchorTestBubbleDialogDelegateView(anchor_view);
    bubble_delegate->set_close_on_deactivate(false);
    return BubbleDialogDelegateView::CreateBubble(bubble_delegate);
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

  View* GetAnchorView(Widget* widget) {
    View* const contents_view = widget->GetContentsView();
    DCHECK(contents_view);
    return contents_view;
  }

 private:
  WidgetAutoclosePtr dummy_widget_;
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
  EXPECT_TRUE(bubble->ShouldPaintAsActive());

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
  EXPECT_TRUE(bubble->ShouldPaintAsActive());

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
  EXPECT_TRUE(bubble->ShouldPaintAsActive());

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
  EXPECT_TRUE(bubble->ShouldPaintAsActive());

  bubble->Activate();
  EXPECT_TRUE(widget->ShouldPaintAsActive());
  EXPECT_TRUE(bubble->ShouldPaintAsActive());

  Anchor(bubble, dummy_widget());
  EXPECT_FALSE(widget->ShouldPaintAsActive());
}

TEST_F(BubbleDialogDelegateViewAnchorTest,
       ActivationPassesAcrossChainOfAnchoredBubbles) {
  auto widget = CreateTopLevelWidget();
  auto other_widget = CreateTopLevelWidget();
  auto* bubble = CreateBubble();
  auto* bubble2 = CreateBubble();
  widget->ShowInactive();
  // Initially, both bubbles are parented to dummy_widget().
  bubble->ShowInactive();
  bubble2->Show();
  EXPECT_FALSE(widget->ShouldPaintAsActive());
  EXPECT_TRUE(bubble->ShouldPaintAsActive());
  EXPECT_TRUE(bubble2->ShouldPaintAsActive());

  // Change the bubble's parent to |widget|.
  Anchor(bubble, widget.get());
  EXPECT_FALSE(widget->ShouldPaintAsActive());
  EXPECT_FALSE(bubble->ShouldPaintAsActive());
  EXPECT_TRUE(bubble2->ShouldPaintAsActive());

  Anchor(bubble2, bubble);
  EXPECT_TRUE(widget->ShouldPaintAsActive());
  EXPECT_TRUE(bubble->ShouldPaintAsActive());
  EXPECT_TRUE(bubble2->ShouldPaintAsActive());

  other_widget->Show();
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
  bubble->ShowInactive();
  bubble2->Show();
  Anchor(bubble, widget.get());
  Anchor(bubble2, bubble);
  bubble->Close();
}

// Tests that if the anchor view has kWidgetForAnchoringKey property,
// uses that widget for anchoring.
TEST_F(BubbleDialogDelegateViewAnchorTest, WidgetForAnchoring) {
  auto widget = CreateTopLevelWidget();
  auto widget_for_anchoring = CreateTopLevelWidget();

  GetAnchorView(widget.get())
      ->SetProperty(kWidgetForAnchoringKey, widget_for_anchoring.get());
  auto* bubble = CreateBubble(widget.get());
  EXPECT_EQ(bubble->parent(), widget_for_anchoring.get());
}

TEST_F(BubbleDialogDelegateViewTest, BubbleMetrics) {
  base::HistogramTester histogram;

  std::unique_ptr<Widget> anchor_widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  TestBubbleDialogDelegateView* bubble_delegate =
      new TestBubbleDialogDelegateView(anchor_widget->GetContentsView());
  Widget* bubble_widget =
      BubbleDialogDelegateView::CreateBubble(bubble_delegate);

  histogram.ExpectTotalCount("Bubble.All.CloseReason", 0);
  histogram.ExpectTotalCount("Bubble.All.CreateToPresentationTime", 0);
  histogram.ExpectTotalCount("Bubble.All.CreateToVisibleTime", 0);
  histogram.ExpectTotalCount("Bubble.All.TimeVisible", 0);

  bubble_widget->Show();

  // Wait until the next frame after CreateToPresentationTime and
  // CreateToVisibleTime is fired.
  base::RunLoop run_loop;
  bubble_delegate->GetWidget()
      ->GetCompositor()
      ->RequestSuccessfulPresentationTimeForNextFrame(base::BindOnce(
          [](base::RunLoop* run_loop,
             const viz::FrameTimingDetails& frame_timing_details) {
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();

  bubble_widget->CloseNow();

  histogram.ExpectTotalCount("Bubble.All.CloseReason", 1);
  histogram.ExpectTotalCount("Bubble.All.CreateToPresentationTime", 1);
  histogram.ExpectTotalCount("Bubble.All.CreateToVisibleTime", 1);
  histogram.ExpectTotalCount("Bubble.All.TimeVisible", 1);
}

class TestBubbleUmaLogger : public BubbleDialogDelegate::BubbleUmaLogger {};

TEST_F(BubbleUmaLoggerTest, LogMetricFromView) {
  base::HistogramTester histogram;
  auto label = std::make_unique<Label>();
  TestBubbleUmaLogger logger;
  const std::array<const char*, 1> allow_names({"Label"});
  logger.set_allowed_class_names_for_testing(allow_names);
  logger.set_bubble_view(label.get());
  histogram.ExpectTotalCount("Bubble.All.Metric1", 0);
  histogram.ExpectTotalCount("Bubble.Label.Metric1", 0);
  logger.LogMetric(base::UmaHistogramTimes, "Metric1", base::Seconds(1));
  histogram.ExpectTotalCount("Bubble.All.Metric1", 1);
  histogram.ExpectTotalCount("Bubble.Label.Metric1", 1);
}

TEST_F(BubbleUmaLoggerTest, LogMetricFromDelegate) {
  base::HistogramTester histogram;

  auto anchored_view = std::make_unique<View>();
  BubbleDialogDelegate delegate(anchored_view.get(),
                                BubbleBorder::Arrow::TOP_LEFT);
  delegate.SetContentsView(std::make_unique<Label>());

  TestBubbleUmaLogger logger;
  const std::array<const char*, 1> allow_names({"Label"});
  logger.set_allowed_class_names_for_testing(allow_names);
  logger.set_delegate(&delegate);

  histogram.ExpectTotalCount("Bubble.All.Metric1", 0);
  histogram.ExpectTotalCount("Bubble.Label.Metric1", 0);
  logger.LogMetric(base::UmaHistogramTimes, "Metric1", base::Seconds(1));
  histogram.ExpectTotalCount("Bubble.All.Metric1", 1);
  histogram.ExpectTotalCount("Bubble.Label.Metric1", 1);
}

TEST_F(BubbleUmaLoggerTest, DoNotLogMetricNotFromAllowedClasses) {
  base::HistogramTester histogram;
  auto label = std::make_unique<Label>();
  TestBubbleUmaLogger logger;
  logger.set_bubble_view(label.get());

  histogram.ExpectTotalCount("Bubble.All.Metric1", 0);
  histogram.ExpectTotalCount("Bubble.Label.Metric1", 0);
  logger.LogMetric(base::UmaHistogramTimes, "Metric1", base::Seconds(1));
  histogram.ExpectTotalCount("Bubble.All.Metric1", 1);
  histogram.ExpectTotalCount("Bubble.Label.Metric1", 0);
}

}  // namespace views
