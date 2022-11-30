// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_BACKING_DMABUF_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_BACKING_DMABUF_H_

#include "base/files/scoped_file.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_backing.h"

namespace ui {

// Manager of wl_buffers backed by a dmabuf buffer.
class WaylandBufferBackingDmabuf : public WaylandBufferBacking {
 public:
  WaylandBufferBackingDmabuf() = delete;
  WaylandBufferBackingDmabuf(const WaylandBufferBackingDmabuf&) = delete;
  WaylandBufferBackingDmabuf& operator=(const WaylandBufferBackingDmabuf&) =
      delete;
  WaylandBufferBackingDmabuf(const WaylandConnection* connection,
                             base::ScopedFD fd,
                             const gfx::Size& size,
                             std::vector<uint32_t> strides,
                             std::vector<uint32_t> offsets,
                             std::vector<uint64_t> modifiers,
                             uint32_t format,
                             uint32_t planes_count,
                             uint32_t buffer_id);
  ~WaylandBufferBackingDmabuf() override;

 private:
  // WaylandBufferBacking override:
  void RequestBufferHandle(
      base::OnceCallback<void(wl::Object<wl_buffer>)> callback) override;
  BufferBackingType GetBackingType() const override;

  base::ScopedFD fd_;
  const std::vector<uint32_t> strides_;
  const std::vector<uint32_t> offsets_;
  const std::vector<uint64_t> modifiers_;
  const uint32_t planes_count_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_BACKING_DMABUF_H_
