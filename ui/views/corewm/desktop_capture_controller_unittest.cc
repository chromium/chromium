// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/capture_controller.h"

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/native_widget_factory.h"
#include "ui/views/test/views_interactive_ui_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/desktop_screen_position_client.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

// NOTE: these tests do native capture, so they have to be in
// interactive_ui_tests.

namespace views {

using DesktopCaptureControllerTest = ViewsInteractiveUITestBase;

// This class provides functionality to verify whether the View instance
// received the gesture event.
class DesktopViewInputTest : public View {
 public:
  DesktopViewInputTest() = default;

  void OnGestureEvent(ui::GestureEvent* event) override {
    received_gesture_event_ = true;
    return View::OnGestureEvent(event);
  }

  // Resets state maintained by this class.
  void Reset() {
    received_gesture_event_ = false;
  }

  bool received_gesture_event() const { return received_gesture_event_; }

 private:
  bool received_gesture_event_ = false;

  DISALLOW_COPY_AND_ASSIGN(DesktopViewInputTest);
};

views::Widget* CreateWidget() {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.accept_events = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.native_widget = new DesktopNativeWidgetAura(widget);
  params.bounds = gfx::Rect(0, 0, 200, 100);
  widget->Init(std::move(params));
  widget->Show();
  return widget;
}

// Verifies mouse handlers are reset when a window gains capture. Specifically
// creates two widgets, does a mouse press in one, sets capture in the other and
// verifies state is reset in the first.
TEST_F(DesktopCaptureControllerTest, ResetMouseHandlers) {
  std::unique_ptr<Widget> w1(CreateWidget());
  std::unique_ptr<Widget> w2(CreateWidget());
  ui::test::EventGenerator generator1(w1->GetNativeView()->GetRootWindow());
  generator1.MoveMouseToCenterOf(w1->GetNativeView());
  generator1.PressLeftButton();
  EXPECT_FALSE(w1->HasCapture());
  aura::WindowEventDispatcher* w1_dispatcher =
      w1->GetNativeView()->GetHost()->dispatcher();
  EXPECT_TRUE(w1_dispatcher->mouse_pressed_handler() != nullptr);
  EXPECT_TRUE(w1_dispatcher->mouse_moved_handler() != nullptr);
  w2->SetCapture(w2->GetRootView());
  EXPECT_TRUE(w2->HasCapture());
  EXPECT_TRUE(w1_dispatcher->mouse_pressed_handler() == nullptr);
  EXPECT_TRUE(w1_dispatcher->mouse_moved_handler() == nullptr);
  w2->ReleaseCapture();
  RunPendingMessages();
}

// Tests aura::Window capture and whether gesture events are sent to the window
// which has capture.
// The test case creates two visible widgets and sets capture to the underlying
// aura::Windows one by one. It then sends a gesture event and validates whether
// the window which had capture receives the gesture.
// TODO(sky): move this test, it should be part of ScopedCaptureClient tests.
TEST_F(DesktopCaptureControllerTest, CaptureWindowInputEventTest) {
  std::unique_ptr<aura::client::ScreenPositionClient> desktop_position_client1;
  std::unique_ptr<aura::client::ScreenPositionClient> desktop_position_client2;

  std::unique_ptr<Widget> widget1(new Widget());
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  std::unique_ptr<wm::ScopedCaptureClient> scoped_capture_client(
      new wm::ScopedCaptureClient(params.context->GetRootWindow()));
  aura::client::CaptureClient* capture_client = wm::CaptureController::Get();
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(50, 50, 650, 650);
  params.native_widget = test::CreatePlatformNativeWidgetImpl(
      params, widget1.get(), test::kStubCapture, nullptr);
  widget1->Init(std::move(params));
  internal::RootView* root1 =
      static_cast<internal::RootView*>(widget1->GetRootView());

  desktop_position_client1 = std::make_unique<DesktopScreenPositionClient>(
      params.context->GetRootWindow());
  aura::client::SetScreenPositionClient(
      widget1->GetNativeView()->GetRootWindow(),
      desktop_position_client1.get());

  DesktopViewInputTest* v1 = new DesktopViewInputTest();
  v1->SetBoundsRect(gfx::Rect(0, 0, 300, 300));
  root1->AddChildView(v1);
  widget1->Show();

  std::unique_ptr<Widget> widget2(new Widget());

  params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(50, 50, 650, 650);
  params.native_widget = test::CreatePlatformNativeWidgetImpl(
      params, widget2.get(), test::kStubCapture, nullptr);
  widget2->Init(std::move(params));

  internal::RootView* root2 =
      static_cast<internal::RootView*>(widget2->GetRootView());
  desktop_position_client2 = std::make_unique<DesktopScreenPositionClient>(
      params.context->GetRootWindow());
  aura::client::SetScreenPositionClient(
      widget2->GetNativeView()->GetRootWindow(),
      desktop_position_client2.get());
  ui::EventDispatchDetails details;

  DesktopViewInputTest* v2 = new DesktopViewInputTest();
  v2->SetBoundsRect(gfx::Rect(0, 0, 300, 300));
  root2->AddChildView(v2);
  widget2->Show();

  EXPECT_FALSE(widget1->GetNativeView()->HasCapture());
  EXPECT_FALSE(widget2->GetNativeView()->HasCapture());
  EXPECT_EQ(nullptr, capture_client->GetCaptureWindow());

  widget1->GetNativeView()->SetCapture();
  EXPECT_TRUE(widget1->GetNativeView()->HasCapture());
  EXPECT_FALSE(widget2->GetNativeView()->HasCapture());
  EXPECT_EQ(capture_client->GetCaptureWindow(), widget1->GetNativeView());

  ui::GestureEvent g1(80, 80, 0, base::TimeTicks(),
                      ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  details = root1->OnEventFromSource(&g1);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_FALSE(details.target_destroyed);

  EXPECT_TRUE(v1->received_gesture_event());
  EXPECT_FALSE(v2->received_gesture_event());
  v1->Reset();
  v2->Reset();

  widget2->GetNativeView()->SetCapture();

  EXPECT_FALSE(widget1->GetNativeView()->HasCapture());
  EXPECT_TRUE(widget2->GetNativeView()->HasCapture());
  EXPECT_EQ(capture_client->GetCaptureWindow(), widget2->GetNativeView());

  details = root2->OnEventFromSource(&g1);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_FALSE(details.target_destroyed);

  EXPECT_TRUE(v2->received_gesture_event());
  EXPECT_FALSE(v1->received_gesture_event());

  widget1->CloseNow();
  widget2->CloseNow();
  RunPendingMessages();
}

}  // namespace views
