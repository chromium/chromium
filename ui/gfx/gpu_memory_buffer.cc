// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gpu_memory_buffer.h"

#include "ui/gfx/generic_shared_memory_id.h"

namespace gfx {

GpuMemoryBufferHandle::GpuMemoryBufferHandle() = default;

#if defined(OS_ANDROID)
GpuMemoryBufferHandle::GpuMemoryBufferHandle(
    base::android::ScopedHardwareBufferHandle handle)
    : type(GpuMemoryBufferType::ANDROID_HARDWARE_BUFFER),
      android_hardware_buffer(std::move(handle)) {}
#endif

// TODO(crbug.com/863011): Reset |type| and possibly the handles on the
// moved-from object.
GpuMemoryBufferHandle::GpuMemoryBufferHandle(GpuMemoryBufferHandle&& other) =
    default;

GpuMemoryBufferHandle& GpuMemoryBufferHandle::operator=(
    GpuMemoryBufferHandle&& other) = default;

GpuMemoryBufferHandle::~GpuMemoryBufferHandle() = default;

GpuMemoryBufferHandle GpuMemoryBufferHandle::Clone() const {
  GpuMemoryBufferHandle handle;
  handle.type = type;
  handle.id = id;
  handle.region = region.Duplicate();
  handle.offset = offset;
  handle.stride = stride;
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_FUCHSIA)
  handle.native_pixmap_handle = CloneHandleForIPC(native_pixmap_handle);
#elif defined(OS_MAC)
  handle.mach_port = mach_port;
#elif defined(OS_WIN)
  NOTIMPLEMENTED();
#elif defined(OS_ANDROID)
  NOTIMPLEMENTED();
#endif
  return handle;
}

void GpuMemoryBuffer::SetColorSpace(const gfx::ColorSpace& color_space) {}

}  // namespace gfx
