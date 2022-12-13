// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_buffer_handle.h"

#include <ostream>

namespace ui {

WaylandBufferHandle::WaylandBufferHandle(WaylandBufferBacking* backing)
    : backing_(backing), weak_factory_(this) {}

WaylandBufferHandle::~WaylandBufferHandle() {
  for (auto& cb : released_callbacks_)
    std::move(cb.second).Run(wl_buffer());
}

void WaylandBufferHandle::OnWlBufferCreated(
    wl::Object<struct wl_buffer> wl_buffer) {
  CHECK(wl_buffer) << "Failed to create wl_buffer object.";

  wl_buffer_ = std::move(wl_buffer);

  // Setup buffer release listener callbacks.
  static struct wl_buffer_listener buffer_listener = {
      &WaylandBufferHandle::BufferRelease,
  };
  if (!backing_->UseExplicitSyncRelease())
    wl_buffer_add_listener(wl_buffer_.get(), &buffer_listener, this);

  if (!created_callback_.is_null())
    std::move(created_callback_).Run();
}

void WaylandBufferHandle::OnExplicitRelease(WaylandSurface* requestor) {
  auto it = released_callbacks_.find(requestor);
  DCHECK(it != released_callbacks_.end());
  released_callbacks_.erase(it);
}

void WaylandBufferHandle::OnWlBufferRelease(struct wl_buffer* wl_buff) {
  DCHECK_EQ(wl_buff, wl_buffer_.get());
  DCHECK(!backing_->UseExplicitSyncRelease());
  DCHECK_LE(released_callbacks_.size(), 1u);

  if (!released_callbacks_.empty())
    std::move(released_callbacks_.begin()->second).Run(wl_buff);
  released_callbacks_.clear();
}

base::WeakPtr<WaylandBufferHandle> WaylandBufferHandle::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

// static
void WaylandBufferHandle::BufferRelease(void* data,
                                        struct wl_buffer* wl_buffer) {
  auto* self = static_cast<WaylandBufferHandle*>(data);
  DCHECK(self);
  self->OnWlBufferRelease(wl_buffer);
}

}  // namespace ui
