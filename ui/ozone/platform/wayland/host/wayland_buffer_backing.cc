// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_buffer_backing.h"

#include "base/check_op.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_handle.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

WaylandBufferBacking::WaylandBufferBacking(const WaylandConnection* connection,
                                           uint32_t buffer_id,
                                           const gfx::Size& size,
                                           uint32_t format)
    : connection_(connection),
      format_(format),
      buffer_id_(buffer_id),
      size_(size) {
  DCHECK(connection_);
  DCHECK_NE(buffer_id_, kInvalidBufferId);
}

WaylandBufferBacking::~WaylandBufferBacking() = default;

bool WaylandBufferBacking::UseExplicitSyncRelease() const {
  return connection_->linux_explicit_synchronization_v1();
}

WaylandBufferHandle* WaylandBufferBacking::EnsureBufferHandle(
    WaylandSurface* requestor) {
  if (UseExplicitSyncRelease())
    requestor = nullptr;

  auto& buffer_handle = buffer_handles_[requestor];

  if (buffer_handle)
    return buffer_handle.get();

  // Assign the anonymous handle to the |requestor|.
  auto& anonymous_handle = buffer_handles_[nullptr];
  if (anonymous_handle) {
    buffer_handle.swap(anonymous_handle);
    return buffer_handle.get();
  }

  // The requested wl_buffer object is not requested.
  buffer_handle = std::make_unique<WaylandBufferHandle>(this);

  // Create wl_buffer associated with the internal Buffer.
  RequestBufferHandle(base::BindOnce(&WaylandBufferHandle::OnWlBufferCreated,
                                     buffer_handle->AsWeakPtr()));

  return buffer_handle.get();
}

WaylandBufferHandle* WaylandBufferBacking::GetBufferHandle(
    WaylandSurface* requestor) {
  DCHECK(requestor);
  if (UseExplicitSyncRelease()) {
    requestor = nullptr;
    DCHECK_LE(buffer_handles_.size(), 1u);
  }
  return buffer_handles_[requestor].get();
}

}  // namespace ui
