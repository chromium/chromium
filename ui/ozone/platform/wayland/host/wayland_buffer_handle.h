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
  bool released() const { return released_; }

  // Call when this buffer is ready to be re-used (i.e. giving back to viz).
  // N.B. This moves the release fence, so it is not idempotent.
  gfx::GpuFenceHandle TakeReleaseFence() { return std::move(release_fence_); }

  // Call when this buffer is attached to a surface.
  void Attached();

  // Call when this buffer is unattached from a surface.
  void Release(gfx::GpuFenceHandle release_fence);

 private:
  // Called when wl_buffer object is created.
  void OnWlBufferCreated(wl::Object<struct wl_buffer> wl_buffer);

  void OnWlBufferRelease(struct wl_buffer* wl_buffer);

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

  // Tells that the buffer has already been released aka not busy, and the
  // surface can tell the gpu about a successful swap.
  bool released_ = true;

  // Optional release fence. This may be set if the buffer is released
  // via the explicit synchronization Wayland protocol.
  gfx::GpuFenceHandle release_fence_;

  // There are two mechanisms to release buffers: release via wl_buffer, and
  // release via linux_explicit_synchronization. These both run in parallel and
  // both are sent. A wl_buffer release that comes late could be matched up with
  // the same buffer, but after it has been released via explicit sync and
  // reattached. To avoid this, keep track of how many releases from wl_buffer
  // we are expecting.
  int expected_wl_buffer_releases_ = 0;

  friend WaylandBufferBacking;

  base::WeakPtrFactory<WaylandBufferHandle> weak_factory_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_HANDLE_H_
