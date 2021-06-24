// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zwp_relative_pointer_manager.h"

#include <relative-pointer-unstable-v1-client-protocol.h>

#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"

namespace ui {

WaylandZwpRelativePointerManager::WaylandZwpRelativePointerManager(
    zwp_relative_pointer_manager_v1* relative_pointer_manager,
    WaylandConnection* connection)
    : obj_(relative_pointer_manager), connection_(connection) {
  DCHECK(obj_);
  DCHECK(connection_);
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
}

void WaylandZwpRelativePointerManager::DisableRelativePointer() {
  relative_pointer_.reset();
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
  NOTIMPLEMENTED();
}

}  // namespace ui
