// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_NATIVE_HANDLE_TYPES_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_NATIVE_HANDLE_TYPES_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/union_traits.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/mojom/native_handle_types.mojom-shared.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(USE_OZONE)
#include "ui/gfx/native_pixmap_handle.h"
#endif

namespace mojo {

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(USE_OZONE)
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

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  static uint64_t modifier(const gfx::NativePixmapHandle& pixmap_handle) {
    return pixmap_handle.modifier;
  }
#endif

#if defined(OS_FUCHSIA)
  static const absl::optional<base::UnguessableToken>& buffer_collection_id(
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
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(USE_OZONE)

}  // namespace mojo

#endif  // UI_GFX_MOJOM_NATIVE_HANDLE_TYPES_MOJOM_TRAITS_H_
