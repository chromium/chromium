// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_event_source.h"

#include <linux/input.h>

#include <algorithm>

#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/common/features.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/test/mock_pointer.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_keyboard.h"
#include "ui/ozone/platform/wayland/test/test_touch.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"

using ::testing::_;
using ::testing::Values;

namespace ui {

namespace {

constexpr gfx::Rect kDefaultBounds(0, 0, 100, 100);

namespace {

struct FeatureState {
  bool dispatch_mouse_events_on_frame_event = false;
  bool dispatch_touch_events_on_frame_event = false;
};

}  // namespace

}  // namespace

class WaylandEventSourceTest
    : public ::testing::WithParamInterface<FeatureState>,
      public WaylandTestSimple {
 public:
  void SetUp() override {
    CHECK(!std::ranges::contains(
        enabled_features_,
        base::test::FeatureRef(kDispatchPointerEventsOnFrameEvent)));
    CHECK(!std::ranges::contains(
        disabled_features_,
        base::test::FeatureRef(kDispatchPointerEventsOnFrameEvent)));
    if (GetParam().dispatch_mouse_events_on_frame_event) {
      enabled_features_.push_back(kDispatchPointerEventsOnFrameEvent);
    } else {
      disabled_features_.push_back(kDispatchPointerEventsOnFrameEvent);
    }

    if (GetParam().dispatch_touch_events_on_frame_event) {
      enabled_features_.push_back(kDispatchTouchEventsOnFrameEvent);
    } else {
      disabled_features_.push_back(kDispatchTouchEventsOnFrameEvent);
    }

    WaylandTestSimple::SetUp();

    pointer_delegate_ = connection_->event_source();
    ASSERT_TRUE(pointer_delegate_);
  }

  void TearDown() override {
    if (GetParam().dispatch_touch_events_on_frame_event) {
      CHECK(enabled_features_.back() == kDispatchTouchEventsOnFrameEvent);
      enabled_features_.pop_back();
    } else {
      CHECK(disabled_features_.back() == kDispatchTouchEventsOnFrameEvent);
      disabled_features_.pop_back();
    }

    if (GetParam().dispatch_mouse_events_on_frame_event) {
      CHECK(enabled_features_.back() == kDispatchPointerEventsOnFrameEvent);
      enabled_features_.pop_back();
    } else {
      CHECK(disabled_features_.back() == kDispatchPointerEventsOnFrameEvent);
      disabled_features_.pop_back();
    }
  }

 protected:
  base::test::ScopedFeatureList features_;
  raw_ptr<WaylandPointer::Delegate> pointer_delegate_ = nullptr;
};

// Verify WaylandEventSource properly manages its internal state as pointer
// button events are sent. More specifically - pointer flags.
TEST_P(WaylandEventSourceTest, CheckPointerButtonHandling) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_seat_send_capabilities(server->seat()->resource(),
                              WL_SEAT_CAPABILITY_POINTER);
  });
  ASSERT_TRUE(connection_->seat()->pointer());

  EXPECT_FALSE(pointer_delegate_->IsPointerButtonPressed(EF_LEFT_MOUSE_BUTTON));
  EXPECT_FALSE(
      pointer_delegate_->IsPointerButtonPressed(EF_RIGHT_MOUSE_BUTTON));
  EXPECT_FALSE(
      pointer_delegate_->IsPointerButtonPressed(EF_MIDDLE_MOUSE_BUTTON));
  EXPECT_FALSE(pointer_delegate_->IsPointerButtonPressed(EF_BACK_MOUSE_BUTTON));
  EXPECT_FALSE(
      pointer_delegate_->IsPointerButtonPressed(EF_FORWARD_MOUSE_BUTTON));

  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(2);

  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();
    auto* const pointer = server->seat()->pointer()->resource();

    wl_pointer_send_enter(pointer, server->GetNextSerial(), surface, 0, 0);
    wl_pointer_send_frame(pointer);
    wl_pointer_send_button(pointer, server->GetNextSerial(),
                           server->GetNextTime(), BTN_LEFT,
                           WL_POINTER_BUTTON_STATE_PRESSED);
    wl_pointer_send_frame(pointer);
  });

  EXPECT_TRUE(pointer_delegate_->IsPointerButtonPressed(EF_LEFT_MOUSE_BUTTON));

  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(1);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const pointer = server->seat()->pointer()->resource();

    wl_pointer_send_button(pointer, server->GetNextSerial(),
                           server->GetNextTime(), BTN_RIGHT,
                           WL_POINTER_BUTTON_STATE_PRESSED);
    wl_pointer_send_frame(pointer);
  });

  EXPECT_TRUE(pointer_delegate_->IsPointerButtonPressed(EF_RIGHT_MOUSE_BUTTON));

  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(2);
  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const pointer = server->seat()->pointer()->resource();

    wl_pointer_send_button(pointer, server->GetNextSerial(),
                           server->GetNextTime(), BTN_LEFT,
                           WL_POINTER_BUTTON_STATE_RELEASED);
    wl_pointer_send_frame(pointer);
    wl_pointer_send_button(pointer, server->GetNextSerial(),
                           server->GetNextTime(), BTN_RIGHT,
                           WL_POINTER_BUTTON_STATE_RELEASED);
    wl_pointer_send_frame(pointer);
  });

  EXPECT_FALSE(pointer_delegate_->IsPointerButtonPressed(EF_LEFT_MOUSE_BUTTON));
  EXPECT_FALSE(
      pointer_delegate_->IsPointerButtonPressed(EF_RIGHT_MOUSE_BUTTON));
}

// Verify WaylandEventSource properly manages its internal state as pointer
// button events are sent. More specifically - pointer flags.
TEST_P(WaylandEventSourceTest, DeleteBeforeTouchFrame) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_seat_send_capabilities(server->seat()->resource(),
                              WL_SEAT_CAPABILITY_TOUCH);
  });
  ASSERT_TRUE(connection_->seat()->touch());

  MockWaylandPlatformWindowDelegate delegate(connection_.get());
  auto window1 = CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                               kDefaultBounds, &delegate);

  PostToServerAndWait([surface_id = window1->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();
    auto* const touch = server->seat()->touch()->resource();

    wl_touch_send_down(touch, server->GetNextSerial(), server->GetNextTime(),
                       surface, /*id=*/0, 0, 0);
    wl_touch_send_down(touch, server->GetNextSerial(), server->GetNextTime(),
                       surface, /*id=*/1, 0, 0);
  });

  // Removing the target during touch event sequence should not cause crash.
  window1.reset();

  EXPECT_CALL(delegate, DispatchEvent(_)).Times(0);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const touch = server->seat()->touch()->resource();

    wl_touch_send_frame(touch);
  });
}

// Verify WaylandEventSource ignores release events for mouse buttons that
// aren't pressed. Regression test for crbug.com/1376393.
TEST_P(WaylandEventSourceTest, IgnoreReleaseWithoutPress) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_seat_send_capabilities(server->seat()->resource(),
                              WL_SEAT_CAPABILITY_POINTER);
  });
  ASSERT_TRUE(connection_->seat()->pointer());

  // The only event the delegate should capture is when the pointer enters the
  // surface.
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(1);
  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();
    auto* const pointer = server->seat()->pointer()->resource();

    wl_pointer_send_enter(pointer, server->GetNextSerial(), surface, 0, 0);
    wl_pointer_send_frame(pointer);

    wl_pointer_send_button(pointer, server->GetNextSerial(),
                           server->GetNextTime(), BTN_LEFT,
                           WL_POINTER_BUTTON_STATE_RELEASED);
    wl_pointer_send_frame(pointer);
  });
}

TEST_P(WaylandEventSourceTest, ReleasesAllPressedPointerButtons) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_seat_send_capabilities(server->seat()->resource(),
                              WL_SEAT_CAPABILITY_POINTER);
  });
  ASSERT_TRUE(connection_->seat()->pointer());

  EXPECT_FALSE(pointer_delegate_->IsPointerButtonPressed(EF_LEFT_MOUSE_BUTTON));
  EXPECT_FALSE(
      pointer_delegate_->IsPointerButtonPressed(EF_RIGHT_MOUSE_BUTTON));
  EXPECT_FALSE(
      pointer_delegate_->IsPointerButtonPressed(EF_MIDDLE_MOUSE_BUTTON));

  // Dispatch enter, left, right and middle press pointer events.
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(4);
  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();
    auto* const pointer = server->seat()->pointer()->resource();

    wl_pointer_send_enter(pointer, server->GetNextSerial(), surface, 0, 0);
    wl_pointer_send_frame(pointer);
    wl_pointer_send_button(pointer, server->GetNextSerial(),
                           server->GetNextTime(), BTN_LEFT,
                           WL_POINTER_BUTTON_STATE_PRESSED);
    wl_pointer_send_frame(pointer);
    wl_pointer_send_button(pointer, server->GetNextSerial(),
                           server->GetNextTime(), BTN_RIGHT,
                           WL_POINTER_BUTTON_STATE_PRESSED);
    wl_pointer_send_frame(pointer);
    wl_pointer_send_button(pointer, server->GetNextSerial(),
                           server->GetNextTime(), BTN_MIDDLE,
                           WL_POINTER_BUTTON_STATE_PRESSED);
    wl_pointer_send_frame(pointer);
  });

  // Left, right and middle mouse buttons should register as pressed.
  EXPECT_TRUE(pointer_delegate_->IsPointerButtonPressed(EF_LEFT_MOUSE_BUTTON));
  EXPECT_TRUE(pointer_delegate_->IsPointerButtonPressed(EF_RIGHT_MOUSE_BUTTON));
  EXPECT_TRUE(
      pointer_delegate_->IsPointerButtonPressed(EF_MIDDLE_MOUSE_BUTTON));

  // Verify release buttons are synthesized for mouse pressed events.
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(3);
  pointer_delegate_->ReleasePressedPointerButtons(window_.get(),
                                                  base::TimeTicks::Now());

  EXPECT_FALSE(pointer_delegate_->IsPointerButtonPressed(EF_LEFT_MOUSE_BUTTON));
  EXPECT_FALSE(
      pointer_delegate_->IsPointerButtonPressed(EF_RIGHT_MOUSE_BUTTON));
  EXPECT_FALSE(
      pointer_delegate_->IsPointerButtonPressed(EF_MIDDLE_MOUSE_BUTTON));
}

// Verify that pressed buttons are released when pointer focus is lost and not
// regained within the same frame (e.g. focus goes to the compositor's own UI).
TEST_P(WaylandEventSourceTest, SyntheticReleaseOnFocusLoss) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_seat_send_capabilities(server->seat()->resource(),
                              WL_SEAT_CAPABILITY_POINTER);
  });
  ASSERT_TRUE(connection_->seat()->pointer());

  // Enter and press the right mouse button.
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(2);
  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();
    auto* const pointer = server->seat()->pointer()->resource();

    wl_pointer_send_enter(pointer, server->GetNextSerial(), surface, 0, 0);
    wl_pointer_send_frame(pointer);
    wl_pointer_send_button(pointer, server->GetNextSerial(),
                           server->GetNextTime(), BTN_RIGHT,
                           WL_POINTER_BUTTON_STATE_PRESSED);
    wl_pointer_send_frame(pointer);
  });

  EXPECT_TRUE(pointer_delegate_->IsPointerButtonPressed(EF_RIGHT_MOUSE_BUTTON));

  // Leave with no subsequent enter in the same frame — the button should be
  // synthetically released at the frame event.
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(testing::AtLeast(1));
  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();
    auto* const pointer = server->seat()->pointer()->resource();

    wl_pointer_send_leave(pointer, server->GetNextSerial(), surface);
    wl_pointer_send_frame(pointer);
  });

  EXPECT_FALSE(
      pointer_delegate_->IsPointerButtonPressed(EF_RIGHT_MOUSE_BUTTON));
}

// Verify that pressed buttons are NOT synthetically released when pointer focus
// leaves and enters within the same frame.
// Regression test for https://crbug.com/500653052.
TEST_P(WaylandEventSourceTest, NoSyntheticReleaseOnIntraClientFocusChange) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_seat_send_capabilities(server->seat()->resource(),
                              WL_SEAT_CAPABILITY_POINTER);
  });
  ASSERT_TRUE(connection_->seat()->pointer());

  // Enter and press the right mouse button.
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(2);
  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();
    auto* const pointer = server->seat()->pointer()->resource();

    wl_pointer_send_enter(pointer, server->GetNextSerial(), surface, 0, 0);
    wl_pointer_send_frame(pointer);
    wl_pointer_send_button(pointer, server->GetNextSerial(),
                           server->GetNextTime(), BTN_RIGHT,
                           WL_POINTER_BUTTON_STATE_PRESSED);
    wl_pointer_send_frame(pointer);
  });

  EXPECT_TRUE(pointer_delegate_->IsPointerButtonPressed(EF_RIGHT_MOUSE_BUTTON));

  // Leave and re-enter (same surface for simplicity) within the same frame.
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(testing::AtLeast(1));
  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();
    auto* const pointer = server->seat()->pointer()->resource();

    wl_pointer_send_leave(pointer, server->GetNextSerial(), surface);
    wl_pointer_send_enter(pointer, server->GetNextSerial(), surface, 0, 0);
    wl_pointer_send_frame(pointer);
  });

  EXPECT_TRUE(pointer_delegate_->IsPointerButtonPressed(EF_RIGHT_MOUSE_BUTTON));
}

// Verify that pressed buttons are NOT synthetically released when pointer focus
// leaves but a window is still capturing the pointer (e.g., a menu popup).
// Regression test for https://crbug.com/500653052.
TEST_P(WaylandEventSourceTest, NoSyntheticReleaseWhileMenuHasCapture) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_seat_send_capabilities(server->seat()->resource(),
                              WL_SEAT_CAPABILITY_POINTER);
  });
  ASSERT_TRUE(connection_->seat()->pointer());

  // Enter and press the right mouse button.
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(2);
  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();
    auto* const pointer = server->seat()->pointer()->resource();

    wl_pointer_send_enter(pointer, server->GetNextSerial(), surface, 0, 0);
    wl_pointer_send_frame(pointer);
    wl_pointer_send_button(pointer, server->GetNextSerial(),
                           server->GetNextTime(), BTN_RIGHT,
                           WL_POINTER_BUTTON_STATE_PRESSED);
    wl_pointer_send_frame(pointer);
  });

  EXPECT_TRUE(pointer_delegate_->IsPointerButtonPressed(EF_RIGHT_MOUSE_BUTTON));

  // A menu opens and takes capture.
  window_->SetCapture();
  ASSERT_TRUE(window_->HasCapture());

  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(testing::AnyNumber());
  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();
    auto* const pointer = server->seat()->pointer()->resource();

    wl_pointer_send_leave(pointer, server->GetNextSerial(), surface);
    wl_pointer_send_frame(pointer);
  });

  EXPECT_TRUE(pointer_delegate_->IsPointerButtonPressed(EF_RIGHT_MOUSE_BUTTON));

  window_->ReleaseCapture();
}

TEST_P(WaylandEventSourceTest, TabletToolProximityInUAF) {
  auto* event_source = connection_->event_source();

  // Create two windows.
  MockWaylandPlatformWindowDelegate delegate1(connection_.get());
  auto window1 = CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                               kDefaultBounds, &delegate1);

  MockWaylandPlatformWindowDelegate delegate2(connection_.get());
  auto window2 = CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                               kDefaultBounds, &delegate2);

  // Set `window1` as focused.
  event_source->OnTabletToolProximityIn(window1.get(), gfx::PointF(), {},
                                        base::TimeTicks::Now());

  // Set up `delegate1` to destroy `window2` when it receives `kMouseExited`.
  // When `window1` is the `tablet_tool_focused_window_`, calling
  // `OnTabletToolProximityIn(window2)` will call `OnTabletToolProximityOut()`,
  // which dispatches `kMouseExited` to `window1`.

  EXPECT_CALL(delegate1, DispatchEvent(::testing::_))
      .WillOnce([&](Event* event) {
        if (event->type() == EventType::kMouseExited) {
          window2.reset();
        }
      });

  // This should not crash.
  event_source->OnTabletToolProximityIn(window2.get(), gfx::PointF(), {},
                                        base::TimeTicks::Now());
}

// A tablet tool borrows pointer focus while in proximity, and must hand it back
// when it leaves. Clearing it instead strands it: the mouse never left the
// surface, so no wl_pointer.enter follows to restore focus, and every
// subsequent mouse event is dropped for lack of a target.
TEST_P(WaylandEventSourceTest, TabletToolProximityOutRestoresPointerFocus) {
  auto* event_source = connection_->event_source();
  auto* window_manager = connection_->window_manager();

  MockWaylandPlatformWindowDelegate delegate(connection_.get());
  auto window = CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                              kDefaultBounds, &delegate);
  EXPECT_CALL(delegate, DispatchEvent(::testing::_))
      .Times(::testing::AnyNumber());

  // The mouse enters the window, as wl_pointer.enter would.
  event_source->OnPointerFocusChanged(window.get(), gfx::PointF(10, 10),
                                      base::TimeTicks::Now(),
                                      wl::EventDispatchPolicy::kImmediate);
  ASSERT_EQ(window.get(), window_manager->GetCurrentPointerFocusedWindow());

  // Stylus tab dragging resolves its drag origin through the pointer focused
  // window, so the tool takes pointer focus while in proximity.
  event_source->OnTabletToolProximityIn(window.get(), gfx::PointF(20, 20), {},
                                        base::TimeTicks::Now());
  EXPECT_EQ(window.get(), window_manager->GetCurrentPointerFocusedWindow());

  event_source->OnTabletToolProximityOut({}, base::TimeTicks::Now());
  EXPECT_EQ(window.get(), window_manager->GetCurrentPointerFocusedWindow());
}

// The exit event dispatched on proximity out must carry the tool's pointer
// type. Defaulting it to a mouse surfaces in Blink as a `pointerType:"mouse"`
// sample with the spec-default `pressure:0.5` at the tail of a pen stroke.
TEST_P(WaylandEventSourceTest, TabletToolProximityOutKeepsPenPointerType) {
  auto* event_source = connection_->event_source();

  MockWaylandPlatformWindowDelegate delegate(connection_.get());
  auto window = CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                              kDefaultBounds, &delegate);

  const PointerDetails pen(EventPointerType::kPen, /*pointer_id=*/2);
  EXPECT_CALL(delegate, DispatchEvent(::testing::_))
      .Times(::testing::AnyNumber());
  event_source->OnTabletToolProximityIn(window.get(), gfx::PointF(20, 20), pen,
                                        base::TimeTicks::Now());
  ::testing::Mock::VerifyAndClearExpectations(&delegate);

  EXPECT_CALL(delegate, DispatchEvent(::testing::_)).WillOnce([](Event* event) {
    ASSERT_TRUE(event->IsMouseEvent());
    auto* mouse_event = event->AsMouseEvent();
    EXPECT_EQ(mouse_event->type(), EventType::kMouseExited);
    EXPECT_EQ(mouse_event->pointer_details().pointer_type,
              EventPointerType::kPen);
  });
  event_source->OnTabletToolProximityOut(pen, base::TimeTicks::Now());
}

// The mirror case: with no mouse in the window, proximity out must not leave
// the tool's window holding pointer focus.
TEST_P(WaylandEventSourceTest,
       TabletToolProximityOutClearsUnownedPointerFocus) {
  auto* event_source = connection_->event_source();
  auto* window_manager = connection_->window_manager();

  MockWaylandPlatformWindowDelegate delegate(connection_.get());
  auto window = CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                              kDefaultBounds, &delegate);
  EXPECT_CALL(delegate, DispatchEvent(::testing::_))
      .Times(::testing::AnyNumber());

  ASSERT_EQ(nullptr, window_manager->GetCurrentPointerFocusedWindow());

  event_source->OnTabletToolProximityIn(window.get(), gfx::PointF(20, 20), {},
                                        base::TimeTicks::Now());
  EXPECT_EQ(window.get(), window_manager->GetCurrentPointerFocusedWindow());

  event_source->OnTabletToolProximityOut({}, base::TimeTicks::Now());
  EXPECT_EQ(nullptr, window_manager->GetCurrentPointerFocusedWindow());
}

// Check that if an event dispatched by ReleasePressedPointerButtons causes the
// target window to be destroyed, we don't cause a UAF or dangling pointer.
TEST_P(WaylandEventSourceTest, ReleasePressedPointerButtonsUAF) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_seat_send_capabilities(server->seat()->resource(),
                              WL_SEAT_CAPABILITY_POINTER);
  });
  ASSERT_TRUE(connection_->seat()->pointer());

  // Record two pressed buttons so ReleasePressedPointerButtons iterates twice.
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(::testing::AnyNumber());
  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();
    auto* const pointer = server->seat()->pointer()->resource();
    wl_pointer_send_enter(pointer, server->GetNextSerial(), surface, 0, 0);
    wl_pointer_send_button(pointer, server->GetNextSerial(),
                           server->GetNextTime(), BTN_LEFT,
                           WL_POINTER_BUTTON_STATE_PRESSED);
    wl_pointer_send_button(pointer, server->GetNextSerial(),
                           server->GetNextTime(), BTN_RIGHT,
                           WL_POINTER_BUTTON_STATE_PRESSED);
    wl_pointer_send_frame(pointer);
  });
  ASSERT_TRUE(pointer_delegate_->IsPointerButtonPressed(EF_LEFT_MOUSE_BUTTON));
  ASSERT_TRUE(pointer_delegate_->IsPointerButtonPressed(EF_RIGHT_MOUSE_BUTTON));

  // Destroy the window on the first mouse release.
  EXPECT_CALL(delegate_, DispatchEvent(_))
      .WillOnce([&](Event* event) {
        EXPECT_EQ(event->type(), EventType::kMouseReleased);
        window_.reset();
      })
      .WillRepeatedly(::testing::Return());

  // Release both pressed buttons.
  pointer_delegate_->ReleasePressedPointerButtons(window_.get(),
                                                  base::TimeTicks::Now());
}

TEST_P(WaylandEventSourceTest, TabletToolButtonEvents) {
  auto* event_source = connection_->event_source();

  MockWaylandPlatformWindowDelegate delegate(connection_.get());
  auto window = CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                              kDefaultBounds, &delegate);

  // Set `window` as focused under tablet proximity.
  event_source->OnTabletToolProximityIn(window.get(), gfx::PointF(100, 100), {},
                                        base::TimeTicks::Now());

  // Test BTN_STYLUS press (which maps to EF_MIDDLE_MOUSE_BUTTON).
  {
    EXPECT_CALL(delegate, DispatchEvent(::testing::_))
        .WillOnce([](Event* event) {
          ASSERT_TRUE(event->IsMouseEvent());
          auto* mouse_event = event->AsMouseEvent();
          EXPECT_EQ(mouse_event->type(), EventType::kMousePressed);
          EXPECT_EQ(mouse_event->changed_button_flags(),
                    EF_MIDDLE_MOUSE_BUTTON);
          EXPECT_EQ(mouse_event->button_flags(), EF_MIDDLE_MOUSE_BUTTON);
        });

    event_source->OnTabletToolButton(EF_MIDDLE_MOUSE_BUTTON, /*pressed=*/true,
                                     {}, base::TimeTicks::Now());
    ::testing::Mock::VerifyAndClearExpectations(&delegate);
  }

  // Test BTN_STYLUS2 press (which maps to EF_RIGHT_MOUSE_BUTTON).
  {
    EXPECT_CALL(delegate, DispatchEvent(::testing::_))
        .WillOnce([](Event* event) {
          ASSERT_TRUE(event->IsMouseEvent());
          auto* mouse_event = event->AsMouseEvent();
          EXPECT_EQ(mouse_event->type(), EventType::kMousePressed);
          EXPECT_EQ(mouse_event->changed_button_flags(), EF_RIGHT_MOUSE_BUTTON);
          EXPECT_EQ(mouse_event->button_flags(),
                    EF_MIDDLE_MOUSE_BUTTON | EF_RIGHT_MOUSE_BUTTON);
        });

    event_source->OnTabletToolButton(EF_RIGHT_MOUSE_BUTTON, /*pressed=*/true,
                                     {}, base::TimeTicks::Now());
    ::testing::Mock::VerifyAndClearExpectations(&delegate);
  }

  // Test BTN_STYLUS release (EF_MIDDLE_MOUSE_BUTTON).
  {
    EXPECT_CALL(delegate, DispatchEvent(::testing::_))
        .WillOnce([](Event* event) {
          ASSERT_TRUE(event->IsMouseEvent());
          auto* mouse_event = event->AsMouseEvent();
          EXPECT_EQ(mouse_event->type(), EventType::kMouseReleased);
          EXPECT_EQ(mouse_event->changed_button_flags(),
                    EF_MIDDLE_MOUSE_BUTTON);
          // Button release flags include the released button per
          // OnTabletToolButton.
          EXPECT_EQ(mouse_event->button_flags(),
                    EF_RIGHT_MOUSE_BUTTON | EF_MIDDLE_MOUSE_BUTTON);
        });

    event_source->OnTabletToolButton(EF_MIDDLE_MOUSE_BUTTON, /*pressed=*/false,
                                     {}, base::TimeTicks::Now());
    ::testing::Mock::VerifyAndClearExpectations(&delegate);
  }

  // Test BTN_STYLUS2 release (EF_RIGHT_MOUSE_BUTTON).
  {
    EXPECT_CALL(delegate, DispatchEvent(::testing::_))
        .WillOnce([](Event* event) {
          ASSERT_TRUE(event->IsMouseEvent());
          auto* mouse_event = event->AsMouseEvent();
          EXPECT_EQ(mouse_event->type(), EventType::kMouseReleased);
          EXPECT_EQ(mouse_event->changed_button_flags(), EF_RIGHT_MOUSE_BUTTON);
          EXPECT_EQ(mouse_event->button_flags(), EF_RIGHT_MOUSE_BUTTON);
        });

    event_source->OnTabletToolButton(EF_RIGHT_MOUSE_BUTTON, /*pressed=*/false,
                                     {}, base::TimeTicks::Now());
    ::testing::Mock::VerifyAndClearExpectations(&delegate);
  }
}

INSTANTIATE_TEST_SUITE_P(
    EventsDispatchPolicyTest,
    WaylandEventSourceTest,
    ::testing::Values(
        FeatureState{.dispatch_mouse_events_on_frame_event = false,
                     .dispatch_touch_events_on_frame_event = false},
        FeatureState{.dispatch_mouse_events_on_frame_event = true,
                     .dispatch_touch_events_on_frame_event = true}));

}  // namespace ui
