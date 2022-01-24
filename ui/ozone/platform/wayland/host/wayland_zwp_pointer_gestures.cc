// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zwp_pointer_gestures.h"

#include <pointer-gestures-unstable-v1-client-protocol.h>
#include <wayland-util.h>

#include "base/logging.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor_position.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"
#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"

namespace ui {

namespace {
constexpr uint32_t kMinZwpPointerGesturesVersion = 1;
}

// static
constexpr char WaylandZwpPointerGestures::kInterfaceName[];

// static
void WaylandZwpPointerGestures::Instantiate(WaylandConnection* connection,
                                            wl_registry* registry,
                                            uint32_t name,
                                            const std::string& interface,
                                            uint32_t version) {
  DCHECK_EQ(interface, kInterfaceName);

  if (connection->wayland_zwp_pointer_gestures_ ||
      version < kMinZwpPointerGesturesVersion)
    return;

  auto zwp_pointer_gestures_v1 =
      wl::Bind<struct zwp_pointer_gestures_v1>(registry, name, version);
  if (!zwp_pointer_gestures_v1) {
    LOG(ERROR) << "Failed to bind wp_pointer_gestures_v1";
    return;
  }
  connection->wayland_zwp_pointer_gestures_ =
      std::make_unique<WaylandZwpPointerGestures>(
          zwp_pointer_gestures_v1.release(), connection,
          connection->event_source());
}

WaylandZwpPointerGestures::WaylandZwpPointerGestures(
    zwp_pointer_gestures_v1* pointer_gestures,
    WaylandConnection* connection,
    Delegate* delegate)
    : obj_(pointer_gestures), connection_(connection), delegate_(delegate) {
  DCHECK(obj_);
  DCHECK(connection_);
  DCHECK(delegate_);
}

WaylandZwpPointerGestures::~WaylandZwpPointerGestures() = default;

void WaylandZwpPointerGestures::Init() {
  DCHECK(connection_->pointer());

  pinch_.reset(zwp_pointer_gestures_v1_get_pinch_gesture(
      obj_.get(), connection_->pointer()->wl_object()));

  static constexpr zwp_pointer_gesture_pinch_v1_listener
      zwp_pointer_gesture_pinch_v1_listener = {
          &WaylandZwpPointerGestures::OnPinchBegin,
          &WaylandZwpPointerGestures::OnPinchUpdate,
          &WaylandZwpPointerGestures::OnPinchEnd,
      };
  zwp_pointer_gesture_pinch_v1_add_listener(
      pinch_.get(), &zwp_pointer_gesture_pinch_v1_listener, this);
}

// static
void WaylandZwpPointerGestures::OnPinchBegin(
    void* data,
    struct zwp_pointer_gesture_pinch_v1* zwp_pointer_gesture_pinch_v1,
    uint32_t serial,
    uint32_t time,
    struct wl_surface* surface,
    uint32_t fingers) {
  auto* thiz = static_cast<WaylandZwpPointerGestures*>(data);

  base::TimeTicks timestamp = base::TimeTicks() + base::Milliseconds(time);

  thiz->delegate_->OnPinchEvent(ET_GESTURE_PINCH_BEGIN,
                                gfx::Vector2dF() /*delta*/, timestamp,
                                thiz->obj_.id());
}

// static
void WaylandZwpPointerGestures::OnPinchUpdate(
    void* data,
    struct zwp_pointer_gesture_pinch_v1* zwp_pointer_gesture_pinch_v1,
    uint32_t time,
    wl_fixed_t dx,
    wl_fixed_t dy,
    wl_fixed_t scale,
    wl_fixed_t rotation) {
  auto* thiz = static_cast<WaylandZwpPointerGestures*>(data);

  base::TimeTicks timestamp = base::TimeTicks() + base::Milliseconds(time);

  gfx::Vector2dF delta = {static_cast<float>(wl_fixed_to_double(dx)),
                          static_cast<float>(wl_fixed_to_double(dy))};
  thiz->delegate_->OnPinchEvent(ET_GESTURE_PINCH_UPDATE, delta, timestamp,
                                thiz->obj_.id(), wl_fixed_to_double(scale));
}

void WaylandZwpPointerGestures::OnPinchEnd(
    void* data,
    struct zwp_pointer_gesture_pinch_v1* zwp_pointer_gesture_pinch_v1,
    uint32_t serial,
    uint32_t time,
    int32_t cancelled) {
  auto* thiz = static_cast<WaylandZwpPointerGestures*>(data);

  base::TimeTicks timestamp = base::TimeTicks() + base::Milliseconds(time);

  thiz->delegate_->OnPinchEvent(ET_GESTURE_PINCH_END,
                                gfx::Vector2dF() /*delta*/, timestamp,
                                thiz->obj_.id());
}

}  // namespace ui
