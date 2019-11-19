// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_BUFFER_TYPES_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_BUFFER_TYPES_MOJOM_TRAITS_H_

#include <vector>

#include "base/containers/span.h"
#include "build/build_config.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/mojom/buffer_types.mojom.h"

namespace mojo {

template <>
struct EnumTraits<gfx::mojom::BufferFormat, gfx::BufferFormat> {
  static gfx::mojom::BufferFormat ToMojom(gfx::BufferFormat format) {
    switch (format) {
      case gfx::BufferFormat::R_8:
        return gfx::mojom::BufferFormat::R_8;
      case gfx::BufferFormat::R_16:
        return gfx::mojom::BufferFormat::R_16;
      case gfx::BufferFormat::RG_88:
        return gfx::mojom::BufferFormat::RG_88;
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
      case gfx::BufferFormat::BGRX_1010102:
        return gfx::mojom::BufferFormat::BGRX_1010102;
      case gfx::BufferFormat::RGBX_1010102:
        return gfx::mojom::BufferFormat::RGBX_1010102;
      case gfx::BufferFormat::BGRA_8888:
        return gfx::mojom::BufferFormat::BGRA_8888;
      case gfx::BufferFormat::RGBA_F16:
        return gfx::mojom::BufferFormat::RGBA_F16;
      case gfx::BufferFormat::YVU_420:
        return gfx::mojom::BufferFormat::YVU_420;
      case gfx::BufferFormat::YUV_420_BIPLANAR:
        return gfx::mojom::BufferFormat::YUV_420_BIPLANAR;
      case gfx::BufferFormat::P010:
        return gfx::mojom::BufferFormat::P010;
    }
    NOTREACHED();
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
      case gfx::mojom::BufferFormat::BGR_565:
        *out = gfx::BufferFormat::BGR_565;
        return true;
      case gfx::mojom::BufferFormat::RGBA_4444:
        *out = gfx::BufferFormat::RGBA_4444;
        return true;
      case gfx::mojom::BufferFormat::RGBX_8888:
        *out = gfx::BufferFormat::RGBX_8888;
        return true;
      case gfx::mojom::BufferFormat::BGRX_1010102:
        *out = gfx::BufferFormat::BGRX_1010102;
        return true;
      case gfx::mojom::BufferFormat::RGBX_1010102:
        *out = gfx::BufferFormat::RGBX_1010102;
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
      case gfx::mojom::BufferFormat::P010:
        *out = gfx::BufferFormat::P010;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

template <>
struct EnumTraits<gfx::mojom::BufferUsage, gfx::BufferUsage> {
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
      case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
        return gfx::mojom::BufferUsage::GPU_READ_CPU_READ_WRITE;
      case gfx::BufferUsage::SCANOUT_VEA_READ_CAMERA_AND_CPU_READ_WRITE:
        return gfx::mojom::BufferUsage::
            SCANOUT_VEA_READ_CAMERA_AND_CPU_READ_WRITE;
    }
    NOTREACHED();
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
      case gfx::mojom::BufferUsage::GPU_READ_CPU_READ_WRITE:
        *out = gfx::BufferUsage::GPU_READ_CPU_READ_WRITE;
        return true;
      case gfx::mojom::BufferUsage::SCANOUT_VEA_READ_CAMERA_AND_CPU_READ_WRITE:
        *out = gfx::BufferUsage::SCANOUT_VEA_READ_CAMERA_AND_CPU_READ_WRITE;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

template <>
struct StructTraits<gfx::mojom::BufferUsageAndFormatDataView,
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
struct StructTraits<gfx::mojom::GpuMemoryBufferIdDataView,
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

#if defined(OS_LINUX) || defined(USE_OZONE)
template <>
struct StructTraits<gfx::mojom::NativePixmapPlaneDataView,
                    gfx::NativePixmapPlane> {
  static uint32_t stride(const gfx::NativePixmapPlane& plane) {
    return plane.stride;
  }
  static int32_t offset(const gfx::NativePixmapPlane& plane) {
    return plane.offset;
  }
  static uint64_t size(const gfx::NativePixmapPlane& plane) {
    return plane.size;
  }
  static mojo::ScopedHandle buffer_handle(gfx::NativePixmapPlane& plane);
  static bool Read(gfx::mojom::NativePixmapPlaneDataView data,
                   gfx::NativePixmapPlane* out);
};

template <>
struct StructTraits<gfx::mojom::NativePixmapHandleDataView,
                    gfx::NativePixmapHandle> {
  static std::vector<gfx::NativePixmapPlane>& planes(
      gfx::NativePixmapHandle& pixmap_handle) {
    return pixmap_handle.planes;
  }

#if defined(OS_LINUX)
  static uint64_t modifier(const gfx::NativePixmapHandle& pixmap_handle) {
    return pixmap_handle.modifier;
  }
#endif

#if defined(OS_FUCHSIA)
  static const base::Optional<base::UnguessableToken>& buffer_collection_id(
      const gfx::NativePixmapHandle& pixmap_handle) {
    return pixmap_handle.buffer_collection_id;
  }

  static uint32_t buffer_index(gfx::NativePixmapHandle& pixmap_handle) {
    return pixmap_handle.buffer_index;
  }

  static bool ram_coherency(gfx::NativePixmapHandle& pixmap_handle) {
    return pixmap_handle.ram_coherency;
  }
#endif  // defined(OS_FUCHSIA)

  static bool Read(gfx::mojom::NativePixmapHandleDataView data,
                   gfx::NativePixmapHandle* out);
};
#endif  // defined(OS_LINUX) || defined(USE_OZONE)

template <>
struct StructTraits<gfx::mojom::GpuMemoryBufferHandleDataView,
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
  static gfx::mojom::GpuMemoryBufferPlatformHandlePtr platform_handle(
      gfx::GpuMemoryBufferHandle& handle);

  static bool Read(gfx::mojom::GpuMemoryBufferHandleDataView data,
                   gfx::GpuMemoryBufferHandle* handle);
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_BUFFER_TYPES_MOJOM_TRAITS_H_
