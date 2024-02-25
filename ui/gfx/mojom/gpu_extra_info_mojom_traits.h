// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_GPU_EXTRA_INFO_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_GPU_EXTRA_INFO_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "build/build_config.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/gfx/gpu_extra_info.h"
#include "ui/gfx/mojom/buffer_types_mojom_traits.h"
#include "ui/gfx/mojom/gpu_extra_info.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(GFX_SHARED_MOJOM_TRAITS)
    StructTraits<gfx::mojom::ANGLEFeatureDataView, gfx::ANGLEFeature> {
  static bool Read(gfx::mojom::ANGLEFeatureDataView data,
                   gfx::ANGLEFeature* out);

  static const std::string& name(const gfx::ANGLEFeature& input) {
    return input.name;
  }

  static const std::string& category(const gfx::ANGLEFeature& input) {
    return input.category;
  }

  static const std::string& description(const gfx::ANGLEFeature& input) {
    return input.description;
  }

  static const std::string& bug(const gfx::ANGLEFeature& input) {
    return input.bug;
  }

  static const std::string& status(const gfx::ANGLEFeature& input) {
    return input.status;
  }

  static const std::string& condition(const gfx::ANGLEFeature& input) {
    return input.condition;
  }
};

template <>
struct COMPONENT_EXPORT(GFX_SHARED_MOJOM_TRAITS)
    StructTraits<gfx::mojom::GpuExtraInfoDataView, gfx::GpuExtraInfo> {
  static bool Read(gfx::mojom::GpuExtraInfoDataView data,
                   gfx::GpuExtraInfo* out);

  static const std::vector<gfx::ANGLEFeature>& angle_features(
      const gfx::GpuExtraInfo& input) {
    return input.angle_features;
  }

#if BUILDFLAG(IS_OZONE_X11)
  static const std::vector<gfx::BufferUsageAndFormat>&
  gpu_memory_buffer_support_x11(const gfx::GpuExtraInfo& input) {
    return input.gpu_memory_buffer_support_x11;
  }
#endif  // BUILDFLAG(IS_OZONE_X11)
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_GPU_EXTRA_INFO_MOJOM_TRAITS_H_
