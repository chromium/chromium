// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gpu_memory_buffer_handle.h"

#include "base/logging.h"
#include "base/notimplemented.h"
#include "build/build_config.h"
#include "ui/gfx/generic_shared_memory_id.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/scoped_handle.h"
#endif  // BUILDFLAG(IS_WIN)

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
  CHECK(region_.IsValid(), base::NotFatalUntil::M147);
}

#if BUILDFLAG(IS_WIN)
GpuMemoryBufferHandle::GpuMemoryBufferHandle(DXGIHandle handle)
    : type(GpuMemoryBufferType::DXGI_SHARED_HANDLE),
      dxgi_handle_(std::move(handle)) {
  CHECK(dxgi_handle_.IsValid());
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_OZONE)
GpuMemoryBufferHandle::GpuMemoryBufferHandle(
    NativePixmapHandle native_pixmap_handle)
    : type(GpuMemoryBufferType::NATIVE_PIXMAP),
      native_pixmap_handle_(std::move(native_pixmap_handle)) {}
#endif  // BUILDFLAG(IS_OZONE)

#if BUILDFLAG(IS_ANDROID)
GpuMemoryBufferHandle::GpuMemoryBufferHandle(
    base::android::ScopedHardwareBufferHandle handle)
    : type(GpuMemoryBufferType::ANDROID_HARDWARE_BUFFER),
      android_hardware_buffer(std::move(handle)) {}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_APPLE)
GpuMemoryBufferHandle::GpuMemoryBufferHandle(ScopedIOSurface io_surface)
    : type(GpuMemoryBufferType::IO_SURFACE_BUFFER),
      io_surface_(std::move(io_surface)) {
  CHECK(io_surface_);
#if BUILDFLAG(IS_IOS)
  io_surface_mach_port_.reset(IOSurfaceCreateMachPort(io_surface_.get()));
  ExportIOSurfaceSharedMemoryRegion(
      io_surface_.get(), io_surface_shared_memory_region_,
      io_surface_plane_strides_, io_surface_plane_offsets_);
#endif  // BUILDFLAG(IS_IOS)
}
#endif  // BUILDFLAG(IS_APPLE)

// TODO(crbug.com/40584691): Reset |type| and possibly the handles on the
// moved-from object.
GpuMemoryBufferHandle::GpuMemoryBufferHandle(GpuMemoryBufferHandle&& other) =
    default;

GpuMemoryBufferHandle& GpuMemoryBufferHandle::operator=(
    GpuMemoryBufferHandle&& other) = default;

GpuMemoryBufferHandle::~GpuMemoryBufferHandle() = default;

GpuMemoryBufferHandle GpuMemoryBufferHandle::Clone() const {
  GpuMemoryBufferHandle handle;
  handle.type = type;
  handle.offset = offset;
  handle.stride = stride;
#if BUILDFLAG(IS_OZONE)
  handle.native_pixmap_handle_ = CloneHandleForIPC(native_pixmap_handle_);
#elif BUILDFLAG(IS_APPLE)
  handle.io_surface_ = io_surface_;
#if BUILDFLAG(IS_IOS)
  handle.io_surface_mach_port_ = io_surface_mach_port_;
  handle.io_surface_shared_memory_region_ =
      io_surface_shared_memory_region_.Duplicate();
  handle.io_surface_plane_strides_ = io_surface_plane_strides_;
  handle.io_surface_plane_offsets_ = io_surface_plane_offsets_;
#endif
#elif BUILDFLAG(IS_WIN)
  handle.dxgi_handle_ = dxgi_handle_.Clone();
#elif BUILDFLAG(IS_ANDROID)
  if (android_hardware_buffer.is_valid()) {
    handle.android_hardware_buffer = android_hardware_buffer.Clone();
  }
#endif
  handle.region_ = region_.Duplicate();
  return handle;
}

}  // namespace gfx
