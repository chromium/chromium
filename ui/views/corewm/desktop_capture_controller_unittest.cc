// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ui/aura/env.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/native_widget_factory.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/desktop_screen_position_client.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/capture_controller.h"

// NOTE: these tests do native capture, so they have to be in
// interactive_ui_tests.

namespace views {

using DesktopCaptureControllerTest = test::DesktopWidgetTestInteractive;

// This class provides functionality to verify whether the View instance
// received the gesture event.
class DesktopViewInputTest : public View {
  METADATA_HEADER(DesktopViewInputTest, View)

 public:
  DesktopViewInputTest() = default;

  DesktopViewInputTest(const DesktopViewInputTest&) = delete;
  DesktopViewInputTest& operator=(const DesktopViewInputTest&) = delete;

  void OnGestureEvent(ui::GestureEvent* event) override {
    received_gesture_event_ = true;
    return View::OnGestureEvent(event);
  }

  // Resets state maintained by this class.
  void Reset() { received_gesture_event_ = false; }

  bool received_gesture_event() const { return received_gesture_event_; }

 private:
  bool received_gesture_event_ = false;
};

BEGIN_METADATA(DesktopViewInputTest)
END_METADATA

std::unique_ptr<views::Widget> CreateWidget() {
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.accept_events = true;
  params.native_widget = new DesktopNativeWidgetAura(widget.get());
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

  auto widget1 = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  std::unique_ptr<wm::ScopedCaptureClient> scoped_capture_client(
      new wm::ScopedCaptureClient(params.context->GetRootWindow()));
  aura::client::CaptureClient* capture_client = wm::CaptureController::Get();
  params.bounds = gfx::Rect(50, 50, 650, 650);
  params.native_widget = test::CreatePlatformNativeWidgetImpl(
      widget1.get(), test::kStubCapture, nullptr);
  desktop_position_client1 = std::make_unique<DesktopScreenPositionClient>(
      params.context->GetRootWindow());
  widget1->Init(std::move(params));
  internal::RootView* root1 =
      static_cast<internal::RootView*>(widget1->GetRootView());
  aura::client::SetScreenPositionClient(
      widget1->GetNativeView()->GetRootWindow(),
      desktop_position_client1.get());

  DesktopViewInputTest* v1 = new DesktopViewInputTest();
  v1->SetBoundsRect(gfx::Rect(0, 0, 300, 300));
  root1->AddChildView(v1);
  widget1->Show();

  auto widget2 = std::make_unique<Widget>();

  params = CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                        Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(50, 50, 650, 650);
  params.native_widget = test::CreatePlatformNativeWidgetImpl(
      widget2.get(), test::kStubCapture, nullptr);
  desktop_position_client2 = std::make_unique<DesktopScreenPositionClient>(
      params.context->GetRootWindow());
  widget2->Init(std::move(params));
  internal::RootView* root2 =
      static_cast<internal::RootView*>(widget2->GetRootView());
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

  ui::GestureEvent g1(
      80, 80, 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureLongPress));
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
