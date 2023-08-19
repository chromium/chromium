// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mojom/buffer_types_mojom_traits.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_hardware_buffer_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/scope_to_message_pipe.h"
#endif

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
          std::move(handle.region));
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
      DCHECK(handle.dxgi_handle.IsValid());
      DCHECK(handle.dxgi_token.has_value());
      return gfx::mojom::GpuMemoryBufferPlatformHandle::NewDxgiHandle(
          gfx::mojom::DXGIHandle::New(
              mojo::PlatformHandle(std::move(handle.dxgi_handle)),
              std::move(handle.dxgi_token.value()), std::move(handle.region)));
#else
      break;
#endif
    case gfx::ANDROID_HARDWARE_BUFFER: {
#if BUILDFLAG(IS_ANDROID)
      // We must keep a ref to the AHardwareBuffer alive until the receiver has
      // acquired its own reference. We do this by sending a message pipe handle
      // along with the buffer. When the receiver deserializes (or even if they
      // die without ever reading the message) their end of the pipe will be
      // closed. We will eventually detect this and release the AHB reference.
      mojo::MessagePipe tracking_pipe;
      auto wrapped_handle = gfx::mojom::AHardwareBufferHandle::New(
          mojo::PlatformHandle(
              handle.android_hardware_buffer.SerializeAsFileDescriptor()),
          std::move(tracking_pipe.handle0));

      // Pass ownership of the input handle to our tracking pipe to keep the AHB
      // alive until it's deserialized.
      mojo::ScopeToMessagePipe(std::move(handle.android_hardware_buffer),
                               std::move(tracking_pipe.handle1));
      return gfx::mojom::GpuMemoryBufferPlatformHandle::
          NewAndroidHardwareBufferHandle(std::move(wrapped_handle));
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
      out->region = std::move(platform_handle->get_shared_memory_handle());
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
      auto dxgi_handle = std::move(platform_handle->get_dxgi_handle());
      out->dxgi_handle = dxgi_handle->buffer_handle.TakeHandle();
      out->dxgi_token = std::move(dxgi_handle->token);
      out->region = std::move(dxgi_handle->shared_memory_handle);
      return true;
    }
#elif BUILDFLAG(IS_ANDROID)
    case gfx::mojom::GpuMemoryBufferPlatformHandleDataView::Tag::
        kAndroidHardwareBufferHandle: {
      out->type = gfx::ANDROID_HARDWARE_BUFFER;
      gfx::mojom::AHardwareBufferHandlePtr buffer_handle =
          std::move(platform_handle->get_android_hardware_buffer_handle());
      if (!buffer_handle)
        return false;

      base::ScopedFD scoped_fd = buffer_handle->buffer_handle.TakeFD();
      if (!scoped_fd.is_valid())
        return false;
      out->android_hardware_buffer = base::android::ScopedHardwareBufferHandle::
          DeserializeFromFileDescriptor(std::move(scoped_fd));
      return out->android_hardware_buffer.is_valid();
    }
#endif
  }

  return false;
}

}  // namespace mojo
