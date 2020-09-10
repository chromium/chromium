// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/mojom/hdr_metadata_mojom_traits.h"

namespace mojo {

bool StructTraits<gl::mojom::MasteringMetadataDataView, gl::MasteringMetadata>::
    Read(gl::mojom::MasteringMetadataDataView data,
         gl::MasteringMetadata* output) {
  output->luminance_max = data.luminance_max();
  output->luminance_min = data.luminance_min();
  if (!data.ReadPrimaryR(&output->primary_r))
    return false;
  if (!data.ReadPrimaryG(&output->primary_g))
    return false;
  if (!data.ReadPrimaryB(&output->primary_b))
    return false;
  if (!data.ReadWhitePoint(&output->white_point))
    return false;
  return true;
}

bool StructTraits<gl::mojom::HDRMetadataDataView, gl::HDRMetadata>::Read(
    gl::mojom::HDRMetadataDataView data,
    gl::HDRMetadata* output) {
  output->max_content_light_level = data.max_content_light_level();
  output->max_frame_average_light_level = data.max_frame_average_light_level();
  if (!data.ReadMasteringMetadata(&output->mastering_metadata))
    return false;
  return true;
}
}  // namespace mojo
