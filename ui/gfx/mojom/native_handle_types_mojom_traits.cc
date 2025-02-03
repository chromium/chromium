// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mojom/native_handle_types_mojom_traits.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_hardware_buffer_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/scope_to_message_pipe.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/memory/unsafe_shared_memory_region.h"
#include "mojo/public/cpp/base/shared_memory_mojom_traits.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "ui/gfx/gpu_memory_buffer.h"
#endif  // BUILDFLAG(IS_WIN)

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

}  // namespace mojo
