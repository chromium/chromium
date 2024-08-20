// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_cursor.h"

#include <wayland-cursor.h>

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"

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
                                 int buffer_scale) {
  if (!pointer_)
    return;

  if (!cursor_image.size())
    return HideCursor();

  const SkBitmap& image = cursor_image[0];
  if (image.dimensions().isEmpty())
    return HideCursor();

  gfx::Size image_size = gfx::SkISizeToSize(image.dimensions());
  WaylandShmBuffer buffer(connection_->buffer_factory(), image_size);

  if (!buffer.IsValid()) {
    LOG(ERROR) << "Failed to create SHM buffer for Cursor Bitmap.";
    return HideCursor();
  }

  buffer_scale_ = buffer_scale;
  wl_surface_set_buffer_scale(pointer_surface_.get(), buffer_scale_);

  static constexpr wl_buffer_listener wl_buffer_listener{
      &WaylandCursor::OnBufferRelease};
  wl_buffer_add_listener(buffer.get(), &wl_buffer_listener, this);

  wl::DrawBitmap(image, &buffer);

  AttachAndCommit(buffer.get(), image_size.width(), image_size.height(),
                  hotspot_in_dips.x(), hotspot_in_dips.y());

  auto* address = buffer.get();
  buffers_.emplace(address, std::move(buffer));

  if (listener_)
    listener_->OnCursorBufferAttached(nullptr);
}

void WaylandCursor::SetPlatformShape(wl_cursor* cursor_data, int buffer_scale) {
  if (!pointer_)
    return;

  animation_timer_.Stop();

  cursor_data_ = cursor_data;
  buffer_scale_ = buffer_scale;
  current_image_index_ = 0;

  wl_surface_set_buffer_scale(pointer_surface_.get(), buffer_scale_);

  SetPlatformShapeInternal();

  if (listener_)
    listener_->OnCursorBufferAttached(cursor_data);
}

void WaylandCursor::HideCursor() {
  auto pointer_enter_serial =
      connection_->serial_tracker().GetSerial(wl::SerialType::kMouseEnter);
  if (!pointer_enter_serial) {
    VLOG(1) << "Failed to hide cursor. No mouse enter serial found.";
    return;
  }

  DCHECK(pointer_);
  wl_pointer_set_cursor(pointer_->wl_object(), pointer_enter_serial->value,
                        nullptr, 0, 0);

  wl_surface_attach(pointer_surface_.get(), nullptr, 0, 0);
  wl_surface_commit(pointer_surface_.get());

  connection_->Flush();

  if (listener_)
    listener_->OnCursorBufferAttached(nullptr);
}

void WaylandCursor::SetPlatformShapeInternal() {
  DCHECK_GT(cursor_data_->image_count, 0U);

  // Under some conditions, either `images` is nullptr, or `image_count` is 0.
  // See https://crbug.com/1341202
  if (!cursor_data_->images || cursor_data_->image_count == 0U) {
    return;
  }

  // The image index is incremented every time the animation frame is committed.
  // Here we reset the counter if the final frame in the series has been sent,
  // so the new cycle of the animation starts.
  if (current_image_index_ >= cursor_data_->image_count)
    current_image_index_ = 0;

  // SAFETY: Wayland ensures that `images` and `image_count` describes a
  // valid span.
  auto image_span = UNSAFE_BUFFERS(
      base::span(cursor_data_->images, cursor_data_->image_count));
  wl_cursor_image* const cursor_image = image_span[current_image_index_];

  AttachAndCommit(wl_cursor_image_get_buffer(cursor_image), cursor_image->width,
                  cursor_image->height, cursor_image->hotspot_x / buffer_scale_,
                  cursor_image->hotspot_y / buffer_scale_);

  if (cursor_data_->image_count > 1 && cursor_image->delay > 0) {
    // If we have multiple frames, then we have animated cursor.  Schedule
    // sending the next frame.  See also the comment above.
    animation_timer_.Start(FROM_HERE, base::Milliseconds(cursor_image->delay),
                           this, &WaylandCursor::SetPlatformShapeInternal);
    ++current_image_index_;
  }
}

void WaylandCursor::AttachAndCommit(wl_buffer* buffer,
                                    uint32_t buffer_width,
                                    uint32_t buffer_height,
                                    uint32_t hotspot_x_dip,
                                    uint32_t hotspot_y_dip) {
  auto pointer_enter_serial =
      connection_->serial_tracker().GetSerial(wl::SerialType::kMouseEnter);
  if (!pointer_enter_serial) {
    VLOG(1) << "Failed to hide cursor. No mouse enter serial found.";
    return;
  }

  DCHECK(pointer_);

  wl_surface_damage(pointer_surface_.get(), 0, 0, buffer_width, buffer_height);
  // Note: should the offset be non-zero, use wl_surface_offset() to set it.
  wl_surface_attach(pointer_surface_.get(), buffer, 0, 0);
  // Set cursor before commit to ensure a role is set on the surface, so that
  // the compositor can exclude this surface from being checked that its buffer
  // size is divisible by the buffer scale.
  // For more details see the following:
  // - https://gitlab.freedesktop.org/wayland/wayland/-/issues/194
  // - https://gitlab.freedesktop.org/wayland/wayland/-/merge_requests/65
  // - https://wayland.app/protocols/wayland#wl_pointer:request:set_cursor
  //
  // Gnome in particular made a change which expects a role to be set, or it
  // will throw a protocol error when the size is not divisible by the scale:
  // https://gitlab.gnome.org/GNOME/mutter/-/commit/98c8c03729fd3fe72980d55fc3dccd8a0e9702e9
  wl_pointer_set_cursor(pointer_->wl_object(), pointer_enter_serial->value,
                        pointer_surface_.get(), hotspot_x_dip, hotspot_y_dip);
  wl_surface_commit(pointer_surface_.get());

  connection_->Flush();
}

}  // namespace ui
