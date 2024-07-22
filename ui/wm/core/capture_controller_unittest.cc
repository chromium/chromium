// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/capture_controller.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "ui/aura/client/capture_delegate.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tracker.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/wm/core/capture_controller.h"

namespace wm {

namespace {

// aura::client::CaptureDelegate which allows querying whether native capture
// has been acquired.
class TestCaptureDelegate : public aura::client::CaptureDelegate {
 public:
  TestCaptureDelegate() = default;

  TestCaptureDelegate(const TestCaptureDelegate&) = delete;
  TestCaptureDelegate& operator=(const TestCaptureDelegate&) = delete;

  ~TestCaptureDelegate() override = default;

  bool HasNativeCapture() const { return has_capture_; }
  aura::Window* old_capture() { return old_capture_; }
  aura::Window* new_capture() { return new_capture_; }
  void SetDestroyOldCapture(bool destroy) { destroy_old_capture_ = destroy; }

  // aura::client::CaptureDelegate:
  void UpdateCapture(aura::Window* old_capture,
                     aura::Window* new_capture) override {
    old_capture_ = old_capture;
    new_capture_ = new_capture;
    if (old_capture && destroy_old_capture_)
      delete old_capture;
  }
  void OnOtherRootGotCapture() override {}
  void SetNativeCapture() override { has_capture_ = true; }
  void ReleaseNativeCapture() override { has_capture_ = false; }

 private:
  bool has_capture_ = false;
  raw_ptr<aura::Window, DanglingUntriaged> old_capture_ = nullptr;
  raw_ptr<aura::Window, DanglingUntriaged> new_capture_ = nullptr;
  bool destroy_old_capture_ = false;
};

}  // namespace

class CaptureControllerTest : public aura::test::AuraTestBase {
 public:
  CaptureControllerTest() {}

  CaptureControllerTest(const CaptureControllerTest&) = delete;
  CaptureControllerTest& operator=(const CaptureControllerTest&) = delete;

  void SetUp() override {
    AuraTestBase::SetUp();
    capture_client_ = std::make_unique<ScopedCaptureClient>(root_window());

    second_host_ = aura::WindowTreeHost::Create(
        ui::PlatformWindowInitProperties{gfx::Rect(0, 0, 800, 600)});
    second_host_->InitHost();
    second_host_->window()->Show();
    second_host_->SetBoundsInPixels(gfx::Rect(800, 600));
    second_capture_client_ =
        std::make_unique<ScopedCaptureClient>(second_host_->window());
  }

  void TearDown() override {
    RunAllPendingInMessageLoop();

    second_capture_client_.reset();

    // Kill any active compositors before we hit the compositor shutdown paths.
    second_host_.reset();

    capture_client_.reset();

    AuraTestBase::TearDown();
  }

  aura::Window* CreateNormalWindowWithBounds(int id,
                                             aura::Window* parent,
                                             const gfx::Rect& bounds,
                                             aura::WindowDelegate* delegate) {
    aura::Window* window = new aura::Window(
        delegate
            ? delegate
            : aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate());
    window->SetId(id);
    window->Init(ui::LAYER_TEXTURED);
    parent->AddChild(window);
    window->SetBounds(bounds);
    window->Show();
    return window;
  }

  aura::Window* GetCaptureWindow() {
    return CaptureController::Get()->GetCaptureWindow();
  }

  std::unique_ptr<ScopedCaptureClient> capture_client_;
  std::unique_ptr<aura::WindowTreeHost> second_host_;
  std::unique_ptr<ScopedCaptureClient> second_capture_client_;
};

// Makes sure that internal details that are set on mouse down (such as
// mouse_pressed_handler()) are cleared when another root window takes capture.
TEST_F(CaptureControllerTest, ResetMouseEventHandlerOnCapture) {
  // Create a window inside the WindowEventDispatcher.
  std::unique_ptr<aura::Window> w1(
      CreateNormalWindow(1, root_window(), nullptr));

  // Make a synthesized mouse down event. Ensure that the WindowEventDispatcher
  // will dispatch further mouse events to |w1|.
  ui::MouseEvent mouse_pressed_event(ui::EventType::kMousePressed,
                                     gfx::Point(5, 5), gfx::Point(5, 5),
                                     ui::EventTimeForNow(), 0, 0);
  DispatchEventUsingWindowDispatcher(&mouse_pressed_event);
  EXPECT_EQ(w1.get(), host()->dispatcher()->mouse_pressed_handler());

  // Build a window in the second WindowEventDispatcher.
  std::unique_ptr<aura::Window> w2(
      CreateNormalWindow(2, second_host_->window(), nullptr));

  // The act of having the second window take capture should clear out mouse
  // pressed handler in the first WindowEventDispatcher.
  w2->SetCapture();
  EXPECT_EQ(nullptr, host()->dispatcher()->mouse_pressed_handler());
}

// Makes sure that when one window gets capture, it forces the release on the
// other. This is needed has to be handled explicitly on Linux, and is a sanity
// check on Windows.
TEST_F(CaptureControllerTest, ResetOtherWindowCaptureOnCapture) {
  // Create a window inside the WindowEventDispatcher.
  std::unique_ptr<aura::Window> w1(
      CreateNormalWindow(1, root_window(), nullptr));
  w1->SetCapture();
  EXPECT_EQ(w1.get(), GetCaptureWindow());

  // Build a window in the second WindowEventDispatcher and give it capture.
  std::unique_ptr<aura::Window> w2(
      CreateNormalWindow(2, second_host_->window(), nullptr));
  w2->SetCapture();
  EXPECT_EQ(w2.get(), GetCaptureWindow());
}

// Verifies the touch target for the WindowEventDispatcher gets reset on
// releasing capture.
TEST_F(CaptureControllerTest, TouchTargetResetOnCaptureChange) {
  // Create a window inside the WindowEventDispatcher.
  std::unique_ptr<aura::Window> w1(
      CreateNormalWindow(1, root_window(), nullptr));
  ui::test::EventGenerator event_generator1(root_window());
  event_generator1.PressTouch();
  w1->SetCapture();
  EXPECT_EQ(w1.get(), GetCaptureWindow());

  // Build a window in the second WindowEventDispatcher and give it capture.
  std::unique_ptr<aura::Window> w2(
      CreateNormalWindow(2, second_host_->window(), nullptr));
  w2->SetCapture();
  EXPECT_EQ(w2.get(), GetCaptureWindow());

  // Release capture on the window. Releasing capture should reset the touch
  // target of the first WindowEventDispatcher (as it no longer contains the
  // capture target).
  w2->ReleaseCapture();
  EXPECT_EQ(nullptr, GetCaptureWindow());
}

// Test that native capture is released properly when the window with capture
// is reparented to a different root window while it has capture.
TEST_F(CaptureControllerTest, ReparentedWhileCaptured) {
  std::unique_ptr<TestCaptureDelegate> delegate(new TestCaptureDelegate);
  ScopedCaptureClient::TestApi(capture_client_.get())
      .SetDelegate(delegate.get());
  std::unique_ptr<TestCaptureDelegate> delegate2(new TestCaptureDelegate);
  ScopedCaptureClient::TestApi(second_capture_client_.get())
      .SetDelegate(delegate2.get());

  std::unique_ptr<aura::Window> w(
      CreateNormalWindow(1, root_window(), nullptr));
  w->SetCapture();
  EXPECT_EQ(w.get(), GetCaptureWindow());
  EXPECT_TRUE(delegate->HasNativeCapture());
  EXPECT_FALSE(delegate2->HasNativeCapture());

  second_host_->window()->AddChild(w.get());
  EXPECT_EQ(w.get(), GetCaptureWindow());
  EXPECT_TRUE(delegate->HasNativeCapture());
  EXPECT_FALSE(delegate2->HasNativeCapture());

  w->ReleaseCapture();
  EXPECT_EQ(nullptr, GetCaptureWindow());
  EXPECT_FALSE(delegate->HasNativeCapture());
  EXPECT_FALSE(delegate2->HasNativeCapture());
}

// Test that a call to `PrepareForShutdown()` releases capture from capture
// window and also prevents capture window from being set afterwards.
TEST_F(CaptureControllerTest, PrepareForShutdown) {
  aura::Window* w = CreateNormalWindow(1, root_window(), nullptr);
  w->SetCapture();
  EXPECT_EQ(CaptureController::Get()->GetCaptureWindow(), w);

  CaptureController::Get()->PrepareForShutdown();
  EXPECT_EQ(CaptureController::Get()->GetCaptureWindow(), nullptr);

  w->SetCapture();
  // Once `PrepareForShutdown()` is called, capture window cannot be set.
  EXPECT_EQ(CaptureController::Get()->GetCaptureWindow(), nullptr);
}

// A delegate that deletes a window on scroll cancel gesture event.
class GestureEventDeleteWindowOnScrollEnd
    : public aura::test::TestWindowDelegate {
 public:
  GestureEventDeleteWindowOnScrollEnd() {}

  GestureEventDeleteWindowOnScrollEnd(
      const GestureEventDeleteWindowOnScrollEnd&) = delete;
  GestureEventDeleteWindowOnScrollEnd& operator=(
      const GestureEventDeleteWindowOnScrollEnd&) = delete;

  void SetWindow(std::unique_ptr<aura::Window> window) {
    window_ = std::move(window);
  }
  aura::Window* window() { return window_.get(); }

  // aura::test::TestWindowDelegate:
  void OnGestureEvent(ui::GestureEvent* gesture) override {
    TestWindowDelegate::OnGestureEvent(gesture);
    if (gesture->type() != ui::EventType::kGestureScrollEnd) {
      return;
    }
    window_.reset();
  }

 private:
  std::unique_ptr<aura::Window> window_;
};

// Tests a scenario when a window gets deleted while a capture is being set on
// it and when that window releases its capture prior to being deleted.
// This scenario should end safely without capture being set.
TEST_F(CaptureControllerTest, GestureResetWithCapture) {
  std::unique_ptr<GestureEventDeleteWindowOnScrollEnd> delegate(
      new GestureEventDeleteWindowOnScrollEnd());
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window1(
      CreateNormalWindowWithBounds(-1235, root_window(), bounds, nullptr));

  bounds.Offset(0, 100);
  std::unique_ptr<aura::Window> window2(CreateNormalWindowWithBounds(
      -1234, root_window(), bounds, delegate.get()));
  delegate->SetWindow(std::move(window1));

  ui::test::EventGenerator event_generator(root_window());
  const int position_x = bounds.x() + 1;
  int position_y = bounds.y() + 1;
  event_generator.MoveTouch(gfx::Point(position_x, position_y));
  event_generator.PressTouch();
  for (int idx = 0 ; idx < 20 ; idx++, position_y++)
    event_generator.MoveTouch(gfx::Point(position_x, position_y));

  // Setting capture on |window1| cancels touch gestures that are active on
  // |window2|. GestureEventDeleteWindowOnScrollEnd will then delete |window1|
  // and should release capture on it.
  delegate->window()->SetCapture();

  // capture should not be set upon exit from SetCapture() above.
  aura::client::CaptureClient* capture_client =
      aura::client::GetCaptureClient(root_window());
  ASSERT_NE(nullptr, capture_client);
  EXPECT_EQ(nullptr, capture_client->GetCaptureWindow());

  // Send a mouse click. We no longer hold capture so this should not crash.
  ui::MouseEvent mouse_press(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), base::TimeTicks(), 0, 0);
  DispatchEventUsingWindowDispatcher(&mouse_press);
}

TEST_F(CaptureControllerTest, UpdateCaptureDestroysOldCaptureWindow) {
  TestCaptureDelegate delegate;
  ScopedCaptureClient::TestApi(capture_client_.get()).SetDelegate(&delegate);
  TestCaptureDelegate delegate2;
  ScopedCaptureClient::TestApi(second_capture_client_.get())
      .SetDelegate(&delegate2);

  // Since delegate iteration order is not deterministic, use this to assert
  // that the two scenarios below have opposite order.
  aura::Window* first_old_capture = nullptr;

  {
    // Create a window inside the WindowEventDispatcher.
    std::unique_ptr<aura::Window> capture_window(
        CreateNormalWindow(1, root_window(), nullptr));
    ui::test::EventGenerator event_generator(root_window());
    event_generator.PressTouch();
    capture_window->SetCapture();

    delegate.SetDestroyOldCapture(true);
    delegate2.SetDestroyOldCapture(false);

    aura::WindowTracker tracker({capture_window.get()});
    CaptureController::Get()->SetCapture(nullptr);
    EXPECT_EQ(delegate.old_capture(), capture_window.get());
    first_old_capture = delegate2.old_capture();
    EXPECT_FALSE(tracker.Contains(capture_window.get()));
    if (!tracker.Contains(capture_window.get()))
      capture_window.release();
  }

  {
    // Create a window inside the WindowEventDispatcher.
    std::unique_ptr<aura::Window> capture_window(
        CreateNormalWindow(1, root_window(), nullptr));
    ui::test::EventGenerator event_generator(root_window());
    event_generator.PressTouch();
    capture_window->SetCapture();

    // Change order to account for map traversal order.
    delegate.SetDestroyOldCapture(false);
    delegate2.SetDestroyOldCapture(true);

    aura::WindowTracker tracker({capture_window.get()});
    CaptureController::Get()->SetCapture(nullptr);
    EXPECT_NE(delegate.old_capture(), first_old_capture);
    EXPECT_EQ(delegate2.old_capture(), capture_window.get());
    EXPECT_FALSE(tracker.Contains(capture_window.get()));
    if (!tracker.Contains(capture_window.get()))
      capture_window.release();
  }
}

}  // namespace wm
