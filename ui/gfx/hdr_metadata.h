// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_HDR_METADATA_H_
#define UI_GFX_HDR_METADATA_H_

#include <stdint.h>

#include <optional>
#include <string>

#include "base/check.h"
#include "base/containers/span.h"
#include "skia/ext/skcolorspace_primaries.h"
#include "third_party/skia/include/private/SkHdrMetadata.h"
#include "ui/gfx/color_space_export.h"
#include "ui/gfx/geometry/point_f.h"

namespace gfx {

class ColorSpace;

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

  std::weak_ordering operator<=>(const HdrMetadataExtendedRange&) const;
  friend bool operator==(const HdrMetadataExtendedRange&,
                         const HdrMetadataExtendedRange&) = default;
};

// Return whether or not use of AGTM metadata is enabled by default or not.
struct COLOR_SPACE_EXPORT HdrMetadataAgtm {
  static bool IsEnabled();
};

// HDR metadata common for HDR10 and WebM/VP9-based HDR formats.
struct COLOR_SPACE_EXPORT HDRMetadata {
  HDRMetadata();
  HDRMetadata(const skhdr::Metadata& sk_hdr_metadata);
  HDRMetadata(const skhdr::MasteringDisplayColorVolume& smpte_st_2086,
              const skhdr::ContentLightLevelInformation& cta_861_3);
  explicit HDRMetadata(const skhdr::MasteringDisplayColorVolume& smpte_st_2086);
  explicit HDRMetadata(const skhdr::ContentLightLevelInformation& cta_861_3);
  HDRMetadata(const HDRMetadata& rhs);
  HDRMetadata& operator=(const HDRMetadata& rhs);
  ~HDRMetadata();

  // Adaptive global tone mapping (AGTM) metadata.
  void SetAgtm(const skhdr::AdaptiveGlobalToneMap& agtm) { agtm_ = agtm; }
  bool HasAgtm() const { return agtm_.has_value(); }
  const skhdr::AdaptiveGlobalToneMap& GetAgtm() const {
    CHECK(agtm_.has_value());
    return agtm_.value();
  }

  // Parse `data` to AGTM metadata. If parsing succeeds, then set the AGTM
  // metadata to the parsed value. Otherwise leave it unchanged.
  void SetSerializedAgtm(base::span<const uint8_t> data);

  // Mastering display color volume (MDCV) metadata.
  void SetMDCV(const skhdr::MasteringDisplayColorVolume& smpte) {
    mdcv_ = smpte;
  }
  bool HasMDCV() const { return mdcv_.has_value(); }
  const skhdr::MasteringDisplayColorVolume& GetMDCV() const {
    CHECK(mdcv_.has_value());
    return mdcv_.value();
  }

  // Content light level information (CLLI) metadata.
  void SetCLLI(const skhdr::ContentLightLevelInformation& cta) { clli_ = cta; }
  bool HasCLLI() const { return clli_.has_value(); }
  const skhdr::ContentLightLevelInformation& GetCLLI() const {
    CHECK(clli_.has_value());
    return clli_.value();
  }

  // Nominal diffuse white level (NDWL), which is the number of nits of SDR
  // white.
  void SetNDWL(float nits) { ndwl_ = nits; }
  bool HasNDWL() const { return ndwl_.has_value(); }
  float GetNDWL() const {
    CHECK(ndwl_.has_value());
    return ndwl_.value();
  }

  // Brightness points for extended range color spaces.
  std::optional<HdrMetadataExtendedRange> extended_range;

  // For each metadata item of `other` that is specified, replace `this`
  // metadata item with that of `other`. E.g, if `other` has CLLI and MDCV
  // metadata, and `this` has MDCV and AGTM metadata, then after this call,
  // `this` will have its original AGTM, but the CLLI and MDCV from `other`.
  void MergeMetadataFrom(const HDRMetadata& other);

  // Return true if this structure holds no metadata.
  bool IsEmpty() const {
    return !mdcv_.has_value() && !clli_.has_value() && !ndwl_.has_value() &&
           !extended_range.has_value() && !agtm_.has_value();
  }

  bool IsValid() const {
    return (clli_ && (clli_->fMaxCLL > 0 || clli_->fMaxFALL > 0)) ||
           (mdcv_ &&
            (mdcv_->fDisplayPrimaries != SkNamedPrimariesExt::kInvalid ||
             mdcv_->fMaximumDisplayMasteringLuminance != 0.f ||
             mdcv_->fMinimumDisplayMasteringLuminance != 0.f)) ||
           extended_range;
  }

  // Compute the maximum luminance for the specified HDR metadata. This will
  // - return the CTA 861.3 max content light level metadata, if present
  // - return the SMPTE ST 2086 luminance max metadata, if present
  // - otherwise return 1,000 nits
  static float GetContentMaxLuminance(const HDRMetadata& metadata);

  // Compute the reference luminance for use with Wayland color management.
  static float GetWaylandReferenceLuminance(const ColorSpace& color_space,
                                            const HDRMetadata& hdr_metadata);

  // Return a copy of `hdr_metadata` with its `smpte_st_2086` fully
  // populated. Any unspecified values are set to default values (in particular,
  // the gamut is set to rec2020, minimum luminance to 0 nits, and maximum
  // luminance to 10,000 nits). The `max_content_light_level` and
  // `max_frame_average_light_level` values are not changed (they may stay
  // zero).
  static HDRMetadata PopulateUnspecifiedWithDefaults(
      const HDRMetadata& hdr_metadata);

  std::string ToString() const;

  // A default weak ordering for use with maps, sets, and caches.
  bool operator==(const HDRMetadata&) const;
  std::weak_ordering operator<=>(const HDRMetadata&) const;

 private:
  std::optional<skhdr::MasteringDisplayColorVolume> mdcv_;
  std::optional<skhdr::ContentLightLevelInformation> clli_;
  std::optional<skhdr::AdaptiveGlobalToneMap> agtm_;
  std::optional<float> ndwl_;
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
