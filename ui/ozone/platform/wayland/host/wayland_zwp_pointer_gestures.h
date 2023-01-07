// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZWP_POINTER_GESTURES_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZWP_POINTER_GESTURES_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/types/event_type.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace gfx {
class Vector2dF;
}

namespace ui {

class WaylandConnection;

// Wraps the zwp_pointer_gestures and zwp_pointer_gesture_pinch_v1 objects.
class WaylandZwpPointerGestures
    : public wl::GlobalObjectRegistrar<WaylandZwpPointerGestures> {
 public:
  static constexpr char kInterfaceName[] = "zwp_pointer_gestures_v1";

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

  class Delegate;

  WaylandZwpPointerGestures(zwp_pointer_gestures_v1* pointer_gestures,
                            WaylandConnection* connection,
                            Delegate* delegate);
  WaylandZwpPointerGestures(const WaylandZwpPointerGestures&) = delete;
  WaylandZwpPointerGestures& operator=(const WaylandZwpPointerGestures&) =
      delete;
  ~WaylandZwpPointerGestures();

  // Init is called by WaylandConnection when its wl_pointer object is
  // instantiated.
  void Init();

 private:
  // zwp_pointer_gesture_pinch_v1_listener
  static void OnPinchBegin(
      void* data,
      struct zwp_pointer_gesture_pinch_v1* zwp_pointer_gesture_pinch_v1,
      uint32_t serial,
      uint32_t time,
      struct wl_surface* surface,
      uint32_t fingers);
  static void OnPinchUpdate(
      void* data,
      struct zwp_pointer_gesture_pinch_v1* zwp_pointer_gesture_pinch_v1,
      uint32_t time,
      wl_fixed_t dx,
      wl_fixed_t dy,
      wl_fixed_t scale,
      wl_fixed_t rotation);
  static void OnPinchEnd(
      void* data,
      struct zwp_pointer_gesture_pinch_v1* zwp_pointer_gesture_pinch_v1,
      uint32_t serial,
      uint32_t time,
      int32_t cancelled);

  wl::Object<zwp_pointer_gestures_v1> obj_;
  wl::Object<zwp_pointer_gesture_pinch_v1> pinch_;
  double current_scale_ = 1;
  const raw_ptr<WaylandConnection> connection_;
  const raw_ptr<Delegate> delegate_;
};

class WaylandZwpPointerGestures::Delegate {
 public:
  // Handles the events coming during the pinch zoom session.
  // |event_type| is one of ET_GESTURE_PINCH_### members.
  // |delta| is empty on the BEGIN and END, and shows the movement of the centre
  // of the gesture compared to the previous event.
  // |scale_delta| is the change to the scale compared to the previous event, to
  // be applied as multiplier (as the compositor expects it).
  virtual void OnPinchEvent(
      EventType event_type,
      const gfx::Vector2dF& delta,
      base::TimeTicks timestamp,
      int device_id,
      absl::optional<float> scale_delta = absl::nullopt) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZWP_POINTER_GESTURES_H_
