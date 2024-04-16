// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>  // Must come before other Windows system headers.

#include <oleacc.h>
#include <wrl/client.h>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_variant.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_edit_commands.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_test_api.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

class AXSystemCaretWinTest : public test::DesktopWidgetTest {
 public:
  AXSystemCaretWinTest()
      : widget_(nullptr), textfield_(nullptr), self_(CHILDID_SELF) {}
  AXSystemCaretWinTest(const AXSystemCaretWinTest&) = delete;
  AXSystemCaretWinTest& operator=(const AXSystemCaretWinTest&) = delete;
  ~AXSystemCaretWinTest() override = default;

  void SetUp() override {
    SetUpForInteractiveTests();
    test::DesktopWidgetTest::SetUp();

    widget_ = CreateTopLevelNativeWidget();
    widget_->SetBounds(gfx::Rect(0, 0, 200, 200));
    textfield_ = new Textfield();
    textfield_->SetBounds(0, 0, 200, 20);
    textfield_->SetText(u"Some text.");
    widget_->GetRootView()->AddChildView(textfield_.get());
    widget_->Show();
    test::WaitForWidgetActive(widget_, true);
    textfield_->RequestFocus();
    ASSERT_TRUE(widget_->IsActive());
    ASSERT_TRUE(textfield_->HasFocus());
    ASSERT_EQ(ui::TEXT_INPUT_TYPE_TEXT,
              widget_->GetInputMethod()->GetTextInputType());
  }

  void TearDown() override {
    DCHECK(!textfield_->owned_by_client());
    textfield_ = nullptr;
    // Calling CloseNow() will destroy the Widget.
    widget_.ExtractAsDangling()->CloseNow();
    test::DesktopWidgetTest::TearDown();
    ui::ResourceBundle::CleanupSharedInstance();
  }

 protected:
  raw_ptr<Widget> widget_;
  raw_ptr<Textfield> textfield_;
  base::win::ScopedVariant self_;
};

class WinAccessibilityCaretEventMonitor {
 public:
  WinAccessibilityCaretEventMonitor(UINT event_min, UINT event_max);
  WinAccessibilityCaretEventMonitor(const WinAccessibilityCaretEventMonitor&) =
      delete;
  WinAccessibilityCaretEventMonitor& operator=(
      const WinAccessibilityCaretEventMonitor&) = delete;
  ~WinAccessibilityCaretEventMonitor();

  // Blocks until the next event is received. When it's received, it
  // queries accessibility information about the object that fired the
  // event and populates the event, hwnd, role, state, and name in the
  // passed arguments.
  void WaitForNextEvent(DWORD* out_event, UINT* out_role, UINT* out_state);

 private:
  void OnWinEventHook(HWINEVENTHOOK handle,
                      DWORD event,
                      HWND hwnd,
                      LONG obj_id,
                      LONG child_id,
                      DWORD event_thread,
                      DWORD event_time);

  static void CALLBACK WinEventHookThunk(HWINEVENTHOOK handle,
                                         DWORD event,
                                         HWND hwnd,
                                         LONG obj_id,
                                         LONG child_id,
                                         DWORD event_thread,
                                         DWORD event_time);

  struct EventInfo {
    DWORD event;
    HWND hwnd;
    LONG obj_id;
    LONG child_id;
  };

  base::circular_deque<EventInfo> event_queue_;
  base::RunLoop loop_runner_;
  HWINEVENTHOOK win_event_hook_handle_;
  static WinAccessibilityCaretEventMonitor* instance_;
};

// static
WinAccessibilityCaretEventMonitor*
    WinAccessibilityCaretEventMonitor::instance_ = nullptr;

WinAccessibilityCaretEventMonitor::WinAccessibilityCaretEventMonitor(
    UINT event_min,
    UINT event_max) {
  CHECK(!instance_) << "There can be only one instance of"
                    << " WinAccessibilityCaretEventMonitor at a time.";
  instance_ = this;
  win_event_hook_handle_ =
      SetWinEventHook(event_min, event_max, nullptr,
                      &WinAccessibilityCaretEventMonitor::WinEventHookThunk,
                      GetCurrentProcessId(),
                      0,  // Hook all threads
                      WINEVENT_OUTOFCONTEXT);
}

WinAccessibilityCaretEventMonitor::~WinAccessibilityCaretEventMonitor() {
  UnhookWinEvent(win_event_hook_handle_);
  instance_ = nullptr;
}

void WinAccessibilityCaretEventMonitor::WaitForNextEvent(DWORD* out_event,
                                                         UINT* out_role,
                                                         UINT* out_state) {
  if (event_queue_.empty())
    loop_runner_.Run();

  EventInfo event_info = event_queue_.front();
  event_queue_.pop_front();

  *out_event = event_info.event;

  Microsoft::WRL::ComPtr<IAccessible> acc_obj;
  base::win::ScopedVariant child_variant;
  CHECK(S_OK == AccessibleObjectFromEvent(event_info.hwnd, event_info.obj_id,
                                          event_info.child_id, &acc_obj,
                                          child_variant.Receive()));

  base::win::ScopedVariant role_variant;
  if (S_OK == acc_obj->get_accRole(child_variant, role_variant.Receive()))
    *out_role = V_I4(role_variant.ptr());
  else
    *out_role = 0;

  base::win::ScopedVariant state_variant;
  if (S_OK == acc_obj->get_accState(child_variant, state_variant.Receive()))
    *out_state = V_I4(state_variant.ptr());
  else
    *out_state = 0;
}

void WinAccessibilityCaretEventMonitor::OnWinEventHook(HWINEVENTHOOK handle,
                                                       DWORD event,
                                                       HWND hwnd,
                                                       LONG obj_id,
                                                       LONG child_id,
                                                       DWORD event_thread,
                                                       DWORD event_time) {
  EventInfo event_info;
  event_info.event = event;
  event_info.hwnd = hwnd;
  event_info.obj_id = obj_id;
  event_info.child_id = child_id;
  event_queue_.push_back(event_info);
  loop_runner_.Quit();
}

// static
void CALLBACK
WinAccessibilityCaretEventMonitor::WinEventHookThunk(HWINEVENTHOOK handle,
                                                     DWORD event,
                                                     HWND hwnd,
                                                     LONG obj_id,
                                                     LONG child_id,
                                                     DWORD event_thread,
                                                     DWORD event_time) {
  if (instance_ && obj_id == OBJID_CARET) {
    instance_->OnWinEventHook(handle, event, hwnd, obj_id, child_id,
                              event_thread, event_time);
  }
}
}  // namespace

TEST_F(AXSystemCaretWinTest, TestOnCaretBoundsChangeInTextField) {
  TextfieldTestApi textfield_test_api(textfield_);
  Microsoft::WRL::ComPtr<IAccessible> caret_accessible;
  gfx::NativeWindow native_window = widget_->GetNativeWindow();
  ASSERT_NE(nullptr, native_window);
  HWND hwnd = native_window->GetHost()->GetAcceleratedWidget();
  EXPECT_HRESULT_SUCCEEDED(AccessibleObjectFromWindow(
      hwnd, static_cast<DWORD>(OBJID_CARET), IID_PPV_ARGS(&caret_accessible)));

  gfx::Rect window_bounds = native_window->GetBoundsInScreen();

  textfield_test_api.ExecuteTextEditCommand(
      ui::TextEditCommand::MOVE_TO_BEGINNING_OF_DOCUMENT);
  gfx::Point caret_position = textfield_test_api.GetCursorViewRect().origin() +
                              window_bounds.OffsetFromOrigin();
  LONG x, y, width, height;
  EXPECT_EQ(S_OK,
            caret_accessible->accLocation(&x, &y, &width, &height, self_));
  EXPECT_EQ(caret_position.x(), x);
  EXPECT_EQ(caret_position.y(), y);
  EXPECT_EQ(1, width);

  textfield_test_api.ExecuteTextEditCommand(
      ui::TextEditCommand::MOVE_TO_END_OF_DOCUMENT);
  gfx::Point caret_position2 = textfield_test_api.GetCursorViewRect().origin() +
                               window_bounds.OffsetFromOrigin();
  EXPECT_NE(caret_position, caret_position2);
  EXPECT_EQ(S_OK,
            caret_accessible->accLocation(&x, &y, &width, &height, self_));
  EXPECT_EQ(caret_position2.x(), x);
  EXPECT_EQ(caret_position2.y(), y);
  EXPECT_EQ(1, width);
}

TEST_F(AXSystemCaretWinTest, TestOnInputTypeChangeInTextField) {
  Microsoft::WRL::ComPtr<IAccessible> caret_accessible;
  gfx::NativeWindow native_window = widget_->GetNativeWindow();
  ASSERT_NE(nullptr, native_window);
  HWND hwnd = native_window->GetHost()->GetAcceleratedWidget();
  EXPECT_HRESULT_SUCCEEDED(AccessibleObjectFromWindow(
      hwnd, static_cast<DWORD>(OBJID_CARET), IID_PPV_ARGS(&caret_accessible)));
  LONG x, y, width, height;
  EXPECT_EQ(S_OK,
            caret_accessible->accLocation(&x, &y, &width, &height, self_));

  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  // Caret object should still be valid.
  EXPECT_EQ(S_OK,
            caret_accessible->accLocation(&x, &y, &width, &height, self_));

  // Retrieving the caret again should also work.
  caret_accessible.Reset();
  EXPECT_HRESULT_SUCCEEDED(AccessibleObjectFromWindow(
      hwnd, static_cast<DWORD>(OBJID_CARET), IID_PPV_ARGS(&caret_accessible)));
  LONG x2, y2, width2, height2;
  EXPECT_EQ(S_OK,
            caret_accessible->accLocation(&x2, &y2, &width2, &height2, self_));
  EXPECT_EQ(x, x2);
  EXPECT_EQ(y, y2);
  EXPECT_EQ(width, width2);
  EXPECT_EQ(height, height2);
}

TEST_F(AXSystemCaretWinTest, TestMovingWindow) {
  Microsoft::WRL::ComPtr<IAccessible> caret_accessible;
  gfx::NativeWindow native_window = widget_->GetNativeWindow();
  ASSERT_NE(nullptr, native_window);
  HWND hwnd = native_window->GetHost()->GetAcceleratedWidget();
  EXPECT_HRESULT_SUCCEEDED(AccessibleObjectFromWindow(
      hwnd, static_cast<DWORD>(OBJID_CARET), IID_PPV_ARGS(&caret_accessible)));
  LONG x, y, width, height;
  EXPECT_EQ(S_OK,
            caret_accessible->accLocation(&x, &y, &width, &height, self_));

  widget_->SetBounds(gfx::Rect(100, 100, 500, 500));
  LONG x2, y2, width2, height2;
  EXPECT_EQ(S_OK,
            caret_accessible->accLocation(&x2, &y2, &width2, &height2, self_));
  EXPECT_NE(x, x2);
  EXPECT_NE(y, y2);
  // The width and height of the caret shouldn't change.
  EXPECT_EQ(width, width2);
  EXPECT_EQ(height, height2);

  // Try maximizing the window.
  SendMessage(hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);

  // On Win7, maximizing the window causes our caret object to get destroyed and
  // re-created, so re-acquire it.
  caret_accessible.Reset();
  EXPECT_HRESULT_SUCCEEDED(AccessibleObjectFromWindow(
      hwnd, static_cast<DWORD>(OBJID_CARET), IID_PPV_ARGS(&caret_accessible)));

  LONG x3, y3, width3, height3;
  EXPECT_EQ(S_OK,
            caret_accessible->accLocation(&x3, &y3, &width3, &height3, self_));
  EXPECT_NE(x2, x3);
  EXPECT_NE(y2, y3);
  // The width and height of the caret shouldn't change.
  EXPECT_EQ(width, width3);
  EXPECT_EQ(height, height3);
}

// TODO(crbug.com/40820766): This test is flaky.
TEST_F(AXSystemCaretWinTest, DISABLED_TestCaretMSAAEvents) {
  TextfieldTestApi textfield_test_api(textfield_);
  Microsoft::WRL::ComPtr<IAccessible> caret_accessible;
  gfx::NativeWindow native_window = widget_->GetNativeWindow();
  ASSERT_NE(nullptr, native_window);
  HWND hwnd = native_window->GetHost()->GetAcceleratedWidget();
  EXPECT_HRESULT_SUCCEEDED(AccessibleObjectFromWindow(
      hwnd, static_cast<DWORD>(OBJID_CARET), IID_PPV_ARGS(&caret_accessible)));

  DWORD event;
  UINT role;
  UINT state;

  {
    // Set caret to start of textfield.
    WinAccessibilityCaretEventMonitor monitor(EVENT_OBJECT_SHOW,
                                              EVENT_OBJECT_LOCATIONCHANGE);
    textfield_test_api.ExecuteTextEditCommand(
        ui::TextEditCommand::MOVE_TO_BEGINNING_OF_DOCUMENT);
    monitor.WaitForNextEvent(&event, &role, &state);
    ASSERT_EQ(event, static_cast<DWORD>(EVENT_OBJECT_LOCATIONCHANGE))
        << "Event should be EVENT_OBJECT_LOCATIONCHANGE";
    ASSERT_EQ(role, static_cast<UINT>(ROLE_SYSTEM_CARET))
        << "Role should be ROLE_SYSTEM_CARET";
    ASSERT_EQ(state, static_cast<UINT>(0)) << "State should be 0";
  }

  {
    // Set caret to end of textfield.
    WinAccessibilityCaretEventMonitor monitor(EVENT_OBJECT_SHOW,
                                              EVENT_OBJECT_LOCATIONCHANGE);
    textfield_test_api.ExecuteTextEditCommand(
        ui::TextEditCommand::MOVE_TO_END_OF_DOCUMENT);
    monitor.WaitForNextEvent(&event, &role, &state);
    ASSERT_EQ(event, static_cast<DWORD>(EVENT_OBJECT_LOCATIONCHANGE))
        << "Event should be EVENT_OBJECT_LOCATIONCHANGE";
    ASSERT_EQ(role, static_cast<UINT>(ROLE_SYSTEM_CARET))
        << "Role should be ROLE_SYSTEM_CARET";
    ASSERT_EQ(state, static_cast<UINT>(0)) << "State should be 0";
  }

  {
    // Move focus to a button.
    LabelButton button{Button::PressedCallback(), std::u16string()};
    button.SetBounds(500, 0, 200, 20);
    widget_->GetRootView()->AddChildView(&button);
    WinAccessibilityCaretEventMonitor monitor(EVENT_OBJECT_SHOW,
                                              EVENT_OBJECT_LOCATIONCHANGE);
    widget_->Show();
    test::WaitForWidgetActive(widget_, true);
    button.SetFocusBehavior(View::FocusBehavior::ALWAYS);
    button.RequestFocus();
    monitor.WaitForNextEvent(&event, &role, &state);
    ASSERT_EQ(event, static_cast<DWORD>(EVENT_OBJECT_HIDE))
        << "Event should be EVENT_OBJECT_HIDE";
    ASSERT_EQ(role, static_cast<UINT>(ROLE_SYSTEM_CARET))
        << "Role should be ROLE_SYSTEM_CARET";
    ASSERT_EQ(state, static_cast<UINT>(STATE_SYSTEM_INVISIBLE))
        << "State should be STATE_SYSTEM_INVISIBLE";
  }

  {
    // Move focus back to the text field.
    WinAccessibilityCaretEventMonitor monitor(EVENT_OBJECT_SHOW,
                                              EVENT_OBJECT_LOCATIONCHANGE);
    textfield_->RequestFocus();
    monitor.WaitForNextEvent(&event, &role, &state);
    ASSERT_EQ(event, static_cast<DWORD>(EVENT_OBJECT_SHOW))
        << "Event should be EVENT_OBJECT_SHOW";
    ASSERT_EQ(role, static_cast<UINT>(ROLE_SYSTEM_CARET))
        << "Role should be ROLE_SYSTEM_CARET";
    ASSERT_EQ(state, static_cast<UINT>(0)) << "State should be 0";
  }
}

}  // namespace views
