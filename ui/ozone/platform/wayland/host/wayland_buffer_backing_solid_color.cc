// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_buffer_backing_solid_color.h"

#include "ui/ozone/platform/wayland/host/surface_augmenter.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

WaylandBufferBackingSolidColor::WaylandBufferBackingSolidColor(
    const WaylandConnection* connection,
    SkColor4f color,
    const gfx::Size& size,
    uint32_t buffer_id)
    : WaylandBufferBacking(connection, buffer_id, size), color_(color) {}

WaylandBufferBackingSolidColor::~WaylandBufferBackingSolidColor() = default;

void WaylandBufferBackingSolidColor::RequestBufferHandle(
    base::OnceCallback<void(wl::Object<wl_buffer>)> callback) {
  DCHECK(!callback.is_null());
  std::move(callback).Run(
      connection()->surface_augmenter()->CreateSolidColorBuffer(color_,
                                                                size()));
}

WaylandBufferBacking::BufferBackingType
WaylandBufferBackingSolidColor::GetBackingType() const {
  return WaylandBufferBacking::BufferBackingType::kSolidColor;
}

}  // namespace ui
