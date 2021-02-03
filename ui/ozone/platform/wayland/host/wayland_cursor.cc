// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_cursor.h"

#include <wayland-cursor.h>
#include <memory>
#include <vector>

#include "base/logging.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/skia_util.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"
#include "ui/ozone/platform/wayland/host/wayland_shm.h"

namespace ui {

WaylandCursor::WaylandCursor(WaylandPointer* pointer,
                             WaylandConnection* connection)
    : pointer_(pointer),
      connection_(connection),
      pointer_surface_(connection->CreateSurface()) {}

WaylandCursor::~WaylandCursor() = default;

// static
void WaylandCursor::OnBufferRelease(void* data, wl_buffer* buffer) {
  auto* cursor = static_cast<WaylandCursor*>(data);
  DCHECK_GT(cursor->buffers_.count(buffer), 0u);
  cursor->buffers_.erase(buffer);
}

void WaylandCursor::UpdateBitmap(const std::vector<SkBitmap>& cursor_image,
                                 const gfx::Point& hotspot_in_dips,
                                 uint32_t serial,
                                 int buffer_scale) {
  DCHECK(connection_->shm());
  if (!pointer_)
    return;

  if (!cursor_image.size())
    return HideCursor(serial);

  const SkBitmap& image = cursor_image[0];
  if (image.dimensions().isEmpty())
    return HideCursor(serial);

  gfx::Size image_size = gfx::SkISizeToSize(image.dimensions());
  WaylandShmBuffer buffer(connection_->shm(), image_size);

  if (!buffer.IsValid()) {
    LOG(ERROR) << "Failed to create SHM buffer for Cursor Bitmap.";
    return HideCursor(serial);
  }

  static const struct wl_buffer_listener wl_buffer_listener {
    &WaylandCursor::OnBufferRelease
  };
  wl_buffer_add_listener(buffer.get(), &wl_buffer_listener, this);

  wl::DrawBitmap(image, &buffer);

  wl_surface_set_buffer_scale(pointer_surface_.get(), buffer_scale);

  wl_pointer_set_cursor(pointer_->wl_object(), serial, pointer_surface_.get(),
                        hotspot_in_dips.x(), hotspot_in_dips.y());
  wl_surface_damage(pointer_surface_.get(), 0, 0, image_size.width(),
                    image_size.height());
  wl_surface_attach(pointer_surface_.get(), buffer.get(), 0, 0);
  wl_surface_commit(pointer_surface_.get());

  auto* address = buffer.get();
  buffers_.emplace(address, std::move(buffer));

  if (listener_)
    listener_->OnCursorBufferAttached(nullptr);
}

void WaylandCursor::SetPlatformShape(wl_cursor* cursor_data,
                                     uint32_t serial,
                                     int buffer_scale) {
  if (!pointer_)
    return;

  wl_cursor_image* cursor_image = cursor_data->images[0];
  wl_buffer* cursor_buffer = wl_cursor_image_get_buffer(cursor_image);

  wl_pointer_set_cursor(pointer_->wl_object(), serial, pointer_surface_.get(),
                        cursor_image->hotspot_x, cursor_image->hotspot_y);
  wl_surface_set_buffer_scale(pointer_surface_.get(), buffer_scale);
  wl_surface_damage(pointer_surface_.get(), 0, 0, cursor_image->width,
                    cursor_image->height);
  wl_surface_attach(pointer_surface_.get(), cursor_buffer, 0, 0);
  wl_surface_commit(pointer_surface_.get());

  if (listener_)
    listener_->OnCursorBufferAttached(cursor_data);
}

void WaylandCursor::HideCursor(uint32_t serial) {
  DCHECK(pointer_);
  wl_pointer_set_cursor(pointer_->wl_object(), serial, nullptr, 0, 0);

  wl_surface_attach(pointer_surface_.get(), nullptr, 0, 0);
  wl_surface_commit(pointer_surface_.get());

  connection_->ScheduleFlush();

  if (listener_)
    listener_->OnCursorBufferAttached(nullptr);
}

}  // namespace ui
