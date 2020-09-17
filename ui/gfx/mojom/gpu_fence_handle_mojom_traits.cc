// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mojom/gpu_fence_handle_mojom_traits.h"

#include "build/build_config.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace mojo {

mojo::PlatformHandle
StructTraits<gfx::mojom::GpuFenceHandleDataView,
             gfx::GpuFenceHandle>::native_fd(gfx::GpuFenceHandle& handle) {
#if defined(OS_POSIX)
  if (handle.type != gfx::GpuFenceHandleType::kAndroidNativeFenceSync)
    return mojo::PlatformHandle();
  return mojo::PlatformHandle(std::move(handle.owned_fd));
#else
  return mojo::PlatformHandle();
#endif
}

bool StructTraits<gfx::mojom::GpuFenceHandleDataView, gfx::GpuFenceHandle>::
    Read(gfx::mojom::GpuFenceHandleDataView data, gfx::GpuFenceHandle* out) {
  if (!data.ReadType(&out->type))
    return false;

  if (out->type == gfx::GpuFenceHandleType::kAndroidNativeFenceSync) {
#if defined(OS_POSIX)
    out->owned_fd = data.TakeNativeFd().TakeFD();
    return true;
#else
    NOTREACHED();
    return false;
#endif
  }
  return true;
}

void StructTraits<gfx::mojom::GpuFenceHandleDataView,
                  gfx::GpuFenceHandle>::SetToNull(gfx::GpuFenceHandle* handle) {
  handle->type = gfx::GpuFenceHandleType::kEmpty;
#if defined(OS_POSIX)
  handle->owned_fd.reset();
#endif
}

}  // namespace mojo
