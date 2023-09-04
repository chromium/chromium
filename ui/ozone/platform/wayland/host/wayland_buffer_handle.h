// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_HANDLE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_HANDLE_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/native_widget_types.h"
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
  void set_buffer_released_callback(
      base::OnceCallback<void(wl_buffer*)> callback,
      WaylandSurface* requestor) {
    released_callbacks_.emplace(requestor, std::move(callback));
  }

  // Returns a weak pointer that is invalidated when the WaylandBufferHandle
  // becomes invalid to use.
  base::WeakPtr<WaylandBufferHandle> AsWeakPtr();

  uint32_t id() const { return backing_->id(); }
  gfx::Size size() const { return backing_->size(); }
  wl_buffer* buffer() const { return wl_buffer_.get(); }

  // The existence of |released_callback_| is an indicator of whether the
  // wl_buffer is released, when deciding whether wl_surface should explicitly
  // call wl_surface.attach w/ the wl_bufffer.
  bool released(WaylandSurface* surface) const {
    return released_callbacks_.find(surface) == released_callbacks_.end();
  }

  // Called when the wl_surface this buffer is attached to becomes hidden, or
  // when linux_explicit_synchronization extension is enabled to replace
  // wl_buffer.release events.
  void OnExplicitRelease(WaylandSurface* requestor);

  WaylandBufferBacking::BufferBackingType backing_type() const {
    return backing_->GetBackingType();
  }

 private:
  void OnWlBufferCreated(wl::Object<wl_buffer> wl_buffer);
  void OnWlBufferReleased(wl_buffer* wl_buffer);

  // wl_buffer_listener:
  static void OnRelease(void* data, wl_buffer* wl_buffer);

  raw_ptr<const WaylandBufferBacking> backing_;

  // A wl_buffer backed by the dmabuf/shm |backing_| created on the GPU side.
  wl::Object<wl_buffer> wl_buffer_;

  // A callback that runs when the wl_buffer is created.
  base::OnceClosure created_callback_;

  // Callbacks that binds WaylandFrameManager::OnWlBufferRelease() function,
  // which erases a pending release buffer entry from the
  // WaylandFrame::submitted_buffers, when wl_buffer.release event is signalled
  // from the wl_compositor.
  // When linux explicit synchronization is adopted, buffer_listener is unset
  // and this callback should be reset by OnExplicitRelease() instead.
  base::flat_map<WaylandSurface*, base::OnceCallback<void(wl_buffer*)>>
      released_callbacks_;

  friend WaylandBufferBacking;

  base::WeakPtrFactory<WaylandBufferHandle> weak_factory_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_HANDLE_H_
