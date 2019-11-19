// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SHM_BUFFER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SHM_BUFFER_H_

#include <wayland-client.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/shared_memory_mapping.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class WaylandShm;

// Encapsulates a Wayland SHM buffer, covering basically 2 use cases:
// (1) Buffers created and mmap'ed locally to draw skia bitmap(s) into; and
// (2) Buffers created using file descriptor (e.g: sent by gpu process/thread,
// through IPC), not mapped in local memory address space.
// WaylandShmBuffer is moveable, non-copyable, and is assumed to own both
// wl_buffer and WritableSharedMemoryMapping (if any) instance.
class WaylandShmBuffer {
 public:
  WaylandShmBuffer(WaylandShm* shm, const gfx::Size& size);
  ~WaylandShmBuffer();

  WaylandShmBuffer(WaylandShmBuffer&& buffer);
  WaylandShmBuffer& operator=(WaylandShmBuffer&& buffer);

  // Buffer is valid if it has been successfully created (and mapped, depending
  // on the constructor used).
  bool IsValid() const { return !!buffer_; }

  // Returns the underlying raw memory buffer, if it's currently mapped into
  // local address space, otherwise return nullptr
  uint8_t* GetMemory() const;

  // Returns the underlying wl_buffer pointer
  wl_buffer* get() const { return buffer_.get(); }

  // Returns the Size used to create this buffer
  const gfx::Size& size() const { return size_; }

  int stride() const { return stride_; }

 private:
  void Initialize(WaylandShm* shm);

  gfx::Size size_;
  int stride_;
  wl::Object<wl_buffer> buffer_;
  base::WritableSharedMemoryMapping shared_memory_mapping_;

  DISALLOW_COPY_AND_ASSIGN(WaylandShmBuffer);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_COMMON_WAYLAND_SHM_BUFFER_H_
