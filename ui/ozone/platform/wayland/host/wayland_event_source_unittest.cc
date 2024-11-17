// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_event_source.h"

#include <linux/input.h>

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
    CHECK(
        !base::Contains(enabled_features_, kDispatchPointerEventsOnFrameEvent));
    CHECK(!base::Contains(disabled_features_,
                          kDispatchPointerEventsOnFrameEvent));
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

  MockWaylandPlatformWindowDelegate delegate;
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

INSTANTIATE_TEST_SUITE_P(
    EventsDispatchPolicyTest,
    WaylandEventSourceTest,
    ::testing::Values(
        FeatureState{.dispatch_mouse_events_on_frame_event = false,
                     .dispatch_touch_events_on_frame_event = false},
        FeatureState{.dispatch_mouse_events_on_frame_event = true,
                     .dispatch_touch_events_on_frame_event = true}));

}  // namespace ui
