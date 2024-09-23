// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_BUFFER_TYPES_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_BUFFER_TYPES_MOJOM_TRAITS_H_


#include "base/component_export.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/mojom/buffer_types.mojom-shared.h"
#include "ui/gfx/mojom/native_handle_types.mojom.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(GFX_SHARED_MOJOM_TRAITS)
    EnumTraits<gfx::mojom::BufferFormat, gfx::BufferFormat> {
  static gfx::mojom::BufferFormat ToMojom(gfx::BufferFormat format) {
    switch (format) {
      case gfx::BufferFormat::R_8:
        return gfx::mojom::BufferFormat::R_8;
      case gfx::BufferFormat::R_16:
        return gfx::mojom::BufferFormat::R_16;
      case gfx::BufferFormat::RG_88:
        return gfx::mojom::BufferFormat::RG_88;
      case gfx::BufferFormat::RG_1616:
        return gfx::mojom::BufferFormat::RG_1616;
      case gfx::BufferFormat::BGR_565:
        return gfx::mojom::BufferFormat::BGR_565;
      case gfx::BufferFormat::RGBA_4444:
        return gfx::mojom::BufferFormat::RGBA_4444;
      case gfx::BufferFormat::RGBX_8888:
        return gfx::mojom::BufferFormat::RGBX_8888;
      case gfx::BufferFormat::RGBA_8888:
        return gfx::mojom::BufferFormat::RGBA_8888;
      case gfx::BufferFormat::BGRX_8888:
        return gfx::mojom::BufferFormat::BGRX_8888;
      case gfx::BufferFormat::BGRA_1010102:
        return gfx::mojom::BufferFormat::BGRA_1010102;
      case gfx::BufferFormat::RGBA_1010102:
        return gfx::mojom::BufferFormat::RGBA_1010102;
      case gfx::BufferFormat::BGRA_8888:
        return gfx::mojom::BufferFormat::BGRA_8888;
      case gfx::BufferFormat::RGBA_F16:
        return gfx::mojom::BufferFormat::RGBA_F16;
      case gfx::BufferFormat::YVU_420:
        return gfx::mojom::BufferFormat::YVU_420;
      case gfx::BufferFormat::YUV_420_BIPLANAR:
        return gfx::mojom::BufferFormat::YUV_420_BIPLANAR;
      case gfx::BufferFormat::YUVA_420_TRIPLANAR:
        return gfx::mojom::BufferFormat::YUVA_420_TRIPLANAR;
      case gfx::BufferFormat::P010:
        return gfx::mojom::BufferFormat::P010;
    }
    NOTREACHED_IN_MIGRATION();
    return gfx::mojom::BufferFormat::kMinValue;
  }

  static bool FromMojom(gfx::mojom::BufferFormat input,
                        gfx::BufferFormat* out) {
    switch (input) {
      case gfx::mojom::BufferFormat::R_8:
        *out = gfx::BufferFormat::R_8;
        return true;
      case gfx::mojom::BufferFormat::R_16:
        *out = gfx::BufferFormat::R_16;
        return true;
      case gfx::mojom::BufferFormat::RG_88:
        *out = gfx::BufferFormat::RG_88;
        return true;
      case gfx::mojom::BufferFormat::RG_1616:
        *out = gfx::BufferFormat::RG_1616;
        return true;
      case gfx::mojom::BufferFormat::BGR_565:
        *out = gfx::BufferFormat::BGR_565;
        return true;
      case gfx::mojom::BufferFormat::RGBA_4444:
        *out = gfx::BufferFormat::RGBA_4444;
        return true;
      case gfx::mojom::BufferFormat::RGBX_8888:
        *out = gfx::BufferFormat::RGBX_8888;
        return true;
      case gfx::mojom::BufferFormat::BGRA_1010102:
        *out = gfx::BufferFormat::BGRA_1010102;
        return true;
      case gfx::mojom::BufferFormat::RGBA_1010102:
        *out = gfx::BufferFormat::RGBA_1010102;
        return true;
      case gfx::mojom::BufferFormat::RGBA_8888:
        *out = gfx::BufferFormat::RGBA_8888;
        return true;
      case gfx::mojom::BufferFormat::BGRX_8888:
        *out = gfx::BufferFormat::BGRX_8888;
        return true;
      case gfx::mojom::BufferFormat::BGRA_8888:
        *out = gfx::BufferFormat::BGRA_8888;
        return true;
      case gfx::mojom::BufferFormat::RGBA_F16:
        *out = gfx::BufferFormat::RGBA_F16;
        return true;
      case gfx::mojom::BufferFormat::YVU_420:
        *out = gfx::BufferFormat::YVU_420;
        return true;
      case gfx::mojom::BufferFormat::YUV_420_BIPLANAR:
        *out = gfx::BufferFormat::YUV_420_BIPLANAR;
        return true;
      case gfx::mojom::BufferFormat::YUVA_420_TRIPLANAR:
        *out = gfx::BufferFormat::YUVA_420_TRIPLANAR;
        return true;
      case gfx::mojom::BufferFormat::P010:
        *out = gfx::BufferFormat::P010;
        return true;
    }
    NOTREACHED_IN_MIGRATION();
    return false;
  }
};

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
    NOTREACHED_IN_MIGRATION();
    return gfx::mojom::BufferUsage::kMinValue;
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
    NOTREACHED_IN_MIGRATION();
    return false;
  }
};

template <>
struct COMPONENT_EXPORT(GFX_SHARED_MOJOM_TRAITS)
    StructTraits<gfx::mojom::BufferUsageAndFormatDataView,
                 gfx::BufferUsageAndFormat> {
  static gfx::BufferUsage usage(const gfx::BufferUsageAndFormat& input) {
    return input.usage;
  }

  static gfx::BufferFormat format(const gfx::BufferUsageAndFormat& input) {
    return input.format;
  }

  static bool Read(gfx::mojom::BufferUsageAndFormatDataView data,
                   gfx::BufferUsageAndFormat* out);
};

template <>
struct COMPONENT_EXPORT(GFX_SHARED_MOJOM_TRAITS)
    StructTraits<gfx::mojom::GpuMemoryBufferIdDataView,
                 gfx::GpuMemoryBufferId> {
  static int32_t id(const gfx::GpuMemoryBufferId& buffer_id) {
    return buffer_id.id;
  }
  static bool Read(gfx::mojom::GpuMemoryBufferIdDataView data,
                   gfx::GpuMemoryBufferId* out) {
    out->id = data.id();
    return true;
  }
};

template <>
struct COMPONENT_EXPORT(GFX_SHARED_MOJOM_TRAITS)
    StructTraits<gfx::mojom::GpuMemoryBufferHandleDataView,
                 gfx::GpuMemoryBufferHandle> {
  static gfx::GpuMemoryBufferId id(const gfx::GpuMemoryBufferHandle& handle) {
    return handle.id;
  }
  static uint32_t offset(const gfx::GpuMemoryBufferHandle& handle) {
    return handle.offset;
  }
  static uint32_t stride(const gfx::GpuMemoryBufferHandle& handle) {
    return handle.stride;
  }
  static mojo::StructPtr<gfx::mojom::GpuMemoryBufferPlatformHandle>
  platform_handle(gfx::GpuMemoryBufferHandle& handle);

  static bool Read(gfx::mojom::GpuMemoryBufferHandleDataView data,
                   gfx::GpuMemoryBufferHandle* handle);
};

template <>
struct COMPONENT_EXPORT(GFX_SHARED_MOJOM_TRAITS)
    EnumTraits<gfx::mojom::BufferPlane, gfx::BufferPlane> {
  static gfx::mojom::BufferPlane ToMojom(gfx::BufferPlane format) {
    switch (format) {
      case gfx::BufferPlane::DEFAULT:
        return gfx::mojom::BufferPlane::DEFAULT;
      case gfx::BufferPlane::Y:
        return gfx::mojom::BufferPlane::Y;
      case gfx::BufferPlane::UV:
        return gfx::mojom::BufferPlane::UV;
      case gfx::BufferPlane::U:
        return gfx::mojom::BufferPlane::U;
      case gfx::BufferPlane::V:
        return gfx::mojom::BufferPlane::V;
      case gfx::BufferPlane::A:
        return gfx::mojom::BufferPlane::A;
    }
    NOTREACHED_IN_MIGRATION();
    return gfx::mojom::BufferPlane::kMinValue;
  }

  static bool FromMojom(gfx::mojom::BufferPlane input, gfx::BufferPlane* out) {
    switch (input) {
      case gfx::mojom::BufferPlane::DEFAULT:
        *out = gfx::BufferPlane::DEFAULT;
        return true;
      case gfx::mojom::BufferPlane::Y:
        *out = gfx::BufferPlane::Y;
        return true;
      case gfx::mojom::BufferPlane::UV:
        *out = gfx::BufferPlane::UV;
        return true;
      case gfx::mojom::BufferPlane::U:
        *out = gfx::BufferPlane::U;
        return true;
      case gfx::mojom::BufferPlane::V:
        *out = gfx::BufferPlane::V;
        return true;
      case gfx::mojom::BufferPlane::A:
        *out = gfx::BufferPlane::A;
        return true;
    }
    NOTREACHED_IN_MIGRATION();
    return false;
  }
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_BUFFER_TYPES_MOJOM_TRAITS_H_
