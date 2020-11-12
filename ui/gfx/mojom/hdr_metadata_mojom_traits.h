// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_HDR_METADATA_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_HDR_METADATA_MOJOM_TRAITS_H_

#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/mojom/hdr_metadata.mojom.h"

namespace mojo {

template <>
struct StructTraits<gfx::mojom::MasteringMetadataDataView,
                    gfx::MasteringMetadata> {
  static const gfx::PointF& primary_r(const gfx::MasteringMetadata& input) {
    return input.primary_r;
  }
  static const gfx::PointF& primary_g(const gfx::MasteringMetadata& input) {
    return input.primary_g;
  }
  static const gfx::PointF& primary_b(const gfx::MasteringMetadata& input) {
    return input.primary_b;
  }
  static const gfx::PointF& white_point(const gfx::MasteringMetadata& input) {
    return input.white_point;
  }
  static float luminance_max(const gfx::MasteringMetadata& input) {
    return input.luminance_max;
  }
  static float luminance_min(const gfx::MasteringMetadata& input) {
    return input.luminance_min;
  }

  static bool Read(gfx::mojom::MasteringMetadataDataView data,
                   gfx::MasteringMetadata* output);
};

template <>
struct StructTraits<gfx::mojom::HDRMetadataDataView, gfx::HDRMetadata> {
  static unsigned max_content_light_level(const gfx::HDRMetadata& input) {
    return input.max_content_light_level;
  }
  static unsigned max_frame_average_light_level(const gfx::HDRMetadata& input) {
    return input.max_frame_average_light_level;
  }
  static const gfx::MasteringMetadata& mastering_metadata(
      const gfx::HDRMetadata& input) {
    return input.mastering_metadata;
  }

  static bool Read(gfx::mojom::HDRMetadataDataView data,
                   gfx::HDRMetadata* output);
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_HDR_METADATA_MOJOM_TRAITS_H_
