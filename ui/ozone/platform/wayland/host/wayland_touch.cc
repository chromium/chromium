// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_touch.h"

#include <stylus-unstable-v2-client-protocol.h>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

namespace {

// TODO(crbug.com/40235357): Remove this method when Compositors other
// than Exo comply with `wl_touch.frame`.
//
// For instance, on Gnome/Wayland, KDE and Weston compositors a wl_touch.up does
// not come accompanied by a respective wl_touch.frame event. On these scenarios
// be conservative and always dispatch the events immediately.
wl::EventDispatchPolicy EventDispatchPolicyForPlatform() {
  return
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      wl::EventDispatchPolicy::kOnFrame;
#else
      wl::EventDispatchPolicy::kImmediate;
#endif
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

  SetupStylus();
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

  gfx::PointF location = self->connection_->MaybeConvertLocation(
      gfx::PointF(wl_fixed_to_double(x), wl_fixed_to_double(y)), window);
  self->delegate_->OnTouchPressEvent(window, location,
                                     wl::EventMillisecondsToTimeTicks(time), id,
                                     EventDispatchPolicyForPlatform());
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
                                       id, EventDispatchPolicyForPlatform(),
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
  gfx::PointF location = self->connection_->MaybeConvertLocation(
      gfx::PointF(wl_fixed_to_double(x), wl_fixed_to_double(y)), target);
  self->delegate_->OnTouchMotionEvent(location,
                                      wl::EventMillisecondsToTimeTicks(time),
                                      id, EventDispatchPolicyForPlatform(),
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

void WaylandTouch::SetupStylus() {
  auto* stylus_v2 = connection_->stylus_v2();
  if (!stylus_v2)
    return;

  zcr_touch_stylus_v2_.reset(
      zcr_stylus_v2_get_touch_stylus(stylus_v2, obj_.get()));

  static constexpr zcr_touch_stylus_v2_listener kTouchStylusListener = {
      .tool = &OnTouchStylusTool,
      .force = &OnTouchStylusForce,
      .tilt = &OnTouchStylusTilt};
  zcr_touch_stylus_v2_add_listener(zcr_touch_stylus_v2_.get(),
                                   &kTouchStylusListener, this);
}

// static
void WaylandTouch::OnTouchStylusTool(void* data,
                                     struct zcr_touch_stylus_v2* stylus,
                                     uint32_t id,
                                     uint32_t stylus_type) {
  auto* self = static_cast<WaylandTouch*>(data);
  DCHECK(self);

  ui::EventPointerType pointer_type = ui::EventPointerType::kTouch;
  switch (stylus_type) {
    case ZCR_TOUCH_STYLUS_V2_TOOL_TYPE_PEN:
      pointer_type = EventPointerType::kPen;
      break;
    case ZCR_TOUCH_STYLUS_V2_TOOL_TYPE_ERASER:
      pointer_type = ui::EventPointerType::kEraser;
      break;
    case ZCR_POINTER_STYLUS_V2_TOOL_TYPE_TOUCH:
      break;
  }

  self->delegate_->OnTouchStylusToolChanged(id, pointer_type);
}

// static
void WaylandTouch::OnTouchStylusForce(void* data,
                                      struct zcr_touch_stylus_v2* stylus,
                                      uint32_t time,
                                      uint32_t id,
                                      wl_fixed_t force) {
  auto* self = static_cast<WaylandTouch*>(data);
  DCHECK(self);

  self->delegate_->OnTouchStylusForceChanged(id, wl_fixed_to_double(force));
}

// static
void WaylandTouch::OnTouchStylusTilt(void* data,
                                     struct zcr_touch_stylus_v2* stylus,
                                     uint32_t time,
                                     uint32_t id,
                                     wl_fixed_t tilt_x,
                                     wl_fixed_t tilt_y) {
  auto* self = static_cast<WaylandTouch*>(data);
  DCHECK(self);

  self->delegate_->OnTouchStylusTiltChanged(
      id,
      gfx::Vector2dF(wl_fixed_to_double(tilt_x), wl_fixed_to_double(tilt_y)));
}

}  // namespace ui
