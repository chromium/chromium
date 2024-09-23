// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZWP_POINTER_GESTURES_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZWP_POINTER_GESTURES_H_

#include <pointer-gestures-unstable-v1-client-protocol.h>

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/events/types/event_type.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"

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
  // zwp_pointer_gesture_pinch_v1_listener callbacks:
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

  // zwp_pointer_gesture_hold_v1_listener callbacks:
#if defined(ZWP_POINTER_GESTURE_HOLD_V1_BEGIN_SINCE_VERSION)
  static void OnHoldBegin(
      void* data,
      struct zwp_pointer_gesture_hold_v1* zwp_pointer_gesture_hold_v1,
      uint32_t serial,
      uint32_t time,
      struct wl_surface* surface,
      uint32_t fingers);
#endif
#if defined(ZWP_POINTER_GESTURE_HOLD_V1_END_SINCE_VERSION)
  static void OnHoldEnd(
      void* data,
      struct zwp_pointer_gesture_hold_v1* zwp_pointer_gesture_hold_v1,
      uint32_t serial,
      uint32_t time,
      int32_t cancelled);
#endif

  wl::Object<zwp_pointer_gestures_v1> obj_;
  wl::Object<zwp_pointer_gesture_pinch_v1> pinch_;
#if defined(ZWP_POINTER_GESTURES_V1_GET_HOLD_GESTURE_SINCE_VERSION)
  wl::Object<zwp_pointer_gesture_hold_v1> hold_;
#endif
  double current_scale_ = 1;
  const raw_ptr<WaylandConnection> connection_;
  const raw_ptr<Delegate> delegate_;
};

class WaylandZwpPointerGestures::Delegate {
 public:
  // Handles the events coming during the pinch zoom session.
  // |event_type| is one of EventType::kGesturePinch### members.
  // |delta| is empty on the BEGIN and END, and shows the movement of the centre
  // of the gesture compared to the previous event.
  // |scale_delta| is the change to the scale compared to the previous event, to
  // be applied as multiplier (as the compositor expects it).
  virtual void OnPinchEvent(
      EventType event_type,
      const gfx::Vector2dF& delta,
      base::TimeTicks timestamp,
      int device_id,
      std::optional<float> scale_delta = std::nullopt) = 0;

  virtual void OnHoldEvent(EventType event_type,
                           uint32_t finger_count,
                           base::TimeTicks timestamp,
                           int device_id,
                           wl::EventDispatchPolicy dispatch_policy) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZWP_POINTER_GESTURES_H_
