// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_HDR_METADATA_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_HDR_METADATA_MOJOM_TRAITS_H_

#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/mojom/hdr_metadata.mojom.h"

namespace mojo {

template <>
struct EnumTraits<gfx::mojom::HDRMode, gfx::HDRMode> {
  static gfx::mojom::HDRMode ToMojom(gfx::HDRMode input) {
    switch (input) {
      case gfx::HDRMode::kDefault:
        return gfx::mojom::HDRMode::kDefault;
      case gfx::HDRMode::kExtended:
        return gfx::mojom::HDRMode::kExtended;
    }
    NOTREACHED();
    return gfx::mojom::HDRMode::kDefault;
  }

  static bool FromMojom(gfx::mojom::HDRMode input, gfx::HDRMode* out) {
    switch (input) {
      case gfx::mojom::HDRMode::kDefault:
        *out = gfx::HDRMode::kDefault;
        return true;
      case gfx::mojom::HDRMode::kExtended:
        *out = gfx::HDRMode::kExtended;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

template <>
struct StructTraits<gfx::mojom::ColorVolumeMetadataDataView,
                    gfx::ColorVolumeMetadata> {
  static const SkColorSpacePrimaries& primaries(
      const gfx::ColorVolumeMetadata& input) {
    return input.primaries;
  }
  static float luminance_max(const gfx::ColorVolumeMetadata& input) {
    return input.luminance_max;
  }
  static float luminance_min(const gfx::ColorVolumeMetadata& input) {
    return input.luminance_min;
  }

  static bool Read(gfx::mojom::ColorVolumeMetadataDataView data,
                   gfx::ColorVolumeMetadata* output);
};

template <>
struct StructTraits<gfx::mojom::HDRMetadataDataView, gfx::HDRMetadata> {
  static unsigned max_content_light_level(const gfx::HDRMetadata& input) {
    return input.max_content_light_level;
  }
  static unsigned max_frame_average_light_level(const gfx::HDRMetadata& input) {
    return input.max_frame_average_light_level;
  }
  static const gfx::ColorVolumeMetadata& color_volume_metadata(
      const gfx::HDRMetadata& input) {
    return input.color_volume_metadata;
  }

  static bool Read(gfx::mojom::HDRMetadataDataView data,
                   gfx::HDRMetadata* output);
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_HDR_METADATA_MOJOM_TRAITS_H_
