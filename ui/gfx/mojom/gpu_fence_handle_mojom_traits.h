// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_GPU_FENCE_HANDLE_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_GPU_FENCE_HANDLE_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/mojom/gpu_fence_handle.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(GFX_SHARED_MOJOM_TRAITS)
    EnumTraits<gfx::mojom::GpuFenceHandleType, gfx::GpuFenceHandleType> {
  static gfx::mojom::GpuFenceHandleType ToMojom(gfx::GpuFenceHandleType type) {
    switch (type) {
      case gfx::GpuFenceHandleType::kEmpty:
        return gfx::mojom::GpuFenceHandleType::kEmpty;
      case gfx::GpuFenceHandleType::kAndroidNativeFenceSync:
        return gfx::mojom::GpuFenceHandleType::kAndroidNativeFenceSync;
    }
    NOTREACHED();
    return gfx::mojom::GpuFenceHandleType::kEmpty;
  }

  static bool FromMojom(gfx::mojom::GpuFenceHandleType input,
                        gfx::GpuFenceHandleType* out) {
    switch (input) {
      case gfx::mojom::GpuFenceHandleType::kEmpty:
        *out = gfx::GpuFenceHandleType::kEmpty;
        return true;
      case gfx::mojom::GpuFenceHandleType::kAndroidNativeFenceSync:
        *out = gfx::GpuFenceHandleType::kAndroidNativeFenceSync;
        return true;
    }
    return false;
  }
};

template <>
struct COMPONENT_EXPORT(GFX_SHARED_MOJOM_TRAITS)
    StructTraits<gfx::mojom::GpuFenceHandleDataView, gfx::GpuFenceHandle> {
  static gfx::GpuFenceHandleType type(const gfx::GpuFenceHandle& handle) {
    return handle.type;
  }
  static mojo::PlatformHandle native_fd(gfx::GpuFenceHandle& handle);
  static bool Read(gfx::mojom::GpuFenceHandleDataView data,
                   gfx::GpuFenceHandle* handle);
  static void SetToNull(gfx::GpuFenceHandle* handle);
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_GPU_FENCE_HANDLE_MOJOM_TRAITS_H_
