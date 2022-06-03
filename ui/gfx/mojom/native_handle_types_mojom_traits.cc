// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mojom/native_handle_types_mojom_traits.h"

#include "build/build_config.h"

namespace mojo {

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(USE_OZONE)
mojo::PlatformHandle StructTraits<
    gfx::mojom::NativePixmapPlaneDataView,
    gfx::NativePixmapPlane>::buffer_handle(gfx::NativePixmapPlane& plane) {
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  return mojo::PlatformHandle(std::move(plane.fd));
#elif defined(OS_FUCHSIA)
  return mojo::PlatformHandle(std::move(plane.vmo));
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)
}

bool StructTraits<
    gfx::mojom::NativePixmapPlaneDataView,
    gfx::NativePixmapPlane>::Read(gfx::mojom::NativePixmapPlaneDataView data,
                                  gfx::NativePixmapPlane* out) {
  out->stride = data.stride();
  out->offset = data.offset();
  out->size = data.size();

  mojo::PlatformHandle handle = data.TakeBufferHandle();
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  if (!handle.is_fd())
    return false;
  out->fd = handle.TakeFD();
#elif defined(OS_FUCHSIA)
  if (!handle.is_handle())
    return false;
  out->vmo = zx::vmo(handle.TakeHandle());
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

  return true;
}

bool StructTraits<
    gfx::mojom::NativePixmapHandleDataView,
    gfx::NativePixmapHandle>::Read(gfx::mojom::NativePixmapHandleDataView data,
                                   gfx::NativePixmapHandle* out) {
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  out->modifier = data.modifier();
#endif

#if defined(OS_FUCHSIA)
  if (!data.ReadBufferCollectionId(&out->buffer_collection_id))
    return false;
  out->buffer_index = data.buffer_index();
  out->ram_coherency = data.ram_coherency();
#endif

  return data.ReadPlanes(&out->planes);
}
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(USE_OZONE)

#if defined(OS_WIN)
bool StructTraits<gfx::mojom::DXGIHandleTokenDataView, gfx::DXGIHandleToken>::
    Read(gfx::mojom::DXGIHandleTokenDataView& input,
         gfx::DXGIHandleToken* output) {
  base::UnguessableToken token;
  if (!input.ReadValue(&token))
    return false;
  *output = gfx::DXGIHandleToken(token);
  return true;
}
#endif  // defined(OS_WIN)

}  // namespace mojo
