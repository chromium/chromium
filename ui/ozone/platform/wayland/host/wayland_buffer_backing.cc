// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_buffer_backing.h"

#include "base/check_op.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_handle.h"

namespace ui {

WaylandBufferBacking::WaylandBufferBacking(const WaylandConnection* connection,
                                           uint32_t buffer_id,
                                           const gfx::Size& size)
    : connection_(connection), buffer_id_(buffer_id), size_(size) {
  DCHECK(connection_);
  DCHECK_NE(buffer_id_, kInvalidBufferId);
}

WaylandBufferBacking::~WaylandBufferBacking() = default;

bool WaylandBufferBacking::EnsureBufferHandle(WaylandSurface* requestor) {
  auto& buffer_handle = buffer_handles_[requestor];

  if (buffer_handle)
    return buffer_handle->wl_buffer_.get();

  // Assign the anonymous handle to the |requestor|.
  auto& anonymous_handle = buffer_handles_[nullptr];
  if (anonymous_handle) {
    buffer_handle.swap(anonymous_handle);
    return buffer_handle->wl_buffer_.get();
  }

  // The requested wl_buffer object is not requested.
  buffer_handle = std::make_unique<WaylandBufferHandle>(this);

  // Create wl_buffer associated with the internal Buffer.
  RequestBufferHandle(base::BindOnce(&WaylandBufferHandle::OnWlBufferCreated,
                                     buffer_handle->AsWeakPtr()));

  return buffer_handle->wl_buffer_.get();
}

WaylandBufferHandle* WaylandBufferBacking::GetBufferHandle(
    WaylandSurface* requestor) {
  DCHECK(requestor);
  return buffer_handles_[requestor].get();
}

}  // namespace ui
