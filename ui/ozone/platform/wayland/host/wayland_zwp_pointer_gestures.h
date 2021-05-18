// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZWP_POINTER_GESTURES_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZWP_POINTER_GESTURES_H_

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
class WaylandZwpPointerGestures {
 public:
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
  WaylandConnection* const connection_;
  Delegate* const delegate_;
};

class WaylandZwpPointerGestures::Delegate {
 public:
  virtual void OnPinchEvent(EventType event_type,
                            const gfx::Vector2dF& delta,
                            base::TimeTicks timestamp,
                            int device_id,
                            absl::optional<float> scale = absl::nullopt) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZWP_POINTER_GESTURES_H_
