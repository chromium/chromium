// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_HDR_METADATA_H_
#define UI_GFX_HDR_METADATA_H_

#include <stdint.h>

#include <optional>
#include <string>

#include "skia/ext/skcolorspace_primaries.h"
#include "ui/gfx/color_space_export.h"
#include "ui/gfx/geometry/point_f.h"

struct SkColorSpacePrimaries;

namespace gfx {

// Content light level info (CLLI) metadata from CTA 861.3.
struct COLOR_SPACE_EXPORT HdrMetadataCta861_3 {
  constexpr HdrMetadataCta861_3() = default;
  constexpr HdrMetadataCta861_3(unsigned max_content_light_level,
                                unsigned max_frame_average_light_level)
      : max_content_light_level(max_content_light_level),
        max_frame_average_light_level(max_frame_average_light_level) {}

  // Max content light level (CLL), i.e. maximum brightness level present in the
  // stream), in nits.
  unsigned max_content_light_level = 0;

  // Max frame-average light level (FALL), i.e. maximum average brightness of
  // the brightest frame in the stream), in nits.
  unsigned max_frame_average_light_level = 0;

  std::string ToString() const;
  bool IsValid() const {
    return max_content_light_level > 0 || max_frame_average_light_level > 0;
  }
  bool operator==(const HdrMetadataCta861_3& rhs) const {
    return max_content_light_level == rhs.max_content_light_level &&
           max_frame_average_light_level == rhs.max_frame_average_light_level;
  }
  bool operator!=(const HdrMetadataCta861_3& rhs) const {
    return !(*this == rhs);
  }
};

// SMPTE ST 2086 color volume metadata.
struct COLOR_SPACE_EXPORT HdrMetadataSmpteSt2086 {
  SkColorSpacePrimaries primaries = SkNamedPrimariesExt::kInvalid;
  float luminance_max = 0;
  float luminance_min = 0;

  constexpr HdrMetadataSmpteSt2086() = default;
  constexpr HdrMetadataSmpteSt2086(const HdrMetadataSmpteSt2086& rhs) = default;
  constexpr HdrMetadataSmpteSt2086(const SkColorSpacePrimaries& primaries,
                                   float luminance_max,
                                   float luminance_min)
      : primaries(primaries),
        luminance_max(luminance_max),
        luminance_min(luminance_min) {}
  HdrMetadataSmpteSt2086& operator=(const HdrMetadataSmpteSt2086& rhs);

  std::string ToString() const;

  bool IsValid() const {
    return primaries != SkNamedPrimariesExt::kInvalid || luminance_max != 0.f ||
           luminance_min != 0.f;
  }

  bool operator==(const HdrMetadataSmpteSt2086& rhs) const {
    return (primaries == rhs.primaries && luminance_max == rhs.luminance_max &&
            luminance_min == rhs.luminance_min);
  }

  bool operator!=(const HdrMetadataSmpteSt2086& rhs) const {
    return !(*this == rhs);
  }
};

// Nominal diffuse white level (NDWL) metadata.
struct COLOR_SPACE_EXPORT HdrMetadataNdwl {
  constexpr HdrMetadataNdwl() = default;
  constexpr explicit HdrMetadataNdwl(float nits) : nits(nits) {}

  // The number of nits of SDR white. Default to 203 nits from ITU-R BT.2408 and
  // ISO 22028-5.
  float nits = 203.f;

  std::string ToString() const;

  bool operator==(const HdrMetadataNdwl& rhs) const { return nits == rhs.nits; }

  bool operator!=(const HdrMetadataNdwl& rhs) const { return !(*this == rhs); }
};

// HDR metadata for extended range color spaces.
struct COLOR_SPACE_EXPORT HdrMetadataExtendedRange {
  constexpr HdrMetadataExtendedRange() = default;
  constexpr HdrMetadataExtendedRange(float current_headroom,
                                     float desired_headroom)
      : current_headroom(current_headroom),
        desired_headroom(desired_headroom) {}

  // The HDR headroom of the contents of the current buffer.
  float current_headroom = 1.f;

  // The desired HDR headroom of the content in the current buffer. This may be
  // greater than `current_headroom` if the content in the current buffer had
  // to be tonemapped to fit into `current_headroom`.
  float desired_headroom = 1.f;

  // For HDR content that does not specify a headroom, this value is the
  // headroom of HLG and most PQ content.
  static constexpr float kDefaultHdrHeadroom = 1000.f / 203.f;

  std::string ToString() const;

  bool operator==(const HdrMetadataExtendedRange& rhs) const {
    return (current_headroom == rhs.current_headroom &&
            desired_headroom == rhs.desired_headroom);
  }

  bool operator!=(const HdrMetadataExtendedRange& rhs) const {
    return !(*this == rhs);
  }
};

// HDR metadata common for HDR10 and WebM/VP9-based HDR formats.
struct COLOR_SPACE_EXPORT HDRMetadata {
  // Mastering display color volume (MDCV) metadata.
  std::optional<HdrMetadataSmpteSt2086> smpte_st_2086;

  // Content light level information (CLLI) metadata.
  std::optional<HdrMetadataCta861_3> cta_861_3;

  // The number of nits of SDR white.
  std::optional<HdrMetadataNdwl> ndwl;

  // Brightness points for extended range color spaces.
  std::optional<HdrMetadataExtendedRange> extended_range;

  HDRMetadata() = default;
  HDRMetadata(const HdrMetadataSmpteSt2086& smpte_st_2086,
              const HdrMetadataCta861_3& cta_861_3)
      : smpte_st_2086(smpte_st_2086), cta_861_3(cta_861_3) {}
  explicit HDRMetadata(const HdrMetadataSmpteSt2086& smpte_st_2086)
      : smpte_st_2086(smpte_st_2086) {}
  explicit HDRMetadata(const HdrMetadataCta861_3& cta_861_3)
      : cta_861_3(cta_861_3) {}
  HDRMetadata(const HDRMetadata& rhs) = default;
  HDRMetadata& operator=(const HDRMetadata& rhs) = default;

  bool IsValid() const {
    return (cta_861_3 && cta_861_3->IsValid()) ||
           (smpte_st_2086 && smpte_st_2086->IsValid()) || extended_range;
  }

  // Return a copy of `hdr_metadata` with its `smpte_st_2086` fully
  // populated. Any unspecified values are set to default values (in particular,
  // the gamut is set to rec2020, minimum luminance to 0 nits, and maximum
  // luminance to 10,000 nits). The `max_content_light_level` and
  // `max_frame_average_light_level` values are not changed (they may stay
  // zero).
  static HDRMetadata PopulateUnspecifiedWithDefaults(
      const std::optional<gfx::HDRMetadata>& hdr_metadata);

  std::string ToString() const;

  bool operator==(const HDRMetadata& rhs) const {
    return cta_861_3 == rhs.cta_861_3 && smpte_st_2086 == rhs.smpte_st_2086 &&
           ndwl == rhs.ndwl && extended_range == rhs.extended_range;
  }

  bool operator!=(const HDRMetadata& rhs) const { return !(*this == rhs); }
};

// HDR metadata types as described in
// https://w3c.github.io/media-capabilities/#enumdef-hdrmetadatatype
enum class HdrMetadataType : uint8_t {
  kNone,
  kSmpteSt2086,
  kSmpteSt2094_10,
  kSmpteSt2094_40,
};

}  // namespace gfx

#endif  // UI_GFX_HDR_METADATA_H_
