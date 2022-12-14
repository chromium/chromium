// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_buffer_backing_dmabuf.h"

#include "ui/ozone/platform/wayland/host/wayland_buffer_factory.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

WaylandBufferBackingDmabuf::WaylandBufferBackingDmabuf(
    const WaylandConnection* connection,
    base::ScopedFD fd,
    const gfx::Size& size,
    std::vector<uint32_t> strides,
    std::vector<uint32_t> offsets,
    std::vector<uint64_t> modifiers,
    uint32_t format,
    uint32_t planes_count,
    uint32_t buffer_id)
    : WaylandBufferBacking(connection, buffer_id, size, format),
      fd_(std::move(fd)),
      strides_(std::move(strides)),
      offsets_(std::move(offsets)),
      modifiers_(std::move(modifiers)),
      planes_count_(planes_count) {}

WaylandBufferBackingDmabuf::~WaylandBufferBackingDmabuf() = default;

void WaylandBufferBackingDmabuf::RequestBufferHandle(
    base::OnceCallback<void(wl::Object<wl_buffer>)> callback) {
  DCHECK(!callback.is_null());
  DCHECK(fd_.is_valid());
  connection()->buffer_factory()->CreateDmabufBuffer(
      fd_, size(), strides_, offsets_, modifiers_, format(), planes_count_,
      std::move(callback));

  if (UseExplicitSyncRelease())
    auto close = std::move(fd_);
}

WaylandBufferBacking::BufferBackingType
WaylandBufferBackingDmabuf::GetBackingType() const {
  return WaylandBufferBacking::BufferBackingType::kDmabuf;
}

}  // namespace ui
