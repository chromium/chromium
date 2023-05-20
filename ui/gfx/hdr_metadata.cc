// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/hdr_metadata.h"

#include "skia/ext/skcolorspace_primaries.h"

#include <iomanip>
#include <sstream>

namespace gfx {

std::string HdrMetadataCta861_3::ToString() const {
  std::stringstream ss;
  ss << std::fixed << std::setprecision(4)
     << "{maxCLL:" << max_content_light_level
     << ", maxFALL:" << max_frame_average_light_level << "}";
  return ss.str();
}

HdrMetadataSmpteSt2086::HdrMetadataSmpteSt2086() = default;
HdrMetadataSmpteSt2086::HdrMetadataSmpteSt2086(
    const HdrMetadataSmpteSt2086& rhs) = default;
HdrMetadataSmpteSt2086& HdrMetadataSmpteSt2086::operator=(
    const HdrMetadataSmpteSt2086& rhs) = default;

HdrMetadataSmpteSt2086::HdrMetadataSmpteSt2086(
    const SkColorSpacePrimaries& primaries,
    float luminance_max,
    float luminance_min)
    : primaries(primaries),
      luminance_max(luminance_max),
      luminance_min(luminance_min) {}

std::string HdrMetadataSmpteSt2086::ToString() const {
  std::stringstream ss;
  ss << std::fixed << std::setprecision(4) << "{"
     << "red:[" << primaries.fRX << ", " << primaries.fRY << "], "
     << "green:[" << primaries.fGX << ", " << primaries.fGY << "], "
     << "blue:[" << primaries.fBX << ", " << primaries.fBY << "], "
     << "whitePoint:[" << primaries.fWX << ", " << primaries.fWY << "], "
     << "minLum:" << luminance_min << ", "
     << "maxLum:" << luminance_max << "}";
  return ss.str();
}

// static
HDRMetadata HDRMetadata::PopulateUnspecifiedWithDefaults(
    const absl::optional<gfx::HDRMetadata>& hdr_metadata) {
  const HDRMetadata defaults(
      HdrMetadataSmpteSt2086(SkNamedPrimariesExt::kRec2020, 10000.f, 0.f));

  if (!hdr_metadata)
    return defaults;

  HDRMetadata result = *hdr_metadata;

  // If the gamut is unspecified, replace it with the default Rec2020.
  if (result.smpte_st_2086.primaries == SkNamedPrimariesExt::kInvalid) {
    result.smpte_st_2086.primaries = defaults.smpte_st_2086.primaries;
  }

  // If the max luminance is unspecified, replace it with the default 10,000
  // nits.
  if (result.smpte_st_2086.luminance_max == 0.f) {
    result.smpte_st_2086.luminance_max = defaults.smpte_st_2086.luminance_max;
  }

  return result;
}

std::string HDRMetadata::ToString() const {
  std::stringstream ss;
  ss << "{";
  ss << "smpte_st_2086:" << smpte_st_2086.ToString();
  ss << ", cta_861_3:" << cta_861_3.ToString() << ", ";

  if (extended_range_brightness) {
    ss << "cur_ratio: " << extended_range_brightness->current_buffer_ratio;
    ss << "desired_ratio: " << extended_range_brightness->desired_ratio;
  }

  ss << "}";
  return ss.str();
}

}  // namespace gfx
