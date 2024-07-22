// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <pointer-gestures-unstable-v1-server-protocol.h>
#include <wayland-util.h>

#include "build/chromeos_buildflags.h"
#include "ui/events/event.h"
#include "ui/events/platform/platform_event_observer.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/wayland_zwp_pointer_gestures.h"
#include "ui/ozone/platform/wayland/test/mock_pointer.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

namespace ui {

namespace {

// Observes events, filters the pinch zoom updates, and records the latest
// update to the scale.
class PinchEventScaleRecorder : public PlatformEventObserver {
 public:
  PinchEventScaleRecorder() {
    PlatformEventSource::GetInstance()->AddPlatformEventObserver(this);
  }

  PinchEventScaleRecorder(const PinchEventScaleRecorder&) = delete;
  PinchEventScaleRecorder& operator=(const PinchEventScaleRecorder&) = delete;

  ~PinchEventScaleRecorder() override {
    PlatformEventSource::GetInstance()->RemovePlatformEventObserver(this);
  }

  double latest_scale_update() const { return latest_scale_update_; }

 protected:
  // PlatformEventObserver:
  void WillProcessEvent(const PlatformEvent& event) override {
    if (!event->IsGestureEvent())
      return;

    const GestureEvent* const gesture = event->AsGestureEvent();
    if (!gesture->IsPinchEvent() ||
        gesture->type() != EventType::kGesturePinchUpdate) {
      return;
    }

    latest_scale_update_ = gesture->details().scale();
  }

  void DidProcessEvent(const PlatformEvent& event) override {}

  double latest_scale_update_ = 1.0;
};

}  // namespace

class WaylandPointerGesturesTest : public WaylandTestSimple {
 public:
  void SetUp() override {
    WaylandTestSimple::SetUp();

    // Pointer capability is required for gesture objects to be initialised.
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      wl_seat_send_capabilities(server->seat()->resource(),
                                WL_SEAT_CAPABILITY_POINTER);
    });

    ASSERT_TRUE(connection_->seat()->pointer());
    ASSERT_TRUE(connection_->zwp_pointer_gestures());
  }

 protected:
  // This method simulates entering a pointer into a Wayland surface.
  void EnterPoint() {
    PostToServerAndWait(
        [surface_id = window_->root_surface()->get_surface_id()](
            wl::TestWaylandServerThread* server) {
          auto* const pointer = server->seat()->pointer()->resource();
          auto* const surface =
              server->GetObject<wl::MockSurface>(surface_id)->resource();
          wl_pointer_send_enter(pointer, server->GetNextSerial(), surface,
                                wl_fixed_from_int(50), wl_fixed_from_int(50));
          wl_pointer_send_frame(pointer);
        });
  }
};

ACTION_P(CloneEvent, ptr) {
  *ptr = arg0->Clone();
}

// Tests that scale in pinch zoom events is fixed to the progression expected by
// the compositor.
//
// During the pinch zoom session, libinput sends the current scale relative to
// the start of the session.  The compositor, however, expects every update to
// have the relative change of the scale, compared to the previous update in
// form of a multiplier applied to the current value.  The factor is fixed at
// the low level immediately after values are received from the server via
// WaylandZwpPointerGestures methods.
//
// See https://crbug.com/1283652
TEST_F(WaylandPointerGesturesTest, PinchZoomScale) {
  // Enter cursor to surface.
  EnterPoint();

  PinchEventScaleRecorder observer;

  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const pinch = server->wp_pointer_gestures().pinch()->resource();
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();

    zwp_pointer_gesture_pinch_v1_send_begin(pinch, server->GetNextSerial(),
                                            server->GetNextTime(), surface,
                                            /* fingers */ 2);
  });

  constexpr double kScales[] = {1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.4,
                                1.3, 1.2, 1.1, 1.0, 0.9, 0.8, 0.7,
                                0.6, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0};
  [[maybe_unused]] auto previous_scale = kScales[0];
  for (auto scale : kScales) {
    PostToServerAndWait([scale](wl::TestWaylandServerThread* server) {
      auto* const pinch = server->wp_pointer_gestures().pinch()->resource();

      zwp_pointer_gesture_pinch_v1_send_update(
          pinch, /* time */ 0, /* dx */ 0, /* dy */ 0,
          wl_fixed_from_double(scale), /* rotation */ 0);
    });
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    EXPECT_FLOAT_EQ(observer.latest_scale_update(),
                    wl_fixed_to_double(wl_fixed_from_double(scale)));
#else
    // The conversion from double to fixed and back is necessary because it
    // happens during the roundtrip, and it creates significant error.
    EXPECT_FLOAT_EQ(
        observer.latest_scale_update(),
        wl_fixed_to_double(wl_fixed_from_double(scale)) / previous_scale);
    previous_scale = wl_fixed_to_double(wl_fixed_from_double(scale));
#endif
  }
}

// Test ensures Hold Event ends a fling.
TEST_F(WaylandPointerGesturesTest, HoldEventCancelsFling) {
  // Enter cursor to surface.
  EnterPoint();
  std::unique_ptr<Event> event1, event2, event3;
  EXPECT_CALL(delegate_, DispatchEvent(testing::_))
      .Times(3)
      .WillOnce(CloneEvent(&event1))
      .WillOnce(CloneEvent(&event2))
      .WillOnce(CloneEvent(&event3));
  // Send vertical scroll event.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const pointer = server->seat()->pointer()->resource();
    wl_pointer_send_axis_source(pointer, WL_POINTER_AXIS_SOURCE_FINGER);
    wl_pointer_send_axis(pointer, server->GetNextTime(),
                         WL_POINTER_AXIS_VERTICAL_SCROLL,
                         wl_fixed_from_int(50));
    wl_pointer_send_frame(pointer);
  });
  // Advance time to emulate delay between events and allow the fling gesture
  // to be recognised.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  // Send axis_stop event.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* const pointer = server->seat()->pointer()->resource();
    wl_pointer_send_axis_stop(pointer, server->GetNextTime(),
                              WL_POINTER_AXIS_VERTICAL_SCROLL);
    wl_pointer_send_axis_stop(pointer, server->GetNextTime(),
                              WL_POINTER_AXIS_HORIZONTAL_SCROLL);
    wl_pointer_send_frame(pointer);
  });
  // Send hold begin event.
  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const hold = server->wp_pointer_gestures().hold()->resource();
    auto* const surface =
        server->GetObject<wl::MockSurface>(surface_id)->resource();
    zwp_pointer_gesture_hold_v1_send_begin(hold, server->GetNextSerial(),
                                           server->GetNextTime(), surface,
                                           /* fingers */ 1);
  });
  // Advance time to emulate delay between hold start and hold end
  task_environment_.FastForwardBy(base::Milliseconds(1));
  // Send hold end event.
  PostToServerAndWait([surface_id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* const hold = server->wp_pointer_gestures().hold()->resource();
    zwp_pointer_gesture_hold_v1_send_end(hold, server->GetNextSerial(),
                                         server->GetNextTime(),
                                         /* cancelled */ 1);
  });
  // One axis event should follow before the fling event.
  ASSERT_TRUE(event1);
  ASSERT_TRUE(event1->IsScrollEvent());
  // We expect a FLING_START event.
  ASSERT_TRUE(event2);
  ASSERT_TRUE(event2->IsScrollEvent());
  auto* scroll_event2 = event2->AsScrollEvent();
  EXPECT_EQ(EventType::kScrollFlingStart, scroll_event2->type());
  // Finally, FLING_CANCEL is expected.
  ASSERT_TRUE(event3);
  ASSERT_TRUE(event3->IsScrollEvent());
  auto* scroll_event3 = event3->AsScrollEvent();
  EXPECT_EQ(EventType::kScrollFlingCancel, scroll_event3->type());
  // Check the offset direction. It should be zero in both axes.
  EXPECT_EQ(0.0f, scroll_event3->x_offset());
  EXPECT_EQ(0.0f, scroll_event3->y_offset());
  EXPECT_EQ(0.0f, scroll_event3->x_offset_ordinal());
  EXPECT_EQ(0.0f, scroll_event3->y_offset_ordinal());
}

}  // namespace ui
