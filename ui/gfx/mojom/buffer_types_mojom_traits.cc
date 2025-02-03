// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mojom/buffer_types_mojom_traits.h"

#include "build/build_config.h"
#include "ui/gfx/mojom/native_handle_types.mojom-shared.h"

namespace mojo {

// static
bool StructTraits<gfx::mojom::BufferUsageAndFormatDataView,
                  gfx::BufferUsageAndFormat>::
    Read(gfx::mojom::BufferUsageAndFormatDataView data,
         gfx::BufferUsageAndFormat* out) {
  return data.ReadUsage(&out->usage) && data.ReadFormat(&out->format);
}

gfx::mojom::GpuMemoryBufferPlatformHandlePtr StructTraits<
    gfx::mojom::GpuMemoryBufferHandleDataView,
    gfx::GpuMemoryBufferHandle>::platform_handle(gfx::GpuMemoryBufferHandle&
                                                     handle) {
  switch (handle.type) {
    case gfx::EMPTY_BUFFER:
      break;
    case gfx::SHARED_MEMORY_BUFFER:
      return gfx::mojom::GpuMemoryBufferPlatformHandle::NewSharedMemoryHandle(
          std::move(handle.region()));
    case gfx::NATIVE_PIXMAP:
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_OZONE)
      return gfx::mojom::GpuMemoryBufferPlatformHandle::NewNativePixmapHandle(
          std::move(handle.native_pixmap_handle));
#else
      break;
#endif
    case gfx::IO_SURFACE_BUFFER: {
#if BUILDFLAG(IS_APPLE)
      gfx::ScopedRefCountedIOSurfaceMachPort io_surface_mach_port(
          IOSurfaceCreateMachPort(handle.io_surface.get()));
      return gfx::mojom::GpuMemoryBufferPlatformHandle::NewMachPort(
          mojo::PlatformHandle(
              base::apple::RetainMachSendRight(io_surface_mach_port.get())));
#else
      break;
#endif
    }
    case gfx::DXGI_SHARED_HANDLE:
#if BUILDFLAG(IS_WIN)
      DCHECK(handle.dxgi_handle().IsValid());
      return gfx::mojom::GpuMemoryBufferPlatformHandle::NewDxgiHandle(
          std::move(handle.dxgi_handle()));
#else
      break;
#endif
    case gfx::ANDROID_HARDWARE_BUFFER: {
#if BUILDFLAG(IS_ANDROID)
      return gfx::mojom::GpuMemoryBufferPlatformHandle::
          NewAndroidHardwareBufferHandle(
              std::move(handle.android_hardware_buffer));
#else
      break;
#endif
    }
  }

  return nullptr;
}

bool StructTraits<gfx::mojom::GpuMemoryBufferHandleDataView,
                  gfx::GpuMemoryBufferHandle>::
    Read(gfx::mojom::GpuMemoryBufferHandleDataView data,
         gfx::GpuMemoryBufferHandle* out) {
  if (!data.ReadId(&out->id))
    return false;

  out->offset = data.offset();
  out->stride = data.stride();

  gfx::mojom::GpuMemoryBufferPlatformHandlePtr platform_handle;
  if (!data.ReadPlatformHandle(&platform_handle)) {
    return false;
  }

  if (!platform_handle) {
    out->type = gfx::EMPTY_BUFFER;
    return true;
  }

  switch (platform_handle->which()) {
    case gfx::mojom::GpuMemoryBufferPlatformHandleDataView::Tag::
        kSharedMemoryHandle:
      out->type = gfx::SHARED_MEMORY_BUFFER;
      out->set_region(std::move(platform_handle->get_shared_memory_handle()));
      return true;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_OZONE)
    case gfx::mojom::GpuMemoryBufferPlatformHandleDataView::Tag::
        kNativePixmapHandle:
      out->type = gfx::NATIVE_PIXMAP;
      out->native_pixmap_handle =
          std::move(platform_handle->get_native_pixmap_handle());
      return true;
#elif BUILDFLAG(IS_APPLE)
    case gfx::mojom::GpuMemoryBufferPlatformHandleDataView::Tag::kMachPort: {
      out->type = gfx::IO_SURFACE_BUFFER;
      if (!platform_handle->get_mach_port().is_mach_send())
        return false;
      gfx::ScopedRefCountedIOSurfaceMachPort io_surface_mach_port(
          platform_handle->get_mach_port().ReleaseMachSendRight());
      if (io_surface_mach_port) {
        out->io_surface.reset(
            IOSurfaceLookupFromMachPort(io_surface_mach_port.get()));
      } else {
        out->io_surface.reset();
      }
      return true;
    }
#elif BUILDFLAG(IS_WIN)
    case gfx::mojom::GpuMemoryBufferPlatformHandleDataView::Tag::kDxgiHandle: {
      out->type = gfx::DXGI_SHARED_HANDLE;
      out->set_dxgi_handle(std::move(platform_handle->get_dxgi_handle()));
      return true;
    }
#elif BUILDFLAG(IS_ANDROID)
    case gfx::mojom::GpuMemoryBufferPlatformHandleDataView::Tag::
        kAndroidHardwareBufferHandle: {
      out->type = gfx::ANDROID_HARDWARE_BUFFER;
      out->android_hardware_buffer =
          std::move(platform_handle->get_android_hardware_buffer_handle());
      return true;
    }
#endif
  }

  return false;
}

}  // namespace mojo
