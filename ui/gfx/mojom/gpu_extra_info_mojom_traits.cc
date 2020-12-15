// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mojom/gpu_extra_info_mojom_traits.h"

#include "build/build_config.h"
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
#if defined(USE_OZONE_PLATFORM_X11) || defined(USE_X11)
  // These visuals below are obtained via methods of gl::GLVisualPickerGLX class
  // and consumed by ui::XVisualManager::UpdateVisualsOnGpuInfoChanged(); should
  // bad visuals come there, the GPU process will be shut down.
  //
  // See content::GpuDataManagerVisualProxyOzoneLinux and the ShutdownGpuOnIO()
  // function there.
  out->system_visual = static_cast<x11::VisualId>(data.system_visual());
  out->rgba_visual = static_cast<x11::VisualId>(data.rgba_visual());
  if (!data.ReadGpuMemoryBufferSupportX11(&out->gpu_memory_buffer_support_x11))
    return false;
#endif
  return true;
}

}  // namespace mojo
