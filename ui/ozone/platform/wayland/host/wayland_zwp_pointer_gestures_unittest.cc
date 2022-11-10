// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <pointer-gestures-unstable-v1-server-protocol.h>
#include <wayland-util.h>

#include "build/chromeos_buildflags.h"
#include "ui/events/event.h"
#include "ui/events/platform/platform_event_observer.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
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
    if (!gesture->IsPinchEvent() || gesture->type() != ET_GESTURE_PINCH_UPDATE)
      return;

    latest_scale_update_ = gesture->details().scale();
  }

  void DidProcessEvent(const PlatformEvent& event) override {}

  double latest_scale_update_ = 1.0;
};

}  // namespace

class WaylandPointerGesturesTest : public WaylandTest {
 public:
  WaylandPointerGesturesTest() = default;
  WaylandPointerGesturesTest(const WaylandPointerGesturesTest&) = delete;
  WaylandPointerGesturesTest& operator=(const WaylandPointerGesturesTest&) =
      delete;
  ~WaylandPointerGesturesTest() override = default;

  void SetUp() override {
    WaylandTest::SetUp();

    // Pointer capability is required for gesture objects to be initialised.
    wl_seat_send_capabilities(server_.seat()->resource(),
                              WL_SEAT_CAPABILITY_POINTER);

    Sync();

    ASSERT_TRUE(connection_->wayland_zwp_pointer_gestures());
  }
};

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
TEST_P(WaylandPointerGesturesTest, PinchZoomScale) {
  auto* const mock_surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->get_surface_id());

  uint32_t serial = 0;
  auto* pointer = server_.seat()->pointer();
  wl_pointer_send_enter(pointer->resource(), ++serial, mock_surface->resource(),
                        wl_fixed_from_int(50), wl_fixed_from_int(50));
  wl_pointer_send_frame(pointer->resource());

  PinchEventScaleRecorder observer;

  auto* pinch_resource = server_.wp_pointer_gestures().pinch()->resource();
  zwp_pointer_gesture_pinch_v1_send_begin(pinch_resource, ++serial,
                                          /* time */ 0,
                                          mock_surface->resource(),
                                          /* fingers */ 2);
  Sync();

  constexpr double kScales[] = {1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.4,
                                1.3, 1.2, 1.1, 1.0, 0.9, 0.8, 0.7,
                                0.6, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0};
  [[maybe_unused]] auto previous_scale = kScales[0];
  for (auto scale : kScales) {
    zwp_pointer_gesture_pinch_v1_send_update(
        pinch_resource, /* time */ 0, /* dx */ 0, /* dy */ 0,
        wl_fixed_from_double(scale), /* rotation */ 0);
    Sync();
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

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandPointerGesturesTest,
                         testing::Values(wl::ServerConfig{}));

}  // namespace ui
