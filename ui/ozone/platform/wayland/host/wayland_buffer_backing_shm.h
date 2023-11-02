// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_BACKING_SHM_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_BACKING_SHM_H_

#include "base/files/scoped_file.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_backing.h"

namespace ui {

// Manager of wl_buffers backed by a shm buffer.
class WaylandBufferBackingShm : public WaylandBufferBacking {
 public:
  WaylandBufferBackingShm() = delete;
  WaylandBufferBackingShm(const WaylandBufferBackingShm&) = delete;
  WaylandBufferBackingShm& operator=(const WaylandBufferBackingShm&) = delete;
  WaylandBufferBackingShm(const WaylandConnection* connection,
                          base::ScopedFD fd,
                          uint64_t length,
                          const gfx::Size& size,
                          uint32_t buffer_id);
  ~WaylandBufferBackingShm() override;

 private:
  // WaylandBufferBacking override:
  void RequestBufferHandle(
      base::OnceCallback<void(wl::Object<wl_buffer>)> callback) override;
  BufferBackingType GetBackingType() const override;

  base::ScopedFD fd_;
  const uint64_t length_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_BACKING_SHM_H_
