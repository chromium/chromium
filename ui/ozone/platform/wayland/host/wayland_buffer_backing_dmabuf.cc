// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_buffer_backing_dmabuf.h"

#include "ui/ozone/platform/wayland/host/wayland_buffer_factory.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

WaylandBufferBackingDmabuf::WaylandBufferBackingDmabuf(
    const WaylandConnection* connection,
    std::vector<base::ScopedFD> fds,
    const gfx::Size& size,
    std::vector<uint32_t> strides,
    std::vector<uint32_t> offsets,
    std::vector<uint64_t> modifiers,
    uint32_t format,
    uint32_t planes_count,
    uint32_t buffer_id)
    : WaylandBufferBacking(connection,
                           buffer_id,
                           size,
                           BufferBackingType::kDmabuf),
      fds_(std::move(fds)),
      strides_(std::move(strides)),
      offsets_(std::move(offsets)),
      modifiers_(std::move(modifiers)),
      format_(format),
      planes_count_(planes_count) {}

WaylandBufferBackingDmabuf::~WaylandBufferBackingDmabuf() = default;

void WaylandBufferBackingDmabuf::RequestBufferHandle(
    base::OnceCallback<void(wl::Object<wl_buffer>)> callback) {
  CHECK(!callback.is_null(), base::NotFatalUntil::M163);
  CHECK(!fds_.empty(), base::NotFatalUntil::M163);
  connection()->buffer_factory()->CreateDmabufBuffer(
      fds_, size(), strides_, offsets_, modifiers_, format_, planes_count_,
      std::move(callback));

  if (UseExplicitSyncRelease())
    auto close = std::move(fds_);
}

}  // namespace ui
