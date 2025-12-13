// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_HANDLE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_HANDLE_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {
class WaylandConnection;
class WaylandBufferBacking;
class WaylandSurface;
class WaylandSyncobjReleaseTimeline;

// This is a wrapper of a wl_buffer. Instances of this class are managed by the
// corresponding WaylandBufferBackings.
class WaylandBufferHandle {
 public:
  enum class SyncMethod {
    // No acquire or release needed
    kNone = 0,
    // Attach and release are treated as ready to use/reuse
    kImplicit = 1,
    // Using DRM syncobj timeline
    kSyncobj = 2,
    // Using fence packaged in dmabuf, should not co-exist with kSyncObj
    kDMAFence = 3,
  };

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
      base::OnceCallback<void(wl_buffer*, bool)> callback,
      WaylandSurface* requestor) {
    released_callbacks_.emplace(requestor, std::move(callback));
  }

  // Returns a weak pointer that is invalidated when the WaylandBufferHandle
  // becomes invalid to use.
  base::WeakPtr<WaylandBufferHandle> AsWeakPtr();

  uint32_t id() const { return id_; }
  gfx::Size size() const { return size_; }
  wl_buffer* buffer() const { return wl_buffer_.get(); }
  SyncMethod sync_method() const { return sync_method_; }
  WaylandSyncobjReleaseTimeline* release_timeline() const {
    return release_timeline_.get();
  }

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

 private:
  void OnWlBufferCreated(wl::Object<wl_buffer> wl_buffer);
  void OnWlBufferReleased(wl_buffer* wl_buffer);

  // wl_buffer_listener:
  static void OnRelease(void* data, wl_buffer* wl_buffer);

  // Non-owned pointer to the main connection.
  raw_ptr<const WaylandConnection> connection_;

  // The id of this buffer.
  const uint32_t id_;

  // Actual buffer size in pixels.
  const gfx::Size size_;

  // A wl_buffer backed by the dmabuf/shm |backing_| created on the GPU side.
  wl::Object<wl_buffer> wl_buffer_;

  // How synchronization of this buffer is managed.
  const SyncMethod sync_method_;

  // A callback that runs when the wl_buffer is created.
  base::OnceClosure created_callback_;

  // Used for explicit sync using linux-drm-syncobj.
  // As per the protocol [1], reusing a timeline for multiple buffers is not
  // safe because the buffers could end up being released out of order. So we
  // have release timelines on a per-buffer basis.
  // In the case of explicit sync there is only one buffer per backing
  // so it is safe to put it here instead of WaylandBufferBacking.
  //
  // [1]
  // https://wayland.app/protocols/linux-drm-syncobj-v1#wp_linux_drm_syncobj_surface_v1:request:set_release_point
  std::unique_ptr<WaylandSyncobjReleaseTimeline> release_timeline_;

  // Callbacks that binds WaylandFrameManager::OnWlBufferRelease() function,
  // which erases a pending release buffer entry from the
  // WaylandFrame::submitted_buffers, when wl_buffer.release event is signalled
  // from the wl_compositor.
  // When linux explicit synchronization is adopted, buffer_listener is unset
  // and this callback should be reset by OnExplicitRelease() instead.
  base::flat_map<WaylandSurface*, base::OnceCallback<void(wl_buffer*, bool)>>
      released_callbacks_;

  friend WaylandBufferBacking;

  base::WeakPtrFactory<WaylandBufferHandle> weak_factory_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_HANDLE_H_
