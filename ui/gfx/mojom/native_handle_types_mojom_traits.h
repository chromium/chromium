// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_NATIVE_HANDLE_TYPES_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_NATIVE_HANDLE_TYPES_MOJOM_TRAITS_H_

#include <optional>

#include "base/component_export.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/union_traits.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/mojom/native_handle_types.mojom-shared.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_OZONE)
#include "ui/gfx/native_pixmap_handle.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "ui/gfx/gpu_memory_buffer.h"  // for gfx::DXGIHandleToken
#endif

namespace mojo {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_OZONE)
template <>
struct COMPONENT_EXPORT(GFX_NATIVE_HANDLE_TYPES_SHARED_MOJOM_TRAITS)
    StructTraits<gfx::mojom::NativePixmapPlaneDataView,
                 gfx::NativePixmapPlane> {
  static uint32_t stride(const gfx::NativePixmapPlane& plane) {
    return plane.stride;
  }
  static int32_t offset(const gfx::NativePixmapPlane& plane) {
    return base::saturated_cast<int32_t>(plane.offset);
  }
  static uint64_t size(const gfx::NativePixmapPlane& plane) {
    return plane.size;
  }
  static mojo::PlatformHandle buffer_handle(gfx::NativePixmapPlane& plane);
  static bool Read(gfx::mojom::NativePixmapPlaneDataView data,
                   gfx::NativePixmapPlane* out);
};

template <>
struct COMPONENT_EXPORT(GFX_NATIVE_HANDLE_TYPES_SHARED_MOJOM_TRAITS)
    StructTraits<gfx::mojom::NativePixmapHandleDataView,
                 gfx::NativePixmapHandle> {
  static std::vector<gfx::NativePixmapPlane>& planes(
      gfx::NativePixmapHandle& pixmap_handle) {
    return pixmap_handle.planes;
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  static uint64_t modifier(const gfx::NativePixmapHandle& pixmap_handle) {
    return pixmap_handle.modifier;
  }
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  static bool supports_zero_copy_webgpu_import(
      const gfx::NativePixmapHandle& pixmap_handle) {
    return pixmap_handle.supports_zero_copy_webgpu_import;
  }
#endif

#if BUILDFLAG(IS_FUCHSIA)
  static PlatformHandle buffer_collection_handle(
      gfx::NativePixmapHandle& pixmap_handle);

  static uint32_t buffer_index(gfx::NativePixmapHandle& pixmap_handle) {
    return pixmap_handle.buffer_index;
  }

  static bool ram_coherency(gfx::NativePixmapHandle& pixmap_handle) {
    return pixmap_handle.ram_coherency;
  }
#endif  // BUILDFLAG(IS_FUCHSIA)

  static bool Read(gfx::mojom::NativePixmapHandleDataView data,
                   gfx::NativePixmapHandle* out);
};
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_OZONE)

#if BUILDFLAG(IS_WIN)
template <>
struct COMPONENT_EXPORT(GFX_NATIVE_HANDLE_TYPES_SHARED_MOJOM_TRAITS)
    StructTraits<gfx::mojom::DXGIHandleTokenDataView, gfx::DXGIHandleToken> {
  static const base::UnguessableToken& value(
      const gfx::DXGIHandleToken& input) {
    return input.value();
  }

  static bool Read(gfx::mojom::DXGIHandleTokenDataView& input,
                   gfx::DXGIHandleToken* output);
};
#endif  // BUILDFLAG(IS_WIN)

}  // namespace mojo

#endif  // UI_GFX_MOJOM_NATIVE_HANDLE_TYPES_MOJOM_TRAITS_H_
