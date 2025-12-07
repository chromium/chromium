// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mojom/gpu_extra_info_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<gfx::mojom::ANGLEFeatureDataView, gfx::ANGLEFeature>::Read(
    gfx::mojom::ANGLEFeatureDataView data,
    gfx::ANGLEFeature* out) {
  return data.ReadName(&out->name) && data.ReadCategory(&out->category) &&
         data.ReadStatus(&out->status);
}

// static
bool StructTraits<gfx::mojom::GpuExtraInfoDataView, gfx::GpuExtraInfo>::Read(
    gfx::mojom::GpuExtraInfoDataView data,
    gfx::GpuExtraInfo* out) {
  if (!data.ReadAngleFeatures(&out->angle_features))
    return false;
  return true;
}

}  // namespace mojo
