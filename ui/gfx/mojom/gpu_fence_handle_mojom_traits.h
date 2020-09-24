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
    StructTraits<gfx::mojom::GpuFenceHandleDataView, gfx::GpuFenceHandle> {
  static mojo::PlatformHandle native_fd(gfx::GpuFenceHandle& handle);
  static bool Read(gfx::mojom::GpuFenceHandleDataView data,
                   gfx::GpuFenceHandle* handle);
  static void SetToNull(gfx::GpuFenceHandle* handle);
  static bool IsNull(const gfx::GpuFenceHandle& handle);
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_GPU_FENCE_HANDLE_MOJOM_TRAITS_H_
