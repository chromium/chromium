// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mojo/buffer_types_struct_traits.h"

#include "build/build_config.h"
#include "mojo/public/cpp/base/shared_memory_mojom_traits.h"
#include "mojo/public/cpp/system/platform_handle.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_hardware_buffer_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/scope_to_message_pipe.h"
#endif

#if !defined(OS_LINUX)
#include "base/no_destructor.h"
#endif

namespace mojo {

// static
bool StructTraits<gfx::mojom::BufferUsageAndFormatDataView,
                  gfx::BufferUsageAndFormat>::
    Read(gfx::mojom::BufferUsageAndFormatDataView data,
         gfx::BufferUsageAndFormat* out) {
  return data.ReadUsage(&out->usage) && data.ReadFormat(&out->format);
}

std::vector<mojo::ScopedHandle>
StructTraits<gfx::mojom::NativePixmapHandleDataView, gfx::NativePixmapHandle>::
    fds(const gfx::NativePixmapHandle& pixmap_handle) {
  std::vector<mojo::ScopedHandle> handles;
#if defined(OS_LINUX)
  for (const base::FileDescriptor& fd : pixmap_handle.fds)
    handles.emplace_back(mojo::WrapPlatformFile(fd.fd));
#endif  // defined(OS_LINUX)
  return handles;
}

bool StructTraits<
    gfx::mojom::NativePixmapHandleDataView,
    gfx::NativePixmapHandle>::Read(gfx::mojom::NativePixmapHandleDataView data,
                                   gfx::NativePixmapHandle* out) {
#if defined(OS_LINUX)
  mojo::ArrayDataView<mojo::ScopedHandle> handles_data_view;
  data.GetFdsDataView(&handles_data_view);
  for (size_t i = 0; i < handles_data_view.size(); ++i) {
    mojo::ScopedHandle handle = handles_data_view.Take(i);
    base::PlatformFile platform_file;
    if (mojo::UnwrapPlatformFile(std::move(handle), &platform_file) !=
        MOJO_RESULT_OK)
      return false;
    constexpr bool auto_close = true;
    out->fds.push_back(base::FileDescriptor(platform_file, auto_close));
  }
  return data.ReadPlanes(&out->planes);
#else
  return false;
#endif
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
    case gfx::NATIVE_PIXMAP: {
#if defined(OS_LINUX)
      return gfx::mojom::GpuMemoryBufferPlatformHandle::NewNativePixmapHandle(
          handle.native_pixmap_handle);
#else
      static base::NoDestructor<gfx::NativePixmapHandle> pixmap_handle;
      return gfx::mojom::GpuMemoryBufferPlatformHandle::NewNativePixmapHandle(
          *pixmap_handle);
#endif
    }
    case gfx::IO_SURFACE_BUFFER:
#if defined(OS_MACOSX) && !defined(OS_IOS)
      return gfx::mojom::GpuMemoryBufferPlatformHandle::NewMachPort(
          mojo::WrapMachPort(handle.mach_port.get()));
#else
      break;
#endif
    case gfx::DXGI_SHARED_HANDLE:
#if defined(OS_WIN)
      DCHECK(handle.dxgi_handle.IsValid());
      return gfx::mojom::GpuMemoryBufferPlatformHandle::NewDxgiHandle(
          mojo::WrapPlatformFile(handle.dxgi_handle.GetHandle()));
#else
      break;
#endif
    case gfx::ANDROID_HARDWARE_BUFFER: {
#if defined(OS_ANDROID)
      // We must keep a ref to the AHardwareBuffer alive until the receiver has
      // acquired its own reference. We do this by sending a message pipe handle
      // along with the buffer. When the receiver deserializes (or even if they
      // die without ever reading the message) their end of the pipe will be
      // closed. We will eventually detect this and release the AHB reference.
      mojo::MessagePipe tracking_pipe;
      auto wrapped_handle = gfx::mojom::AHardwareBufferHandle::New(
          mojo::WrapPlatformFile(
              handle.android_hardware_buffer.SerializeAsFileDescriptor()
                  .release()),
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
        SHARED_MEMORY_HANDLE:
      out->type = gfx::SHARED_MEMORY_BUFFER;
      out->region = std::move(platform_handle->get_shared_memory_handle());
      return true;
    case gfx::mojom::GpuMemoryBufferPlatformHandleDataView::Tag::
        NATIVE_PIXMAP_HANDLE:
#if defined(OS_LINUX)
      out->type = gfx::NATIVE_PIXMAP;
      out->native_pixmap_handle = platform_handle->get_native_pixmap_handle();
      return true;
#else
      return false;
#endif
    case gfx::mojom::GpuMemoryBufferPlatformHandleDataView::Tag::MACH_PORT: {
#if defined(OS_MACOSX) && !defined(OS_IOS)
      out->type = gfx::IO_SURFACE_BUFFER;
      mach_port_t mach_port;
      MojoResult unwrap_result = mojo::UnwrapMachPort(
          std::move(platform_handle->get_mach_port()), &mach_port);
      if (unwrap_result != MOJO_RESULT_OK)
        return false;
      out->mach_port.reset(mach_port);
      return true;
#else
      return false;
#endif
    }
#if defined(OS_WIN)
    case gfx::mojom::GpuMemoryBufferPlatformHandleDataView::Tag::DXGI_HANDLE: {
      out->type = gfx::DXGI_SHARED_HANDLE;
      HANDLE handle;
      MojoResult unwrap_result = mojo::UnwrapPlatformFile(
          std::move(platform_handle->get_dxgi_handle()), &handle);
      if (unwrap_result != MOJO_RESULT_OK)
        return false;
      out->dxgi_handle = IPC::PlatformFileForTransit(handle);
      return true;
    }
#endif
#if defined(OS_ANDROID)
    case gfx::mojom::GpuMemoryBufferPlatformHandleDataView::Tag::
        ANDROID_HARDWARE_BUFFER_HANDLE: {
      out->type = gfx::ANDROID_HARDWARE_BUFFER;
      gfx::mojom::AHardwareBufferHandlePtr buffer_handle =
          std::move(platform_handle->get_android_hardware_buffer_handle());
      if (!buffer_handle)
        return false;

      base::PlatformFile fd;
      MojoResult unwrap_result = mojo::UnwrapPlatformFile(
          std::move(buffer_handle->buffer_handle), &fd);
      base::ScopedFD scoped_fd(fd);
      if (unwrap_result != MOJO_RESULT_OK || !scoped_fd.is_valid())
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
