// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>

#include "base/containers/contains.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/mock_input_method.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/win/hwnd_message_handler.h"
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
  void HandleAccelerator(const ui::Accelerator& accelerator) override {}
  void HandleCreate() override {}
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
  void HandleWindowSizeChanging() override {}
  void HandleWindowSizeUnchanged() override {}
  void HandleWindowScaleFactorChanged(float window_scale_factor) override {}
  void HandleHeadlessWindowBoundsChanged(const gfx::Rect& bounds) override {}
  HBRUSH GetBackgroundPaintBrush() override { return nullptr; }

 private:
  std::unique_ptr<ui::MockInputMethod> mock_input_method_;
};

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
  EXPECT_TRUE(base::Contains(owned_windows, child_handler->hwnd()));

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
  EXPECT_TRUE(base::Contains(owned_windows, parent1_handler->hwnd()));
  EXPECT_TRUE(base::Contains(owned_windows, parent2_handler->hwnd()));
  EXPECT_TRUE(base::Contains(owned_windows, child_handler->hwnd()));

  child_handler->CloseNow();
  parent2_handler->CloseNow();
  parent1_handler->CloseNow();
  grandparent_handler->CloseNow();
}

}  // namespace views
