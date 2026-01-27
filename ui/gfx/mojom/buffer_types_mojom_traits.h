// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_BUFFER_TYPES_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_BUFFER_TYPES_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/notreached.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"
#include "ui/gfx/mojom/buffer_types.mojom-shared.h"
#include "ui/gfx/mojom/native_handle_types.mojom.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(GFX_SHARED_MOJOM_TRAITS)
    EnumTraits<gfx::mojom::BufferUsage, gfx::BufferUsage> {
  static gfx::mojom::BufferUsage ToMojom(gfx::BufferUsage usage) {
    switch (usage) {
      case gfx::BufferUsage::GPU_READ:
        return gfx::mojom::BufferUsage::GPU_READ;
      case gfx::BufferUsage::SCANOUT:
        return gfx::mojom::BufferUsage::SCANOUT;
      case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
        return gfx::mojom::BufferUsage::SCANOUT_CAMERA_READ_WRITE;
      case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
        return gfx::mojom::BufferUsage::CAMERA_AND_CPU_READ_WRITE;
      case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
        return gfx::mojom::BufferUsage::SCANOUT_CPU_READ_WRITE;
      case gfx::BufferUsage::SCANOUT_VDA_WRITE:
        return gfx::mojom::BufferUsage::SCANOUT_VDA_WRITE;
      case gfx::BufferUsage::PROTECTED_SCANOUT:
        return gfx::mojom::BufferUsage::PROTECTED_SCANOUT;
      case gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE:
        return gfx::mojom::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE;
      case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
        return gfx::mojom::BufferUsage::GPU_READ_CPU_READ_WRITE;
      case gfx::BufferUsage::SCANOUT_VEA_CPU_READ:
        return gfx::mojom::BufferUsage::SCANOUT_VEA_CPU_READ;
      case gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE:
        return gfx::mojom::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE;
      case gfx::BufferUsage::SCANOUT_FRONT_RENDERING:
        return gfx::mojom::BufferUsage::SCANOUT_FRONT_RENDERING;
    }
    NOTREACHED();
  }

  static bool FromMojom(gfx::mojom::BufferUsage input, gfx::BufferUsage* out) {
    switch (input) {
      case gfx::mojom::BufferUsage::GPU_READ:
        *out = gfx::BufferUsage::GPU_READ;
        return true;
      case gfx::mojom::BufferUsage::SCANOUT:
        *out = gfx::BufferUsage::SCANOUT;
        return true;
      case gfx::mojom::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
        *out = gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE;
        return true;
      case gfx::mojom::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
        *out = gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE;
        return true;
      case gfx::mojom::BufferUsage::SCANOUT_CPU_READ_WRITE:
        *out = gfx::BufferUsage::SCANOUT_CPU_READ_WRITE;
        return true;
      case gfx::mojom::BufferUsage::SCANOUT_VDA_WRITE:
        *out = gfx::BufferUsage::SCANOUT_VDA_WRITE;
        return true;
      case gfx::mojom::BufferUsage::PROTECTED_SCANOUT:
        *out = gfx::BufferUsage::PROTECTED_SCANOUT;
        return true;
      case gfx::mojom::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE:
        *out = gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE;
        return true;
      case gfx::mojom::BufferUsage::GPU_READ_CPU_READ_WRITE:
        *out = gfx::BufferUsage::GPU_READ_CPU_READ_WRITE;
        return true;
      case gfx::mojom::BufferUsage::SCANOUT_VEA_CPU_READ:
        *out = gfx::BufferUsage::SCANOUT_VEA_CPU_READ;
        return true;
      case gfx::mojom::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE:
        *out = gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE;
        return true;
      case gfx::mojom::BufferUsage::SCANOUT_FRONT_RENDERING:
        *out = gfx::BufferUsage::SCANOUT_FRONT_RENDERING;
        return true;
    }
    NOTREACHED();
  }
};

#if BUILDFLAG(USE_BLINK)
template <>
struct COMPONENT_EXPORT(GFX_SHARED_MOJOM_TRAITS)
    StructTraits<gfx::mojom::GpuMemoryBufferHandleDataView,
                 gfx::GpuMemoryBufferHandle> {
  static uint32_t offset(const gfx::GpuMemoryBufferHandle& handle) {
    return handle.offset;
  }
  static uint32_t stride(const gfx::GpuMemoryBufferHandle& handle) {
    return handle.stride;
  }
  static gfx::GpuMemoryBufferHandle& platform_handle(
      gfx::GpuMemoryBufferHandle& handle);

  static bool Read(gfx::mojom::GpuMemoryBufferHandleDataView data,
                   gfx::GpuMemoryBufferHandle* handle);
};
#endif  // BUILDFLAG(USE_BLINK)

}  // namespace mojo

#endif  // UI_GFX_MOJOM_BUFFER_TYPES_MOJOM_TRAITS_H_
