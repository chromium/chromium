// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_HDR_METADATA_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_HDR_METADATA_MOJOM_TRAITS_H_

#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/mojom/hdr_metadata.mojom.h"

namespace mojo {

template <>
struct StructTraits<gfx::mojom::HdrMetadataCta861_3DataView,
                    gfx::HdrMetadataCta861_3> {
  static unsigned max_content_light_level(
      const gfx::HdrMetadataCta861_3& input) {
    return input.max_content_light_level;
  }
  static unsigned max_frame_average_light_level(
      const gfx::HdrMetadataCta861_3& input) {
    return input.max_frame_average_light_level;
  }
  static bool Read(gfx::mojom::HdrMetadataCta861_3DataView data,
                   gfx::HdrMetadataCta861_3* output);
};

template <>
struct StructTraits<gfx::mojom::HdrMetadataSmpteSt2086DataView,
                    gfx::HdrMetadataSmpteSt2086> {
  static const SkColorSpacePrimaries& primaries(
      const gfx::HdrMetadataSmpteSt2086& input) {
    return input.primaries;
  }
  static float luminance_max(const gfx::HdrMetadataSmpteSt2086& input) {
    return input.luminance_max;
  }
  static float luminance_min(const gfx::HdrMetadataSmpteSt2086& input) {
    return input.luminance_min;
  }

  static bool Read(gfx::mojom::HdrMetadataSmpteSt2086DataView data,
                   gfx::HdrMetadataSmpteSt2086* output);
};

template <>
struct StructTraits<gfx::mojom::HdrMetadataNdwlDataView, gfx::HdrMetadataNdwl> {
  static float nits(const gfx::HdrMetadataNdwl& input) { return input.nits; }

  static bool Read(gfx::mojom::HdrMetadataNdwlDataView data,
                   gfx::HdrMetadataNdwl* output);
};

template <>
struct StructTraits<gfx::mojom::HdrMetadataExtendedRangeDataView,
                    gfx::HdrMetadataExtendedRange> {
  static float current_headroom(const gfx::HdrMetadataExtendedRange& input) {
    return input.current_headroom;
  }
  static float desired_headroom(const gfx::HdrMetadataExtendedRange& input) {
    return input.desired_headroom;
  }

  static bool Read(gfx::mojom::HdrMetadataExtendedRangeDataView data,
                   gfx::HdrMetadataExtendedRange* output);
};

template <>
struct StructTraits<gfx::mojom::HDRMetadataDataView, gfx::HDRMetadata> {
  static const std::optional<gfx::HdrMetadataCta861_3>& cta_861_3(
      const gfx::HDRMetadata& input) {
    return input.cta_861_3;
  }
  static const std::optional<gfx::HdrMetadataSmpteSt2086>& smpte_st_2086(
      const gfx::HDRMetadata& input) {
    return input.smpte_st_2086;
  }
  static const std::optional<gfx::HdrMetadataNdwl>& ndwl(
      const gfx::HDRMetadata& input) {
    return input.ndwl;
  }
  static const std::optional<gfx::HdrMetadataExtendedRange>& extended_range(
      const gfx::HDRMetadata& input) {
    return input.extended_range;
  }

  static bool Read(gfx::mojom::HDRMetadataDataView data,
                   gfx::HDRMetadata* output);
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_HDR_METADATA_MOJOM_TRAITS_H_
