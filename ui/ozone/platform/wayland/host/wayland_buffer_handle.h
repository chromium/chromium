// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_HANDLE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_HANDLE_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_backing.h"

namespace ui {

// This is a wrapper of a wl_buffer. Instances of this class are managed by the
// corresponding WaylandBufferBackings.
class WaylandBufferHandle {
 public:
  WaylandBufferHandle() = delete;
  WaylandBufferHandle(const WaylandBufferHandle&) = delete;
  WaylandBufferHandle& operator=(const WaylandBufferHandle&) = delete;
  explicit WaylandBufferHandle(WaylandBufferBacking* backing);
  ~WaylandBufferHandle();

  void set_buffer_created_callback(base::OnceClosure callback) {
    DCHECK(created_callback_.is_null());
    created_callback_ = std::move(callback);
  }
  void set_buffer_released_callback(base::OnceClosure callback) {
    released_callback_ = std::move(callback);
  }

  // Returns a weak pointer that is invalidated when the WaylandBufferHandle
  // becomes invalid to use.
  base::WeakPtr<WaylandBufferHandle> AsWeakPtr();

  uint32_t id() const { return backing_->id(); }
  gfx::Size size() const { return backing_->size(); }
  struct wl_buffer* wl_buffer() const {
    return wl_buffer_.get();
  }

  // Tells if the buffer has already been released aka not busy, and the
  // surface can tell the gpu about successful swap.
  bool released = true;

  // Optional release fence. This may be set if the buffer is released
  // via the explicit synchronization Wayland protocol.
  gfx::GpuFenceHandle release_fence;

 private:
  // Called when wl_buffer object is created.
  void OnWlBufferCreated(wl::Object<struct wl_buffer> wl_buffer);

  void OnRelease(struct wl_buffer* wl_buffer);

  // wl_buffer_listener:
  static void BufferRelease(void* data, struct wl_buffer* wl_buffer);

  const WaylandBufferBacking* backing_;

  // A wl_buffer backed by the dmabuf/shm |backing_| created on the GPU side.
  wl::Object<struct wl_buffer> wl_buffer_;

  // A callback that runs when the wl_buffer is created.
  base::OnceClosure created_callback_;
  // A callback that runs when the wl_buffer is released by the Wayland
  // compositor.
  base::OnceClosure released_callback_;

  friend WaylandBufferBacking;

  base::WeakPtrFactory<WaylandBufferHandle> weak_factory_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_HANDLE_H_
