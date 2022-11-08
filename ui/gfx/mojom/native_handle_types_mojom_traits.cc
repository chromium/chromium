// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mojom/native_handle_types_mojom_traits.h"

#include "build/build_config.h"

namespace mojo {

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
