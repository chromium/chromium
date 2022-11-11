// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mojom/hdr_metadata_mojom_traits.h"

namespace mojo {

bool StructTraits<gfx::mojom::ColorVolumeMetadataDataView,
                  gfx::ColorVolumeMetadata>::
    Read(gfx::mojom::ColorVolumeMetadataDataView data,
         gfx::ColorVolumeMetadata* output) {
  output->luminance_max = data.luminance_max();
  output->luminance_min = data.luminance_min();
  if (!data.ReadPrimaries(&output->primaries))
    return false;
  return true;
}

bool StructTraits<gfx::mojom::HDRMetadataDataView, gfx::HDRMetadata>::Read(
    gfx::mojom::HDRMetadataDataView data,
    gfx::HDRMetadata* output) {
  output->max_content_light_level = data.max_content_light_level();
  output->max_frame_average_light_level = data.max_frame_average_light_level();
  if (!data.ReadColorVolumeMetadata(&output->color_volume_metadata))
    return false;
  return true;
}
}  // namespace mojo
