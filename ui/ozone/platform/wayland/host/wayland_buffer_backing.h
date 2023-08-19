// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_BACKING_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_BACKING_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "drm_fourcc.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class WaylandSurface;
class WaylandBufferHandle;
class WaylandConnection;

// This represents a dmabuf/shm buffer, which the GPU process creates when
// CreateBuffer is called. It's used for asynchronous buffer creation. It
// dynamically requests wl_buffer objects when a buffer should be attached to a
// wl_surface.
//
// Has one-to-many relationship with WaylandBufferHandle.
class WaylandBufferBacking {
 public:
  enum class BufferBackingType {
    kShm = 0,
    kDmabuf = 1,
    kSolidColor = 2,
    kSinglePixel = 3,
  };

  WaylandBufferBacking() = delete;
  WaylandBufferBacking(const WaylandBufferBacking&) = delete;
  WaylandBufferBacking& operator=(const WaylandBufferBacking&) = delete;
  WaylandBufferBacking(const WaylandConnection* connection,
                       uint32_t buffer_id,
                       const gfx::Size& size,
                       uint32_t format = DRM_FORMAT_INVALID);
  virtual ~WaylandBufferBacking();

  const WaylandConnection* connection() const { return connection_; }
  uint32_t format() const { return format_; }
  uint32_t id() const { return buffer_id_; }
  gfx::Size size() const { return size_; }

  // Whether linux_explicit_synchronization extension is enabled. It is an
  // extension that completely replaces base protocol's wl_buffer.release
  // events.
  bool UseExplicitSyncRelease() const;

  // Returns a wl_buffer wrapper that can be attached to the |requestor|.
  // Requests a new wl_buffer if such a wl_buffer does not exist.
  WaylandBufferHandle* EnsureBufferHandle(WaylandSurface* requestor = nullptr);

  // Same as above but does not do the requesting.
  WaylandBufferHandle* GetBufferHandle(WaylandSurface* requestor);

  // Returns type of the backing. See BufferBackingType.
  virtual BufferBackingType GetBackingType() const = 0;

 private:
  // Non-owned pointer to the main connection.
  raw_ptr<const WaylandConnection> connection_;

  // DRM buffer format if specified, otherwise DRM_FORMAT_INVALID (0)
  const uint32_t format_;

  // Requests a new wl_buffer. |callback| will be run with the created wl_buffer
  // object when creation is complete.
  virtual void RequestBufferHandle(
      base::OnceCallback<void(wl::Object<wl_buffer>)> callback) = 0;

  // Use |kInvalidBufferId| to commit surface state without updating wl_buffer.
  constexpr static uint32_t kInvalidBufferId = 0u;

  // The id of this buffer.
  const uint32_t buffer_id_;

  // Actual buffer size in pixels.
  const gfx::Size size_;

  // Collection of wl_buffers objects backed by this backing. Maintains the
  // relationship of wl_surfaces to wl_buffers, corresponding to the
  // wl_surface.attach(wl_buffer) request. |buffer_handles_[nullptr]| represents
  // an anonymous WaylandBufferHandle not attached to any wl_surface yet.
  base::flat_map<WaylandSurface*, std::unique_ptr<WaylandBufferHandle>>
      buffer_handles_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_BACKING_H_
