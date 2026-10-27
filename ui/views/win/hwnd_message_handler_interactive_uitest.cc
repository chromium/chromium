// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/hwnd_message_handler.h"

#include <windows.h>

#include <algorithm>
#include <iterator>
#include <memory>

#include "base/logging.h"
#include "base/process/process_handle.h"
#include "base/test/run_until.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ime/mock_input_method.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/win/foreground_helper.h"
#include "ui/base/win/window_event_target.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/win/hwnd_util.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/win/hwnd_message_handler_delegate.h"

namespace views {

namespace {

class TestHWNDMessageHandlerDelegate : public HWNDMessageHandlerDelegate {
 public:
  TestHWNDMessageHandlerDelegate() {
    mock_input_method_ = std::make_unique<ui::MockInputMethod>(nullptr);
  }
  TestHWNDMessageHandlerDelegate(const TestHWNDMessageHandlerDelegate&) =
      delete;
  TestHWNDMessageHandlerDelegate& operator=(
      const TestHWNDMessageHandlerDelegate&) = delete;
  ~TestHWNDMessageHandlerDelegate() override = default;

  // HWNDMessageHandlerDelegate:
  ui::InputMethod* GetHWNDMessageDelegateInputMethod() override {
    return mock_input_method_.get();
  }
  bool HasNonClientView() const override { return false; }
  FrameMode GetFrameMode() const override { return FrameMode::SYSTEM_DRAWN; }
  void ShowCustomSystemMenu(const gfx::Point& screen_point) override {}
  bool UsesNativeSystemMenu() const override { return true; }
  bool HasFrame() const override { return false; }
  bool ShouldPaintAsActive() const override { return false; }
  void SchedulePaint() override {}
  bool CanResize() const override { return false; }
  bool CanMaximize() const override { return false; }
  bool CanMinimize() const override { return false; }
  bool CanActivate() const override { return false; }
  bool WidgetSizeIsClientSize() const override { return false; }
  bool IsModal() const override { return false; }
  int GetInitialShowState() const override { return SW_SHOWNORMAL; }
  int GetNonClientComponent(const gfx::Point& point) const override {
    return HTNOWHERE;
  }
  void GetWindowMask(const gfx::Size& size_px, SkPath* mask) override {}
  bool GetClientAreaInsets(gfx::Insets* insets,
                           int frame_thickness) const override {
    return false;
  }
  bool GetDwmFrameInsetsInPixels(gfx::Insets* insets) const override {
    return false;
  }
  void GetMinMaxSize(gfx::Size* min_size, gfx::Size* max_size) const override {}
  gfx::Size GetRootViewSize() const override { return gfx::Size(); }
  gfx::Size DIPToScreenSize(const gfx::Size& dip_size) const override {
    return dip_size;
  }
  void ResetWindowControls() override {}
  gfx::NativeViewAccessible GetNativeViewAccessible() override {
    return nullptr;
  }
  gfx::NativeViewAccessible GetParentNativeViewAccessible() override {
    return nullptr;
  }
  void HandleActivationChanged(bool active) override {}
  bool HandleAppCommand(int command) override { return false; }
  void HandleCancelMode() override {}
  void HandleCaptureLost() override {}
  void HandleClose() override {}
  bool HandleCommand(int command) override { return false; }
  void HandleAccelerator(const ui::Accelerator& accelerator) override {
    accelerator_handled_count_++;
    last_accelerator_ = accelerator;
  }
  void HandleCreate() override {}

  int accelerator_handled_count() const { return accelerator_handled_count_; }
  const ui::Accelerator& last_accelerator() const { return last_accelerator_; }
  void reset_accelerator_state() {
    accelerator_handled_count_ = 0;
    last_accelerator_ = ui::Accelerator();
  }
  void HandleDestroying() override {}
  void HandleDestroyed() override {}
  bool HandleInitialFocus(ui::mojom::WindowShowState show_state) override {
    return false;
  }
  void HandleDisplayChange() override {}
  void HandleBeginWMSizeMove() override {}
  void HandleEndWMSizeMove() override {}
  void HandleBeginUserResize() override {}
  void HandleEndUserResize() override {}
  void HandleBeginUserDrag() override {}
  void HandleEndUserDrag() override {}
  void HandleMove() override {}
  void HandleWorkAreaChanged() override {}
  void HandleVisibilityChanged(bool visible) override {}
  void HandleWindowMinimizedOrRestored(bool restored) override {}
  void HandleClientSizeChanged(const gfx::Size& new_size) override {}
  void HandleFrameChanged() override {}
  void HandleNativeFocus(HWND last_focused_window) override {}
  void HandleNativeBlur(HWND focused_window) override {}
  bool HandleMouseEvent(ui::MouseEvent* event) override { return false; }
  void HandleKeyEvent(ui::KeyEvent* event) override {}
  void HandleTouchEvent(ui::TouchEvent* event) override {}
  bool HandleIMEMessage(UINT message,
                        WPARAM w_param,
                        LPARAM l_param,
                        LRESULT* result) override {
    return false;
  }
  void HandleInputLanguageChange(DWORD character_set,
                                 HKL input_language_id) override {}
  void HandlePaintAccelerated(const gfx::Rect& invalid_rect) override {}
  void HandleMenuLoop(bool in_menu_loop) override {}
  bool PreHandleMSG(UINT message,
                    WPARAM w_param,
                    LPARAM l_param,
                    LRESULT* result) override {
    return false;
  }
  void PostHandleMSG(UINT message, WPARAM w_param, LPARAM l_param) override {}
  bool HandleScrollEvent(ui::ScrollEvent* event) override { return false; }
  bool HandleGestureEvent(ui::GestureEvent* event) override { return false; }
  void HandleWindowScaleFactorChanged(float window_scale_factor) override {}
  void HandleHeadlessWindowBoundsChanged(const gfx::Rect& bounds) override {}
  HBRUSH GetBackgroundPaintBrush() override { return nullptr; }

 private:
  std::unique_ptr<ui::MockInputMethod> mock_input_method_;
  int accelerator_handled_count_ = 0;
  ui::Accelerator last_accelerator_;
};

class ActivatableHWNDMessageHandlerDelegate
    : public TestHWNDMessageHandlerDelegate {
 public:
  // TestHWNDMessageHandlerDelegate:
  bool CanActivate() const override { return true; }
};

// Brings `hwnd` to the foreground. ::SetForegroundWindow() fails if a window of
// another process is in the foreground and this process didn't receive the last
// input event, so use ForegroundHelper to send input and close stray foreign
// foreground windows (see ui_test_utils::ShowAndFocusNativeWindow()).
void EnsureForeground(HWND hwnd) {
  for (int attempt = 0; attempt < 5; ++attempt) {
    if (::GetForegroundWindow() == hwnd) {
      return;
    }
    ui::ForegroundHelper::SetForeground(hwnd);

    HWND foreground = ::GetForegroundWindow();
    if (foreground == hwnd) {
      return;
    }

    // Describe the window that holds the foreground, to help diagnose failures.
    wchar_t title[256] = {};
    ::GetWindowText(foreground, title, std::size(title));
    DWORD process_id = 0;
    ::GetWindowThreadProcessId(foreground, &process_id);
    LOG(ERROR)
        << "Failed to bring window to the foreground. Foreground window: "
        << foreground << ", class: " << gfx::GetClassName(foreground)
        << ", title: " << title << ", process id: " << process_id;

    if (!foreground || process_id == base::GetCurrentProcId()) {
      break;
    }

    // Attempt to close a stray window left behind by a previous test/task.
    ::PostMessage(foreground, WM_CLOSE, 0, 0);
    if (!base::test::RunUntil(
            [&]() { return ::GetForegroundWindow() != foreground; })) {
      break;
    }
  }
}

// Returns true if `above` is higher than `below` in the z-order.
bool IsWindowAbove(HWND above, HWND below) {
  for (HWND w = ::GetWindow(below, GW_HWNDPREV); w;
       w = ::GetWindow(w, GW_HWNDPREV)) {
    if (w == above) {
      return true;
    }
  }
  return false;
}

}  // namespace

using HWNDMessageHandlerTest = test::DesktopWidgetTestInteractive;

TEST_F(HWNDMessageHandlerTest, Init) {
  TestHWNDMessageHandlerDelegate delegate;
  std::unique_ptr<HWNDMessageHandler> handler(
      HWNDMessageHandler::Create(&delegate, "test"));
  ASSERT_TRUE(handler);
  handler->Init(nullptr, gfx::Rect(0, 0, 100, 100));
  ASSERT_TRUE(handler->hwnd());
}

TEST_F(HWNDMessageHandlerTest, GetOwnedWindows) {
  TestHWNDMessageHandlerDelegate parent_delegate;
  std::unique_ptr<HWNDMessageHandler> parent_handler(
      HWNDMessageHandler::Create(&parent_delegate, "parent"));
  ASSERT_TRUE(parent_handler);
  parent_handler->Init(nullptr, gfx::Rect(0, 0, 200, 200));
  ASSERT_TRUE(parent_handler->hwnd());

  TestHWNDMessageHandlerDelegate child_delegate;
  std::unique_ptr<HWNDMessageHandler> child_handler(
      HWNDMessageHandler::Create(&child_delegate, "child"));
  ASSERT_TRUE(child_handler);
  child_handler->set_window_style(WS_POPUP);
  child_handler->Init(parent_handler->hwnd(), gfx::Rect(0, 0, 100, 100));
  ASSERT_TRUE(child_handler->hwnd());

  std::vector<HWND> owned_windows = parent_handler->GetOwnedWindows();
  ASSERT_EQ(1u, owned_windows.size());
  EXPECT_TRUE(std::ranges::contains(owned_windows, child_handler->hwnd()));

  child_handler->CloseNow();
  parent_handler->CloseNow();
}

TEST_F(HWNDMessageHandlerTest, GetOwnedWindows_Depth3) {
  TestHWNDMessageHandlerDelegate grandparent_delegate;
  std::unique_ptr<HWNDMessageHandler> grandparent_handler(
      HWNDMessageHandler::Create(&grandparent_delegate, "grandparent"));
  ASSERT_TRUE(grandparent_handler);
  grandparent_handler->Init(nullptr, gfx::Rect(0, 0, 300, 300));
  ASSERT_TRUE(grandparent_handler->hwnd());

  TestHWNDMessageHandlerDelegate parent1_delegate;
  std::unique_ptr<HWNDMessageHandler> parent1_handler(
      HWNDMessageHandler::Create(&parent1_delegate, "parent1"));
  ASSERT_TRUE(parent1_handler);
  parent1_handler->set_window_style(WS_POPUP);
  parent1_handler->Init(grandparent_handler->hwnd(), gfx::Rect(0, 0, 200, 200));
  ASSERT_TRUE(parent1_handler->hwnd());

  TestHWNDMessageHandlerDelegate parent2_delegate;
  std::unique_ptr<HWNDMessageHandler> parent2_handler(
      HWNDMessageHandler::Create(&parent2_delegate, "parent2"));
  ASSERT_TRUE(parent2_handler);
  parent2_handler->set_window_style(WS_POPUP);
  parent2_handler->Init(grandparent_handler->hwnd(), gfx::Rect(0, 0, 50, 50));
  ASSERT_TRUE(parent2_handler->hwnd());

  TestHWNDMessageHandlerDelegate child_delegate;
  std::unique_ptr<HWNDMessageHandler> child_handler(
      HWNDMessageHandler::Create(&child_delegate, "child"));
  ASSERT_TRUE(child_handler);
  child_handler->set_window_style(WS_POPUP);
  child_handler->Init(parent1_handler->hwnd(), gfx::Rect(0, 0, 100, 100));
  ASSERT_TRUE(child_handler->hwnd());

  std::vector<HWND> owned_windows = grandparent_handler->GetOwnedWindows();
  ASSERT_EQ(3u, owned_windows.size());
  EXPECT_TRUE(std::ranges::contains(owned_windows, parent1_handler->hwnd()));
  EXPECT_TRUE(std::ranges::contains(owned_windows, parent2_handler->hwnd()));
  EXPECT_TRUE(std::ranges::contains(owned_windows, child_handler->hwnd()));

  child_handler->CloseNow();
  parent2_handler->CloseNow();
  parent1_handler->CloseNow();
  grandparent_handler->CloseNow();
}

// Tests that SC_KEYMENU (Alt key) is suppressed when mouse is locked
// (pointer lock is active).
TEST_F(HWNDMessageHandlerTest, AltKeySuppressedWhenMouseLocked) {
  TestHWNDMessageHandlerDelegate delegate;
  std::unique_ptr<HWNDMessageHandler> handler(
      HWNDMessageHandler::Create(&delegate, "test"));
  ASSERT_TRUE(handler);
  handler->Init(nullptr, gfx::Rect(0, 0, 100, 100));
  ASSERT_TRUE(handler->hwnd());

  // Without mouse lock, SC_KEYMENU should trigger HandleAccelerator.
  EXPECT_FALSE(handler->mouse_locked());
  delegate.reset_accelerator_state();
  ::SendMessage(handler->hwnd(), WM_SYSCOMMAND, SC_KEYMENU, 0);
  EXPECT_EQ(1, delegate.accelerator_handled_count());

  // With mouse lock, SC_KEYMENU should be suppressed.
  handler->set_mouse_locked(true);
  EXPECT_TRUE(handler->mouse_locked());
  delegate.reset_accelerator_state();
  ::SendMessage(handler->hwnd(), WM_SYSCOMMAND, SC_KEYMENU, 0);
  EXPECT_EQ(0, delegate.accelerator_handled_count());

  // After unlocking, SC_KEYMENU should work again.
  handler->set_mouse_locked(false);
  EXPECT_FALSE(handler->mouse_locked());
  delegate.reset_accelerator_state();
  ::SendMessage(handler->hwnd(), WM_SYSCOMMAND, SC_KEYMENU, 0);
  EXPECT_EQ(1, delegate.accelerator_handled_count());

  handler->CloseNow();
}

// Tests that enabling/disabling WM_INPUT mode properly resets the tracked
// button state to avoid stale state when pointer lock is toggled.
TEST_F(HWNDMessageHandlerTest, RawInputButtonStateResetOnDisable) {
  TestHWNDMessageHandlerDelegate delegate;
  std::unique_ptr<HWNDMessageHandler> handler(
      HWNDMessageHandler::Create(&delegate, "test"));
  ASSERT_TRUE(handler);
  handler->Init(nullptr, gfx::Rect(0, 0, 100, 100));
  ASSERT_TRUE(handler->hwnd());

  // Initially not using WM_INPUT.
  EXPECT_FALSE(handler->using_wm_input());
  EXPECT_EQ(ui::EF_NONE, handler->raw_input_button_state_for_testing());

  // Enable WM_INPUT mode (simulating pointer lock).
  handler->set_using_wm_input(true);
  EXPECT_TRUE(handler->using_wm_input());
  EXPECT_EQ(ui::EF_NONE, handler->raw_input_button_state_for_testing());

  // Simulate some button state being tracked (e.g., left button down).
  handler->set_raw_input_button_state_for_testing(ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON,
            handler->raw_input_button_state_for_testing());

  // Disable WM_INPUT mode - this should reset button state to EF_NONE.
  handler->set_using_wm_input(false);
  EXPECT_FALSE(handler->using_wm_input());
  EXPECT_EQ(ui::EF_NONE, handler->raw_input_button_state_for_testing());

  handler->CloseNow();
}

TEST_F(HWNDMessageHandlerTest, UsesNativeSystemMenuControlsCapture) {
  class MockDelegate : public TestHWNDMessageHandlerDelegate {
   public:
    void set_uses_native_system_menu(bool uses) { uses_native_ = uses; }
    bool UsesNativeSystemMenu() const override { return uses_native_; }
    void ShowCustomSystemMenu(const gfx::Point& screen_point) override {}

   private:
    bool uses_native_ = true;
  };

  MockDelegate delegate;
  std::unique_ptr<HWNDMessageHandler> handler(
      HWNDMessageHandler::Create(&delegate, "test"));
  ASSERT_TRUE(handler);
  handler->Init(nullptr, gfx::Rect(0, 0, 100, 100));
  ASSERT_TRUE(handler->hwnd());

  // Case 1: UsesNativeSystemMenu is true.
  delegate.set_uses_native_system_menu(true);

  ::SetCapture(nullptr);  // Clear capture first.
  ::SendMessage(handler->hwnd(), WM_NCRBUTTONDOWN, HTCAPTION,
                MAKELPARAM(10, 10));

  EXPECT_EQ(handler->hwnd(), ::GetCapture());

  // Clean up capture.
  ::ReleaseCapture();

  // Case 2: UsesNativeSystemMenu is false.
  delegate.set_uses_native_system_menu(false);

  ::SetCapture(nullptr);  // Clear capture first.
  ::SendMessage(handler->hwnd(), WM_NCRBUTTONDOWN, HTCAPTION,
                MAKELPARAM(10, 10));

  EXPECT_NE(handler->hwnd(), ::GetCapture());

  handler->CloseNow();
}

TEST_F(HWNDMessageHandlerTest, DeferredDestruction) {
  TestHWNDMessageHandlerDelegate delegate;
  HWNDMessageHandler* handler =
      HWNDMessageHandler::Create(&delegate, "test").release();
  ASSERT_TRUE(handler);
  handler->Init(nullptr, gfx::Rect(0, 0, 100, 100));
  ASSERT_TRUE(handler->hwnd());

  base::WeakPtr<HWNDMessageHandler> weak_handler = handler->GetWeakPtr();
  ASSERT_TRUE(weak_handler);

  handler->DestroyHandler();

  EXPECT_TRUE(weak_handler);
  EXPECT_TRUE(weak_handler->delete_pending_);

  ui::WindowEventTarget* target = weak_handler.get();
  ASSERT_TRUE(target);
  bool handled = false;
  target->HandlePointerMessage(WM_POINTERDOWN, 0, 0, &handled);
  EXPECT_FALSE(handled);

  EXPECT_TRUE(base::test::RunUntil([&]() { return !weak_handler; }));

  EXPECT_FALSE(weak_handler);
}

// Verifies that Activate() on an active owned window raises its owner, when
// another window is between them in the z-order.
TEST_F(HWNDMessageHandlerTest, ActivateOwnedWindowRaisesOwner) {
  ActivatableHWNDMessageHandlerDelegate owner_delegate;
  std::unique_ptr<HWNDMessageHandler> owner(
      HWNDMessageHandler::Create(&owner_delegate, "owner"));
  owner->Init(nullptr, gfx::Rect(0, 0, 100, 100));
  owner->Show(ui::mojom::WindowShowState::kNormal, gfx::Rect());

  ActivatableHWNDMessageHandlerDelegate owned_delegate;
  std::unique_ptr<HWNDMessageHandler> owned(
      HWNDMessageHandler::Create(&owned_delegate, "owned"));
  owned->set_window_style(WS_POPUP);
  owned->Init(owner->hwnd(), gfx::Rect(0, 0, 50, 50));
  owned->Show(ui::mojom::WindowShowState::kNormal, gfx::Rect());
  EnsureForeground(owned->hwnd());
  ASSERT_TRUE(owned->IsActive());
  ASSERT_EQ(::GetForegroundWindow(), owned->hwnd());

  // Insert an unrelated window between the owned window and its owner.
  ActivatableHWNDMessageHandlerDelegate other_delegate;
  std::unique_ptr<HWNDMessageHandler> other(
      HWNDMessageHandler::Create(&other_delegate, "other"));
  other->Init(nullptr, gfx::Rect(0, 0, 100, 100));
  ::ShowWindow(other->hwnd(), SW_SHOWNOACTIVATE);
  ::SetWindowPos(other->hwnd(), owned->hwnd(), 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  ASSERT_TRUE(owned->IsActive());
  ASSERT_EQ(::GetForegroundWindow(), owned->hwnd());
  ASSERT_TRUE(IsWindowAbove(owned->hwnd(), other->hwnd()));
  ASSERT_TRUE(IsWindowAbove(other->hwnd(), owner->hwnd()));

  // Activate the owned window. Expect it to stay active and its owner to be
  // raised above the other window, while staying below the owned window.
  owned->Activate();
  EXPECT_TRUE(owned->IsActive());
  EXPECT_TRUE(IsWindowAbove(owned->hwnd(), owner->hwnd()));
  EXPECT_TRUE(IsWindowAbove(owner->hwnd(), other->hwnd()));

  other->CloseNow();
  owned->CloseNow();
  owner->CloseNow();
}

}  // namespace views
