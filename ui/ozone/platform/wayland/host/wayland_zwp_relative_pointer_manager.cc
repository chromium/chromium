// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zwp_relative_pointer_manager.h"

#include <relative-pointer-unstable-v1-client-protocol.h>

#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"

namespace ui {

WaylandZwpRelativePointerManager::WaylandZwpRelativePointerManager(
    zwp_relative_pointer_manager_v1* relative_pointer_manager,
    WaylandConnection* connection)
    : obj_(relative_pointer_manager),
      connection_(connection),
      delegate_(connection_->event_source()) {
  DCHECK(obj_);
  DCHECK(connection_);
  DCHECK(delegate_);
}

WaylandZwpRelativePointerManager::~WaylandZwpRelativePointerManager() = default;

void WaylandZwpRelativePointerManager::EnableRelativePointer() {
  relative_pointer_.reset(zwp_relative_pointer_manager_v1_get_relative_pointer(
      obj_.get(), connection_->pointer()->wl_object()));

  static const struct zwp_relative_pointer_v1_listener
      relative_pointer_listener = {
          &WaylandZwpRelativePointerManager::OnHandleMotion,
      };
  zwp_relative_pointer_v1_add_listener(relative_pointer_.get(),
                                       &relative_pointer_listener, this);
  delegate_->SetRelativePointerMotionEnabled(true);
}

void WaylandZwpRelativePointerManager::DisableRelativePointer() {
  relative_pointer_.reset();
  delegate_->SetRelativePointerMotionEnabled(false);
}

// static
void WaylandZwpRelativePointerManager::OnHandleMotion(
    void* data,
    struct zwp_relative_pointer_v1* pointer,
    uint32_t utime_hi,
    uint32_t utime_lo,
    wl_fixed_t dx,
    wl_fixed_t dy,
    wl_fixed_t dx_unaccel,
    wl_fixed_t dy_unaccel) {
  auto* relative_pointer_manager =
      static_cast<WaylandZwpRelativePointerManager*>(data);

  gfx::Vector2dF delta = {static_cast<float>(wl_fixed_to_double(dx)),
                          static_cast<float>(wl_fixed_to_double(dy))};
  gfx::Vector2dF delta_unaccel = {
      static_cast<float>(wl_fixed_to_double(dx_unaccel)),
      static_cast<float>(wl_fixed_to_double(dy_unaccel))};

  relative_pointer_manager->delegate_->OnRelativePointerMotion(delta);
}

}  // namespace ui
