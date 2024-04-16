// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_FACTORY_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_FACTORY_H_

#include <vector>

#include "base/files/scoped_file.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_shm.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace ui {

class WaylandDrm;
class WaylandZwpLinuxDmabuf;

// A factory that wraps different wayland objects that are able to create
// wl_buffers.
class WaylandBufferFactory {
 public:
  WaylandBufferFactory();
  WaylandBufferFactory(const WaylandBufferFactory&) = delete;
  WaylandBufferFactory& operator=(const WaylandBufferFactory&) = delete;
  ~WaylandBufferFactory();

  // Requests to create a wl_buffer backed by the dmabuf prime |fd| descriptor.
  // The result is sent back via the |callback|. If buffer creation failed,
  // nullptr is sent back via the callback. Otherwise, a pointer to the
  // |wl_buffer| is sent. Depending on the result of |CanCreateDmabufImmed|,
  // a buffer can be created immediately which means the callback will be fired
  // immediately and the client will not have to wait until the buffer is
  // created.
  void CreateDmabufBuffer(const base::ScopedFD& fd,
                          const gfx::Size& size,
                          const std::vector<uint32_t>& strides,
                          const std::vector<uint32_t>& offsets,
                          const std::vector<uint64_t>& modifiers,
                          uint32_t format,
                          uint32_t planes_count,
                          wl::OnRequestBufferCallback callback) const;

  // Creates a wl_buffer based on shared memory handle with the specified
  // |length| and |size|. Whereas |with_alpha_channel| indicates whether the
  // buffer's color format should use or not the alpha channel.
  //
  // TODO(crbug.com/40204912): Remove |with_alpha_channel| parameter once
  // Exo-side Skia Renderer issue is fixed.
  wl::Object<struct wl_buffer> CreateShmBuffer(
      const base::ScopedFD& fd,
      size_t length,
      const gfx::Size& size,
      bool with_alpha_channel = true) const;

  // Returns supported buffer formats received from the Wayland compositor.
  wl::BufferFormatsWithModifiersMap GetSupportedBufferFormats() const;

  // Returns true if dmabuf is supported.
  bool SupportsDmabuf() const;

  // Returns true if a dmabuf buffer can be created immediately. If not, a
  // dmabuf backed buffer is created asynchronously.
  bool CanCreateDmabufImmed() const;

  // Returns wl_shm. This has to be unfortunately exposed as
  // WaylandCursorFactory uses wl_cursor_theme_load to load a cursor theme,
  // which requires to pass the wl_shm object as a parameter when called.
  wl_shm* shm() const { return wayland_shm_ ? wayland_shm_->get() : nullptr; }

 private:
  // Exposed so that globals are able to create these objects when exist.
  friend class WaylandDrm;
  friend class WaylandShm;
  friend class WaylandZwpLinuxDmabuf;

  // A wrapper around wl_drm.
  std::unique_ptr<WaylandDrm> wayland_drm_;
  // A wrapper around wl_shm.
  std::unique_ptr<WaylandShm> wayland_shm_;
  // A wrapper around zwp_linux_dmabuf.
  std::unique_ptr<WaylandZwpLinuxDmabuf> wayland_zwp_dmabuf_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_FACTORY_H_
