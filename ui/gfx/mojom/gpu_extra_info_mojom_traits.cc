// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mojom/gpu_extra_info_mojom_traits.h"

#include "build/build_config.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/gfx/mojom/buffer_types_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<gfx::mojom::ANGLEFeatureDataView, gfx::ANGLEFeature>::Read(
    gfx::mojom::ANGLEFeatureDataView data,
    gfx::ANGLEFeature* out) {
  return data.ReadName(&out->name) && data.ReadCategory(&out->category) &&
         data.ReadDescription(&out->description) && data.ReadBug(&out->bug) &&
         data.ReadStatus(&out->status) && data.ReadCondition(&out->condition);
}

// static
bool StructTraits<gfx::mojom::GpuExtraInfoDataView, gfx::GpuExtraInfo>::Read(
    gfx::mojom::GpuExtraInfoDataView data,
    gfx::GpuExtraInfo* out) {
  if (!data.ReadAngleFeatures(&out->angle_features))
    return false;
#if BUILDFLAG(IS_OZONE_X11)
  if (!data.ReadGpuMemoryBufferSupportX11(&out->gpu_memory_buffer_support_x11))
    return false;
#endif  // BUILDFLAG(IS_OZONE_X11)
  return true;
}

}  // namespace mojo
