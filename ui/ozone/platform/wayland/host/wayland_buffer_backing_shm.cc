// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_buffer_backing_shm.h"

#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_shm.h"

namespace ui {

WaylandBufferBackingShm::WaylandBufferBackingShm(
    const WaylandConnection* connection,
    base::ScopedFD fd,
    uint64_t length,
    const gfx::Size& size,
    uint32_t buffer_id)
    : WaylandBufferBacking(connection, buffer_id, size),
      fd_(std::move(fd)),
      length_(length) {}

WaylandBufferBackingShm::~WaylandBufferBackingShm() = default;

void WaylandBufferBackingShm::RequestBufferHandle(
    base::OnceCallback<void(wl::Object<wl_buffer>)> callback) {
  DCHECK(!callback.is_null());
  std::move(callback).Run(
      connection_->shm()->CreateBuffer(fd_, length_, size()));
}

}  // namespace ui
