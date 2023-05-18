// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mojom/hdr_metadata_mojom_traits.h"

namespace mojo {

bool StructTraits<gfx::mojom::HdrMetadataCta861_3DataView,
                  gfx::HdrMetadataCta861_3>::
    Read(gfx::mojom::HdrMetadataCta861_3DataView data,
         gfx::HdrMetadataCta861_3* output) {
  output->max_content_light_level = data.max_content_light_level();
  output->max_frame_average_light_level = data.max_frame_average_light_level();
  return true;
}

bool StructTraits<gfx::mojom::HdrMetadataSmpteSt2086DataView,
                  gfx::HdrMetadataSmpteSt2086>::
    Read(gfx::mojom::HdrMetadataSmpteSt2086DataView data,
         gfx::HdrMetadataSmpteSt2086* output) {
  output->luminance_max = data.luminance_max();
  output->luminance_min = data.luminance_min();
  if (!data.ReadPrimaries(&output->primaries))
    return false;
  return true;
}

bool StructTraits<gfx::mojom::HDRMetadataDataView, gfx::HDRMetadata>::Read(
    gfx::mojom::HDRMetadataDataView data,
    gfx::HDRMetadata* output) {
  if (!data.ReadCta8613(&output->cta_861_3)) {
    return false;
  }
  if (!data.ReadSmpteSt2086(&output->smpte_st_2086)) {
    return false;
  }
  return true;
}

}  // namespace mojo
