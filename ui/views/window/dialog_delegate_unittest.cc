// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/base/hit_test.h"
#include "ui/events/event_processor.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"
#include "ui/views/window/dialog_delegate.h"

#if defined(OS_MACOSX)
#include "ui/base/test/scoped_fake_full_keyboard_access.h"
#endif

namespace views {

namespace {

class TestDialog : public DialogDelegateView {
 public:
  TestDialog() : input_(new views::Textfield()) {
    DialogDelegate::set_draggable(true);
    AddChildView(input_);
  }
  ~TestDialog() override = default;

  void Init() {
    // Add the accelerator before being added to the widget hierarchy (before
    // DCV has registered its accelerator) to make sure accelerator handling is
    // not dependent on the order of AddAccelerator calls.
    EXPECT_FALSE(GetWidget());
    AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  }

  // WidgetDelegate overrides:
  bool ShouldShowWindowTitle() const override { return !title_.empty(); }
  bool ShouldShowCloseButton() const override { return show_close_button_; }

  // DialogDelegateView overrides:
  bool Cancel() override {
    canceled_ = true;
    return closeable_;
  }
  bool Accept() override {
    accepted_ = true;
    return closeable_;
  }
  bool Close() override {
    closed_ = true;
    return closeable_;
  }

  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(200, 200);
  }
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override {
    return should_handle_escape_;
  }
  base::string16 GetWindowTitle() const override { return title_; }
  View* GetInitiallyFocusedView() override { return input_; }
  int GetDialogButtons() const override { return dialog_buttons_; }

  void CheckAndResetStates(bool canceled,
                           bool accepted,
                           bool closed) {
    EXPECT_EQ(canceled, canceled_);
    canceled_ = false;
    EXPECT_EQ(accepted, accepted_);
    accepted_ = false;
    EXPECT_EQ(closed, closed_);
    closed_ = false;
  }

  void TearDown() {
    closeable_ = true;
    GetWidget()->Close();
  }

  void set_title(const base::string16& title) { title_ = title; }
  void set_show_close_button(bool show_close) {
    show_close_button_ = show_close;
  }
  void set_should_handle_escape(bool should_handle_escape) {
    should_handle_escape_ = should_handle_escape;
  }
  void set_dialog_buttons(int dialog_buttons) {
    dialog_buttons_ = dialog_buttons;
  }

  views::Textfield* input() { return input_; }

 private:
  views::Textfield* input_;
  bool canceled_ = false;
  bool accepted_ = false;
  bool closed_ = false;
  // Prevent the dialog from closing, for repeated ok and cancel button clicks.
  bool closeable_ = false;
  base::string16 title_;
  bool show_close_button_ = true;
  bool should_handle_escape_ = false;
  int dialog_buttons_ = ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;

  DISALLOW_COPY_AND_ASSIGN(TestDialog);
};

class DialogTest : public ViewsTestBase {
 public:
  DialogTest() = default;
  ~DialogTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    // These tests all expect to use a custom frame on the dialog so they can
    // control hit-testing and other behavior. Custom frames are only supported
    // with a parent widget, so create the parent widget here.
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW);
    params.bounds = gfx::Rect(10, 11, 200, 200);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    parent_widget_.Init(std::move(params));
    parent_widget_.Show();

    InitializeDialog();
    ShowDialog();
  }

  void TearDown() override {
    dialog_->TearDown();
    parent_widget_.Close();
    ViewsTestBase::TearDown();
  }

  void InitializeDialog() {
    if (dialog_)
      dialog_->TearDown();

    dialog_ = new TestDialog();
    dialog_->Init();
  }

  views::Widget* CreateDialogWidget(DialogDelegate* dialog) {
    views::Widget* widget = DialogDelegate::CreateDialogWidget(
        dialog, GetContext(), parent_widget_.GetNativeView());
    return widget;
  }

  void ShowDialog() { CreateDialogWidget(dialog_)->Show(); }

  void SimulateKeyEvent(const ui::KeyEvent& event) {
    ui::KeyEvent event_copy = event;
    if (dialog()->GetFocusManager()->OnKeyEvent(event_copy))
      dialog()->GetWidget()->OnKeyEvent(&event_copy);
  }

  TestDialog* dialog() const { return dialog_; }
  views::Widget* parent_widget() { return &parent_widget_; }

 private:
  views::Widget parent_widget_;
  TestDialog* dialog_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(DialogTest);
};

}  // namespace

TEST_F(DialogTest, AcceptAndCancel) {
  DialogClientView* client_view = dialog()->GetDialogClientView();
  LabelButton* ok_button = client_view->ok_button();
  LabelButton* cancel_button = client_view->cancel_button();

  // Check that return/escape accelerators accept/close dialogs.
  EXPECT_EQ(dialog()->input(), dialog()->GetFocusManager()->GetFocusedView());
  const ui::KeyEvent return_event(
      ui::ET_KEY_PRESSED, ui::VKEY_RETURN, ui::EF_NONE);
  SimulateKeyEvent(return_event);
  dialog()->CheckAndResetStates(false, true, false);
  const ui::KeyEvent escape_event(
      ui::ET_KEY_PRESSED, ui::VKEY_ESCAPE, ui::EF_NONE);
  SimulateKeyEvent(escape_event);
  dialog()->CheckAndResetStates(false, false, true);

// Check ok and cancel button behavior on a directed return key event. Buttons
// won't respond to a return key event on Mac, since it performs the default
// action.
#if defined(OS_MACOSX)
  EXPECT_FALSE(ok_button->OnKeyPressed(return_event));
  dialog()->CheckAndResetStates(false, false, false);
  EXPECT_FALSE(cancel_button->OnKeyPressed(return_event));
  dialog()->CheckAndResetStates(false, false, false);
#else
  EXPECT_TRUE(ok_button->OnKeyPressed(return_event));
  dialog()->CheckAndResetStates(false, true, false);
  EXPECT_TRUE(cancel_button->OnKeyPressed(return_event));
  dialog()->CheckAndResetStates(true, false, false);
#endif

  // Check that return accelerators cancel dialogs if cancel is focused, except
  // on Mac where return should perform the default action.
  cancel_button->RequestFocus();
  EXPECT_EQ(cancel_button, dialog()->GetFocusManager()->GetFocusedView());
  SimulateKeyEvent(return_event);
#if defined(OS_MACOSX)
  dialog()->CheckAndResetStates(false, true, false);
#else
  dialog()->CheckAndResetStates(true, false, false);
#endif

  // Check that escape can be overridden.
  dialog()->set_should_handle_escape(true);
  SimulateKeyEvent(escape_event);
  dialog()->CheckAndResetStates(false, false, false);
}

TEST_F(DialogTest, RemoveDefaultButton) {
  // Removing buttons from the dialog here should not cause a crash on close.
  delete dialog()->GetDialogClientView()->ok_button();
  delete dialog()->GetDialogClientView()->cancel_button();
}

TEST_F(DialogTest, HitTest_HiddenTitle) {
  // Ensure that BubbleFrameView hit-tests as expected when the title is hidden.
  const NonClientView* view = dialog()->GetWidget()->non_client_view();
  BubbleFrameView* frame = static_cast<BubbleFrameView*>(view->frame_view());

  constexpr struct {
    const int point;
    const int hit;
  } kCases[] = {
      {0, HTTRANSPARENT},
      {10, HTCAPTION},
      {20, HTNOWHERE},
      {50, HTCLIENT /* Space is reserved for the close button. */},
      {60, HTCLIENT},
      {1000, HTNOWHERE},
  };

  for (const auto test_case : kCases) {
    gfx::Point point(test_case.point, test_case.point);
    EXPECT_EQ(test_case.hit, frame->NonClientHitTest(point))
        << " at point " << test_case.point;
  }
}

TEST_F(DialogTest, HitTest_HiddenTitleNoCloseButton) {
  InitializeDialog();
  dialog()->set_show_close_button(false);
  ShowDialog();

  const NonClientView* view = dialog()->GetWidget()->non_client_view();
  BubbleFrameView* frame = static_cast<BubbleFrameView*>(view->frame_view());

  constexpr struct {
    const int point;
    const int hit;
  } kCases[] = {
      {0, HTTRANSPARENT}, {10, HTCAPTION}, {20, HTCLIENT},
      {50, HTCLIENT},     {60, HTCLIENT},  {1000, HTNOWHERE},
  };

  for (const auto test_case : kCases) {
    gfx::Point point(test_case.point, test_case.point);
    EXPECT_EQ(test_case.hit, frame->NonClientHitTest(point))
        << " at point " << test_case.point;
  }
}

TEST_F(DialogTest, HitTest_WithTitle) {
  // Ensure that BubbleFrameView hit-tests as expected when the title is shown
  // and the modal type is something other than not modal.
  const NonClientView* view = dialog()->GetWidget()->non_client_view();
  dialog()->set_title(base::ASCIIToUTF16("Title"));
  dialog()->GetWidget()->UpdateWindowTitle();
  dialog()->GetWidget()->LayoutRootViewIfNecessary();
  BubbleFrameView* frame = static_cast<BubbleFrameView*>(view->frame_view());

  constexpr struct {
    const int point;
    const int hit;
  } kCases[] = {
      {0, HTTRANSPARENT}, {10, HTCAPTION}, {20, HTCAPTION},
      {50, HTCLIENT},     {60, HTCLIENT},  {1000, HTNOWHERE},
  };

  for (const auto test_case : kCases) {
    gfx::Point point(test_case.point, test_case.point);
    EXPECT_EQ(test_case.hit, frame->NonClientHitTest(point))
        << " at point " << test_case.point;
  }
}

TEST_F(DialogTest, HitTest_CloseButton) {
  const NonClientView* view = dialog()->GetWidget()->non_client_view();
  dialog()->set_show_close_button(true);
  BubbleFrameView* frame = static_cast<BubbleFrameView*>(view->frame_view());
  frame->ResetWindowControls();

  const gfx::Rect close_button_bounds =
      frame->GetCloseButtonForTesting()->bounds();
  EXPECT_EQ(HTCLOSE,
            frame->NonClientHitTest(gfx::Point(close_button_bounds.x() + 4,
                                               close_button_bounds.y() + 4)));
}

TEST_F(DialogTest, BoundsAccommodateTitle) {
  TestDialog* dialog2(new TestDialog());
  dialog2->set_title(base::ASCIIToUTF16("Title"));
  CreateDialogWidget(dialog2);

  // Remove the close button so it doesn't influence the bounds if it's taller
  // than the title.
  dialog()->set_show_close_button(false);
  dialog2->set_show_close_button(false);
  dialog()->GetWidget()->non_client_view()->ResetWindowControls();
  dialog2->GetWidget()->non_client_view()->ResetWindowControls();

  EXPECT_FALSE(dialog()->ShouldShowWindowTitle());
  EXPECT_TRUE(dialog2->ShouldShowWindowTitle());

  // Titled dialogs have taller initial frame bounds than untitled dialogs.
  View* frame1 = dialog()->GetWidget()->non_client_view()->frame_view();
  View* frame2 = dialog2->GetWidget()->non_client_view()->frame_view();
  EXPECT_LT(frame1->GetPreferredSize().height(),
            frame2->GetPreferredSize().height());

  // Giving the default test dialog a title will yield the same bounds.
  dialog()->set_title(base::ASCIIToUTF16("Title"));
  EXPECT_TRUE(dialog()->ShouldShowWindowTitle());

  dialog()->GetWidget()->UpdateWindowTitle();
  EXPECT_EQ(frame1->GetPreferredSize().height(),
            frame2->GetPreferredSize().height());

  dialog2->TearDown();
}

TEST_F(DialogTest, ActualBoundsMatchPreferredBounds) {
  dialog()->set_title(base::ASCIIToUTF16(
      "La la la look at me I'm a really really long title that needs to be "
      "really really long so that the title will multiline wrap."));
  dialog()->GetWidget()->UpdateWindowTitle();

  views::View* root_view = dialog()->GetWidget()->GetRootView();
  gfx::Size preferred_size(root_view->GetPreferredSize());
  EXPECT_FALSE(preferred_size.IsEmpty());
  root_view->SizeToPreferredSize();
  root_view->Layout();
  EXPECT_EQ(preferred_size, root_view->size());
}

// Tests default focus is assigned correctly when showing a new dialog.
TEST_F(DialogTest, InitialFocus) {
  EXPECT_TRUE(dialog()->input()->HasFocus());
  EXPECT_EQ(dialog()->input(), dialog()->GetFocusManager()->GetFocusedView());
}

// A dialog for testing initial focus with only an OK button.
class InitialFocusTestDialog : public DialogDelegateView {
 public:
  InitialFocusTestDialog() = default;
  ~InitialFocusTestDialog() override = default;

  views::View* OkButton() { return GetDialogClientView()->ok_button(); }

  // DialogDelegateView overrides:
  int GetDialogButtons() const override { return ui::DIALOG_BUTTON_OK; }

 private:
  DISALLOW_COPY_AND_ASSIGN(InitialFocusTestDialog);
};

// If the Widget can't be activated while the initial focus View is requesting
// focus, test it is still able to receive focus once the Widget is activated.
TEST_F(DialogTest, InitialFocusWithDeactivatedWidget) {
  InitialFocusTestDialog* dialog = new InitialFocusTestDialog();
  Widget* dialog_widget = CreateDialogWidget(dialog);
  // Set the initial focus while the Widget is unactivated to prevent the
  // initially focused View from receiving focus. Use a minimised state here to
  // prevent the Widget from being activated while this happens.
  dialog_widget->SetInitialFocus(ui::WindowShowState::SHOW_STATE_MINIMIZED);

  // Nothing should be focused, because the Widget is still deactivated.
  EXPECT_EQ(nullptr, dialog_widget->GetFocusManager()->GetFocusedView());
  EXPECT_EQ(dialog->OkButton(),
            dialog_widget->GetFocusManager()->GetStoredFocusView());
  dialog_widget->Show();
  // After activation, the initially focused View should have focus as intended.
  EXPECT_EQ(dialog->OkButton(),
            dialog_widget->GetFocusManager()->GetFocusedView());
  EXPECT_TRUE(dialog->OkButton()->HasFocus());
  dialog_widget->CloseNow();
}

// If the initially focused View provided is unfocusable, check the next
// available focusable View is focused.
TEST_F(DialogTest, UnfocusableInitialFocus) {
#if defined(OS_MACOSX)
  // On Mac, make all buttons unfocusable by turning off full keyboard access.
  // This is the more common configuration, and if a dialog has a focusable
  // textfield, tree or table, that should obtain focus instead.
  ui::test::ScopedFakeFullKeyboardAccess::GetInstance()
      ->set_full_keyboard_access_state(false);
#endif

  DialogDelegateView* dialog = new DialogDelegateView();
  Textfield* textfield = new Textfield();
  dialog->AddChildView(textfield);
  Widget* dialog_widget = CreateDialogWidget(dialog);

#if !defined(OS_MACOSX)
  // For non-Mac, turn off focusability on all the dialog's buttons manually.
  // This achieves the same effect as disabling full keyboard access.
  DialogClientView* dcv = dialog->GetDialogClientView();
  dcv->ok_button()->SetFocusBehavior(View::FocusBehavior::NEVER);
  dcv->cancel_button()->SetFocusBehavior(View::FocusBehavior::NEVER);
#endif

  // On showing the dialog, the initially focused View will be the OK button.
  // Since it is no longer focusable, focus should advance to the next focusable
  // View, which is |textfield|.
  dialog_widget->Show();
  EXPECT_TRUE(textfield->HasFocus());
  EXPECT_EQ(textfield, dialog->GetFocusManager()->GetFocusedView());
  dialog_widget->CloseNow();
}

}  // namespace views
