// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_buffer_handle.h"

namespace ui {

WaylandBufferHandle::WaylandBufferHandle(WaylandBufferBacking* backing)
    : backing_(backing), weak_factory_(this) {}

WaylandBufferHandle::~WaylandBufferHandle() {
  if (!released_callback_.is_null())
    std::move(released_callback_).Run();
}

void WaylandBufferHandle::OnWlBufferCreated(
    wl::Object<struct wl_buffer> wl_buffer) {
  CHECK(wl_buffer) << "Failed to create wl_buffer object.";

  wl_buffer_ = std::move(wl_buffer);

  // Setup buffer release listener callbacks.
  static struct wl_buffer_listener buffer_listener = {
      &WaylandBufferHandle::BufferRelease,
  };
  wl_buffer_add_listener(wl_buffer_.get(), &buffer_listener, this);

  if (!created_callback_.is_null())
    std::move(created_callback_).Run();
}

void WaylandBufferHandle::OnRelease(struct wl_buffer* wl_buff) {
  DCHECK_EQ(wl_buff, wl_buffer_.get());
  if (!released_callback_.is_null())
    std::move(released_callback_).Run();
}

base::WeakPtr<WaylandBufferHandle> WaylandBufferHandle::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

// static
void WaylandBufferHandle::BufferRelease(void* data,
                                        struct wl_buffer* wl_buffer) {
  WaylandBufferHandle* self = static_cast<WaylandBufferHandle*>(data);
  DCHECK(self);
  self->OnRelease(wl_buffer);
}

}  // namespace ui
