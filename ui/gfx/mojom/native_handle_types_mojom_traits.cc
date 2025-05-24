// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mojom/native_handle_types_mojom_traits.h"

#include "base/notreached.h"
#include "build/build_config.h"
#include "mojo/public/cpp/base/shared_memory_mojom_traits.h"
#include "mojo/public/cpp/platform/platform_handle.h"

#if BUILDFLAG(IS_APPLE)
#include "base/apple/scoped_mach_port.h"
#include "ui/gfx/mac/io_surface.h"
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_OZONE)
#include "ui/gfx/native_pixmap_handle.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_OZONE)

#if BUILDFLAG(IS_WIN)
#include "base/memory/unsafe_shared_memory_region.h"
#include "ui/gfx/gpu_memory_buffer.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_hardware_buffer_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/scope_to_message_pipe.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace mojo {

#if BUILDFLAG(IS_ANDROID)
// static
PlatformHandle StructTraits<gfx::mojom::AHardwareBufferHandleDataView,
                            ::base::android::ScopedHardwareBufferHandle>::
    buffer_handle(::base::android::ScopedHardwareBufferHandle& handle) {
  return PlatformHandle(handle.SerializeAsFileDescriptor());
}

// static
ScopedMessagePipeHandle
StructTraits<gfx::mojom::AHardwareBufferHandleDataView,
             ::base::android::ScopedHardwareBufferHandle>::
    tracking_pipe(::base::android::ScopedHardwareBufferHandle& handle) {
  // We must keep a ref to the AHardwareBuffer alive until the receiver has
  // acquired its own reference. We do this by sending a message pipe handle
  // along with the buffer. When the receiver deserializes (or even if they
  // die without ever reading the message) their end of the pipe will be
  // closed. We will eventually detect this and release the AHB reference.
  mojo::MessagePipe tracking_pipe;
  // Pass ownership of the input handle to our tracking pipe to keep the AHB
  // alive until it's deserialized.
  //
  // SUBTLE: Both `buffer_handle` and `tracking_pipe` use `handle`, but the line
  // below consumes `handle` by tying its lifetime to the message pipe. This is
  // not a use-after-move, but it depends on internal details of Mojo
  // serialization; specifically, the fact that struct fields are serialized in
  // ordinal order. Since `buffer_handle` is declared before `tracking_pipe`,
  // and neither has an explicit ordinal, Mojo will always serialize
  // `buffer_handle` before `tracking_pipe`.
  mojo::ScopeToMessagePipe(std::move(handle), std::move(tracking_pipe.handle0));
  return std::move(tracking_pipe.handle1);
}

// static
bool StructTraits<gfx::mojom::AHardwareBufferHandleDataView,
                  ::base::android::ScopedHardwareBufferHandle>::
    Read(gfx::mojom::AHardwareBufferHandleDataView data,
         base::android::ScopedHardwareBufferHandle* handle) {
  base::ScopedFD scoped_fd = data.TakeBufferHandle().TakeFD();
  if (!scoped_fd.is_valid()) {
    return false;
  }
  *handle =
      base::android::ScopedHardwareBufferHandle::DeserializeFromFileDescriptor(
          std::move(scoped_fd));
  return handle->is_valid();
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_OZONE)
mojo::PlatformHandle StructTraits<
    gfx::mojom::NativePixmapPlaneDataView,
    gfx::NativePixmapPlane>::buffer_handle(gfx::NativePixmapPlane& plane) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return mojo::PlatformHandle(std::move(plane.fd));
#elif BUILDFLAG(IS_FUCHSIA)
  return mojo::PlatformHandle(std::move(plane.vmo));
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
}

bool StructTraits<
    gfx::mojom::NativePixmapPlaneDataView,
    gfx::NativePixmapPlane>::Read(gfx::mojom::NativePixmapPlaneDataView data,
                                  gfx::NativePixmapPlane* out) {
  out->stride = data.stride();
  out->offset = data.offset();
  out->size = data.size();

  mojo::PlatformHandle handle = data.TakeBufferHandle();
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (!handle.is_fd())
    return false;
  out->fd = handle.TakeFD();
#elif BUILDFLAG(IS_FUCHSIA)
  if (!handle.is_handle())
    return false;
  out->vmo = zx::vmo(handle.TakeHandle());
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  return true;
}

#if BUILDFLAG(IS_FUCHSIA)
PlatformHandle
StructTraits<gfx::mojom::NativePixmapHandleDataView, gfx::NativePixmapHandle>::
    buffer_collection_handle(gfx::NativePixmapHandle& pixmap_handle) {
  return mojo::PlatformHandle(
      std::move(pixmap_handle.buffer_collection_handle));
}
#endif  // BUILDFLAG(IS_FUCHSIA)

bool StructTraits<
    gfx::mojom::NativePixmapHandleDataView,
    gfx::NativePixmapHandle>::Read(gfx::mojom::NativePixmapHandleDataView data,
                                   gfx::NativePixmapHandle* out) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  out->modifier = data.modifier();
  out->supports_zero_copy_webgpu_import =
      data.supports_zero_copy_webgpu_import();
#endif

#if BUILDFLAG(IS_FUCHSIA)
  mojo::PlatformHandle handle = data.TakeBufferCollectionHandle();
  if (!handle.is_handle())
    return false;
  out->buffer_collection_handle = zx::eventpair(handle.TakeHandle());
  out->buffer_index = data.buffer_index();
  out->ram_coherency = data.ram_coherency();
#endif

  return data.ReadPlanes(&out->planes);
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_OZONE)

#if BUILDFLAG(IS_WIN)
bool StructTraits<gfx::mojom::DXGIHandleDataView, gfx::DXGIHandle>::Read(
    gfx::mojom::DXGIHandleDataView data,
    gfx::DXGIHandle* handle) {
  base::win::ScopedHandle buffer_handle = data.TakeBufferHandle().TakeHandle();
  gfx::DXGIHandleToken token;
  if (!data.ReadToken(&token)) {
    return false;
  }
  base::UnsafeSharedMemoryRegion region;
  if (!data.ReadSharedMemoryHandle(&region)) {
    return false;
  }
  *handle = gfx::DXGIHandle(std::move(buffer_handle), token, std::move(region));
  DCHECK(handle->IsValid());
  return true;
}

bool StructTraits<gfx::mojom::DXGIHandleTokenDataView, gfx::DXGIHandleToken>::
    Read(gfx::mojom::DXGIHandleTokenDataView& input,
         gfx::DXGIHandleToken* output) {
  base::UnguessableToken token;
  if (!input.ReadValue(&token))
    return false;
  *output = gfx::DXGIHandleToken(token);
  return true;
}
#endif  // BUILDFLAG(IS_WIN)

gfx::mojom::GpuMemoryBufferPlatformHandleDataView::Tag UnionTraits<
    gfx::mojom::GpuMemoryBufferPlatformHandleDataView,
    gfx::GpuMemoryBufferHandle>::GetTag(const gfx::GpuMemoryBufferHandle&
                                            handle) {
  switch (handle.type) {
    case gfx::EMPTY_BUFFER:
      NOTREACHED();  // Handled by `IsNull()` and should never reach here.
    case gfx::SHARED_MEMORY_BUFFER:
      return Tag::kSharedMemoryHandle;
#if BUILDFLAG(IS_APPLE)
    case gfx::IO_SURFACE_BUFFER:
      return Tag::kMachPort;
#endif  // BUILDFLAG(IS_APPLE)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_OZONE)
    case gfx::NATIVE_PIXMAP:
      return Tag::kNativePixmapHandle;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_OZONE)
#if BUILDFLAG(IS_WIN)
    case gfx::DXGI_SHARED_HANDLE:
      return Tag::kDxgiHandle;
#endif
#if BUILDFLAG(IS_ANDROID)
    case gfx::ANDROID_HARDWARE_BUFFER:
      return Tag::kAndroidHardwareBufferHandle;
#endif  // BUILDFLAG(IS_ANDROID)
  }
  NOTREACHED();
}

bool UnionTraits<gfx::mojom::GpuMemoryBufferPlatformHandleDataView,
                 gfx::GpuMemoryBufferHandle>::
    IsNull(const gfx::GpuMemoryBufferHandle& handle) {
  return handle.type == gfx::EMPTY_BUFFER;
}

void UnionTraits<
    gfx::mojom::GpuMemoryBufferPlatformHandleDataView,
    gfx::GpuMemoryBufferHandle>::SetToNull(gfx::GpuMemoryBufferHandle* handle) {
  handle->type = gfx::EMPTY_BUFFER;
}

#if BUILDFLAG(IS_APPLE)
PlatformHandle UnionTraits<
    gfx::mojom::GpuMemoryBufferPlatformHandleDataView,
    gfx::GpuMemoryBufferHandle>::mach_port(gfx::GpuMemoryBufferHandle& handle) {
  gfx::ScopedRefCountedIOSurfaceMachPort io_surface_mach_port(
      IOSurfaceCreateMachPort(handle.io_surface.get()));
  return PlatformHandle(
      base::apple::RetainMachSendRight(io_surface_mach_port.get()));
}
#endif  // BUILDFLAG(IS_APPLE)

bool UnionTraits<gfx::mojom::GpuMemoryBufferPlatformHandleDataView,
                 gfx::GpuMemoryBufferHandle>::
    Read(gfx::mojom::GpuMemoryBufferPlatformHandleDataView data,
         gfx::GpuMemoryBufferHandle* handle) {
  switch (data.tag()) {
    case Tag::kSharedMemoryHandle:
      handle->type = gfx::SHARED_MEMORY_BUFFER;
      return data.ReadSharedMemoryHandle(&handle->region_);
#if BUILDFLAG(IS_APPLE)
    case Tag::kMachPort:
      handle->type = gfx::IO_SURFACE_BUFFER;
      PlatformHandle mach_port = data.TakeMachPort();
      if (!mach_port.is_mach_send()) {
        return false;
      }
      gfx::ScopedRefCountedIOSurfaceMachPort io_surface_mach_port(
          mach_port.ReleaseMachSendRight());
      if (io_surface_mach_port) {
        handle->io_surface.reset(
            IOSurfaceLookupFromMachPort(io_surface_mach_port.get()));
      } else {
        handle->io_surface.reset();
      }
      return true;
#endif  // BUILDFLAG(IS_APPLE)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_OZONE)
    case Tag::kNativePixmapHandle:
      handle->type = gfx::NATIVE_PIXMAP;
      return data.ReadNativePixmapHandle(&handle->native_pixmap_handle_);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_OZONE)
#if BUILDFLAG(IS_WIN)
    case Tag::kDxgiHandle:
      handle->type = gfx::DXGI_SHARED_HANDLE;
      return data.ReadDxgiHandle(&handle->dxgi_handle_);
#endif  // BUILDFLAG(IS_WIN)
#if BUILDFLAG(IS_ANDROID)
    case Tag::kAndroidHardwareBufferHandle:
      handle->type = gfx::ANDROID_HARDWARE_BUFFER;
      return data.ReadAndroidHardwareBufferHandle(
          &handle->android_hardware_buffer);
#endif  // BUILDFLAG(IS_ANDROID)
  }
  return false;
}

}  // namespace mojo
