// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/hwnd_message_handler.h"

#include <memory>

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/mock_input_method.h"
#include "ui/display/screen.h"
#include "ui/display/test/test_screen.h"
#include "ui/views/test/test_views_delegate.h"
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

class HWNDMessageHandlerTest : public ::testing::Test {
 public:
  // testing::Test:
  void SetUp() override {
    Test::SetUp();
    display::Screen::SetScreenInstance(&test_screen_);
  }
  void TearDown() override {
    display::Screen::SetScreenInstance(nullptr);
    Test::TearDown();
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  TestViewsDelegate test_views_delegate_;
  display::test::TestScreen test_screen_;
};

TEST_F(HWNDMessageHandlerTest, Init) {
  TestHWNDMessageHandlerDelegate delegate;
  std::unique_ptr<HWNDMessageHandler> handler(
      HWNDMessageHandler::Create(&delegate, "test"));
  ASSERT_TRUE(handler);
  handler->Init(nullptr, gfx::Rect(0, 0, 100, 100));
  ASSERT_TRUE(handler->hwnd());
}

}  // namespace views
