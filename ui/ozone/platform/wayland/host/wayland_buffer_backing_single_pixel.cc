// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_buffer_backing_single_pixel.h"

#include "ui/ozone/platform/wayland/host/single_pixel_buffer.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

WaylandBufferBackingSinglePixel::WaylandBufferBackingSinglePixel(
    const WaylandConnection* connection,
    SkColor4f color,
    uint32_t buffer_id)
    : WaylandBufferBacking(connection, buffer_id, gfx::Size(1, 1)),
      color_(color) {}

WaylandBufferBackingSinglePixel::~WaylandBufferBackingSinglePixel() = default;

void WaylandBufferBackingSinglePixel::RequestBufferHandle(
    base::OnceCallback<void(wl::Object<wl_buffer>)> callback) {
  DCHECK(!callback.is_null());
  std::move(callback).Run(
      connection()->single_pixel_buffer()->CreateSinglePixelBuffer(color_));
}

WaylandBufferBacking::BufferBackingType
WaylandBufferBackingSinglePixel::GetBackingType() const {
  return WaylandBufferBacking::BufferBackingType::kSinglePixel;
}

}  // namespace ui
