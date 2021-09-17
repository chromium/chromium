// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gpu_memory_buffer.h"

#include "base/logging.h"
#include "ui/gfx/generic_shared_memory_id.h"

#if defined(OS_WIN)
#include <windows.h>
#include "base/win/scoped_handle.h"
#endif

namespace gfx {

#if defined(OS_WIN)
namespace {
base::win::ScopedHandle CloneDXGIHandle(HANDLE handle) {
  HANDLE target_handle = nullptr;
  if (!::DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(),
                         &target_handle, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
    DVLOG(1) << "Error duplicating GMB DXGI handle. error=" << GetLastError();
  }
  return base::win::ScopedHandle(target_handle);
}
}  // namespace
#endif

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
  handle.io_surface = io_surface;
#elif defined(OS_WIN)
  handle.dxgi_handle = CloneDXGIHandle(dxgi_handle.Get());
#elif defined(OS_ANDROID)
  NOTIMPLEMENTED();
#endif
  return handle;
}

void GpuMemoryBuffer::SetColorSpace(const ColorSpace& color_space) {}

void GpuMemoryBuffer::SetHDRMetadata(const HDRMetadata& hdr_metadata) {}

}  // namespace gfx
