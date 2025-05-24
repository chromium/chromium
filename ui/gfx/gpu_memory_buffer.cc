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

DXGIHandle DXGIHandle::CreateFakeForTest() {
  // DXGIHandle requires a valid HANDLE, so just create a placeholder event.
  base::win::ScopedHandle fake_handle(
      ::CreateEvent(nullptr, FALSE, FALSE, nullptr));
  return DXGIHandle(std::move(fake_handle));
}

DXGIHandle::DXGIHandle() = default;
DXGIHandle::~DXGIHandle() = default;

DXGIHandle::DXGIHandle(base::win::ScopedHandle buffer_handle)
    : buffer_handle_(std::move(buffer_handle)) {
  DCHECK(buffer_handle_.is_valid());
}

DXGIHandle::DXGIHandle(base::win::ScopedHandle buffer_handle,
                       const DXGIHandleToken& token,
                       base::UnsafeSharedMemoryRegion region)
    : buffer_handle_(std::move(buffer_handle)),
      token_(token),
      region_(std::move(region)) {
  DCHECK(buffer_handle_.is_valid());
}

DXGIHandle::DXGIHandle(DXGIHandle&&) = default;
DXGIHandle& DXGIHandle::operator=(DXGIHandle&&) = default;

bool DXGIHandle::IsValid() const {
  return buffer_handle_.is_valid();
}

DXGIHandle DXGIHandle::Clone() const {
  DXGIHandle handle;
  if (buffer_handle_.is_valid()) {
    handle.buffer_handle_ = CloneDXGIHandle(buffer_handle_.Get());
  }
  handle.token_ = token_;
  handle.region_ = region_.Duplicate();
  return handle;
}

DXGIHandle DXGIHandle::CloneWithRegion(
    base::UnsafeSharedMemoryRegion region) const {
  DXGIHandle handle = Clone();
  DCHECK(!handle.region_.IsValid());
  handle.region_ = std::move(region);
  return handle;
}

base::win::ScopedHandle DXGIHandle::TakeBufferHandle() {
  DCHECK(buffer_handle_.is_valid());
  return std::move(buffer_handle_);
}
#endif  // BUILDFLAG(IS_WIN)

GpuMemoryBufferHandle::GpuMemoryBufferHandle() = default;

GpuMemoryBufferHandle::GpuMemoryBufferHandle(
    base::UnsafeSharedMemoryRegion region)
    : type(GpuMemoryBufferType::SHARED_MEMORY_BUFFER),
      region_(std::move(region)) {
  CHECK(region_.IsValid(), base::NotFatalUntil::M141);
}

#if BUILDFLAG(IS_WIN)
GpuMemoryBufferHandle::GpuMemoryBufferHandle(DXGIHandle handle)
    : type(GpuMemoryBufferType::DXGI_SHARED_HANDLE),
      dxgi_handle_(std::move(handle)) {
  CHECK(dxgi_handle_.IsValid());
}
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
GpuMemoryBufferHandle::GpuMemoryBufferHandle(
    NativePixmapHandle native_pixmap_handle)
    : type(GpuMemoryBufferType::NATIVE_PIXMAP),
      native_pixmap_handle_(std::move(native_pixmap_handle)) {}
#endif

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
  handle.offset = offset;
  handle.stride = stride;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
  handle.native_pixmap_handle_ = CloneHandleForIPC(native_pixmap_handle_);
#elif BUILDFLAG(IS_APPLE)
  handle.io_surface = io_surface;
#elif BUILDFLAG(IS_WIN)
  handle.dxgi_handle_ = dxgi_handle_.Clone();
#elif BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED();
#endif
  handle.region_ = region_.Duplicate();
  return handle;
}

}  // namespace gfx
