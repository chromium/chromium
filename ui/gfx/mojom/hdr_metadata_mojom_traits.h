// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_HDR_METADATA_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_HDR_METADATA_MOJOM_TRAITS_H_

#include "skia/public/mojom/hdr_metadata_mojom_traits.h"
#include "third_party/skia/include/private/SkHdrMetadata.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/mojom/hdr_metadata.mojom.h"

namespace mojo {

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
  static mojo::OptionalAsPointer<const skhdr::ContentLightLevelInformation>
  clli(const gfx::HDRMetadata& input) {
    return mojo::OptionalAsPointer(input.HasCLLI() ? &input.GetCLLI()
                                                   : nullptr);
  }

  static mojo::OptionalAsPointer<const skhdr::MasteringDisplayColorVolume> mdcv(
      const gfx::HDRMetadata& input) {
    return mojo::OptionalAsPointer(input.HasMDCV() ? &input.GetMDCV()
                                                   : nullptr);
  }

  static std::optional<float> ndwl(const gfx::HDRMetadata& input) {
    if (input.HasNDWL()) {
      return input.GetNDWL();
    }
    return std::nullopt;
  }

  static mojo::OptionalAsPointer<const skhdr::AdaptiveGlobalToneMap> agtm(
      const gfx::HDRMetadata& input) {
    return mojo::OptionalAsPointer(input.HasAgtm() ? &input.GetAgtm()
                                                   : nullptr);
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
