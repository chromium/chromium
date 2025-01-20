// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_touch.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/ozone/common/features.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

namespace {

// See TODO in //ui/ozone/common/features.cc
wl::EventDispatchPolicy GetEventDispatchPolicy() {
  return IsDispatchTouchEventsOnFrameEventEnabled()
             ? wl::EventDispatchPolicy::kOnFrame
             : wl::EventDispatchPolicy::kImmediate;
}

}  // namespace

WaylandTouch::WaylandTouch(wl_touch* touch,
                           WaylandConnection* connection,
                           Delegate* delegate)
    : obj_(touch), connection_(connection), delegate_(delegate) {
  static constexpr wl_touch_listener kTouchListener = {
      .down = &OnTouchDown,
      .up = &OnTouchUp,
      .motion = &OnTouchMotion,
      .frame = &OnTouchFrame,
      .cancel = &OnTouchCancel,
      .shape = &OnTouchShape,
      .orientation = &OnTouchOrientation,
  };

  wl_touch_add_listener(obj_.get(), &kTouchListener, this);
}

WaylandTouch::~WaylandTouch() {
  delegate_->OnTouchCancelEvent();
}

// static
void WaylandTouch::OnTouchDown(void* data,
                               wl_touch* touch,
                               uint32_t serial,
                               uint32_t time,
                               struct wl_surface* surface,
                               int32_t id,
                               wl_fixed_t x,
                               wl_fixed_t y) {
  if (!surface)
    return;

  auto* self = static_cast<WaylandTouch*>(data);
  DCHECK(self);

  self->connection_->serial_tracker().UpdateSerial(wl::SerialType::kTouchPress,
                                                   serial);

  WaylandWindow* window = wl::RootWindowFromWlSurface(surface);
  if (!window) {
    return;
  }

  self->delegate_->OnTouchPressEvent(
      window, gfx::PointF(wl_fixed_to_double(x), wl_fixed_to_double(y)),
      wl::EventMillisecondsToTimeTicks(time), id, GetEventDispatchPolicy());
}

// static
void WaylandTouch::OnTouchUp(void* data,
                             wl_touch* touch,
                             uint32_t serial,
                             uint32_t time,
                             int32_t id) {
  auto* self = static_cast<WaylandTouch*>(data);
  DCHECK(self);

  self->delegate_->OnTouchReleaseEvent(wl::EventMillisecondsToTimeTicks(time),
                                       id, GetEventDispatchPolicy(),
                                       /*is_synthesized=*/false);
}

// static
void WaylandTouch::OnTouchMotion(void* data,
                                 wl_touch* touch,
                                 uint32_t time,
                                 int32_t id,
                                 wl_fixed_t x,
                                 wl_fixed_t y) {
  auto* self = static_cast<WaylandTouch*>(data);
  DCHECK(self);

  const WaylandWindow* target = self->delegate_->GetTouchTarget(id);
  if (!target) {
    LOG(WARNING) << "Touch event fired with wrong id";
    return;
  }
  self->delegate_->OnTouchMotionEvent(
      gfx::PointF(wl_fixed_to_double(x), wl_fixed_to_double(y)),
      wl::EventMillisecondsToTimeTicks(time), id, GetEventDispatchPolicy(),
      /*is_synthesized=*/false);
}

// static
void WaylandTouch::OnTouchShape(void* data,
                                wl_touch* touch,
                                int32_t id,
                                wl_fixed_t major,
                                wl_fixed_t minor) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void WaylandTouch::OnTouchOrientation(void* data,
                                      wl_touch* touch,
                                      int32_t id,
                                      wl_fixed_t orientation) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void WaylandTouch::OnTouchCancel(void* data, wl_touch* touch) {
  auto* self = static_cast<WaylandTouch*>(data);
  DCHECK(self);

  self->delegate_->OnTouchCancelEvent();
}

// static
void WaylandTouch::OnTouchFrame(void* data, wl_touch* touch) {
  auto* self = static_cast<WaylandTouch*>(data);
  DCHECK(self);

  self->delegate_->OnTouchFrame();
}

}  // namespace ui
