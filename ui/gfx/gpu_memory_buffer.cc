// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gpu_memory_buffer.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "ui/gfx/generic_shared_memory_id.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/scoped_handle.h"
#endif

namespace gfx {

#if BUILDFLAG(IS_WIN)
namespace {
base::win::ScopedHandle CloneDXGIHandle(HANDLE handle) {
  HANDLE target_handle = nullptr;
  if (!::DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(),
                         &target_handle, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
    DVPLOG(1) << "Error duplicating GMB DXGI handle";
  }
  return base::win::ScopedHandle(target_handle);
}
}  // namespace
#endif

GpuMemoryBufferHandle::GpuMemoryBufferHandle() = default;

#if BUILDFLAG(IS_ANDROID)
GpuMemoryBufferHandle::GpuMemoryBufferHandle(
    base::android::ScopedHardwareBufferHandle handle)
    : type(GpuMemoryBufferType::ANDROID_HARDWARE_BUFFER),
      android_hardware_buffer(std::move(handle)) {}
#endif

// TODO(crbug.com/40584691): Reset |type| and possibly the handles on the
// moved-from object.
GpuMemoryBufferHandle::GpuMemoryBufferHandle(GpuMemoryBufferHandle&& other) =
    default;

GpuMemoryBufferHandle& GpuMemoryBufferHandle::operator=(
    GpuMemoryBufferHandle&& other) = default;

GpuMemoryBufferHandle::~GpuMemoryBufferHandle() = default;

void GpuMemoryBuffer::MapAsync(base::OnceCallback<void(bool)> result_cb) {
  std::move(result_cb).Run(Map());
}

bool GpuMemoryBuffer::AsyncMappingIsNonBlocking() const {
  return false;
}

GpuMemoryBufferHandle GpuMemoryBufferHandle::Clone() const {
  GpuMemoryBufferHandle handle;
  handle.type = type;
  handle.id = id;
  handle.region = region.Duplicate();
  handle.offset = offset;
  handle.stride = stride;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
  handle.native_pixmap_handle = CloneHandleForIPC(native_pixmap_handle);
#elif BUILDFLAG(IS_APPLE)
  handle.io_surface = io_surface;
#elif BUILDFLAG(IS_WIN)
  if (dxgi_handle.is_valid()) {
    handle.dxgi_handle = CloneDXGIHandle(dxgi_handle.Get());
  }
  handle.dxgi_token = dxgi_token;
#elif BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED();
#endif
  return handle;
}

}  // namespace gfx
