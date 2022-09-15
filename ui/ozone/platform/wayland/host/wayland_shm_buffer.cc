// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_shm_buffer.h"

#include "base/logging.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_factory.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

WaylandShmBuffer::WaylandShmBuffer(WaylandBufferFactory* buffer_factory,
                                   const gfx::Size& size)
    : size_(size) {
  Initialize(buffer_factory);
}

WaylandShmBuffer::~WaylandShmBuffer() = default;
WaylandShmBuffer::WaylandShmBuffer(WaylandShmBuffer&& buffer) = default;
WaylandShmBuffer& WaylandShmBuffer::operator=(WaylandShmBuffer&& buffer) =
    default;

void WaylandShmBuffer::Initialize(WaylandBufferFactory* buffer_factory) {
  DCHECK(buffer_factory);

  SkImageInfo info = SkImageInfo::MakeN32Premul(size_.width(), size_.height());
  int stride = info.minRowBytes();

  size_t buffer_size = info.computeByteSize(stride);
  if (buffer_size == SIZE_MAX)
    return;

  base::UnsafeSharedMemoryRegion shared_memory_region =
      base::UnsafeSharedMemoryRegion::Create(buffer_size);
  shared_memory_mapping_ = shared_memory_region.Map();
  if (!shared_memory_mapping_.IsValid()) {
    PLOG(ERROR) << "Create and mmap failed.";
    return;
  }
  base::subtle::PlatformSharedMemoryRegion platform_shared_memory =
      base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
          std::move(shared_memory_region));

  base::subtle::ScopedFDPair fd_pair =
      platform_shared_memory.PassPlatformHandle();
  buffer_ = buffer_factory->CreateShmBuffer(std::move(fd_pair.fd), buffer_size,
                                            size_);
  if (!buffer_) {
    shared_memory_mapping_ = base::WritableSharedMemoryMapping();
    return;
  }

  stride_ = stride;
}

uint8_t* WaylandShmBuffer::GetMemory() const {
  if (!IsValid() || !shared_memory_mapping_.IsValid())
    return nullptr;
  return shared_memory_mapping_.GetMemoryAs<uint8_t>();
}

}  // namespace ui
