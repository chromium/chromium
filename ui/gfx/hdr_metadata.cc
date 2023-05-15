// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/hdr_metadata.h"

#include "skia/ext/skcolorspace_primaries.h"

#include <iomanip>
#include <sstream>

namespace gfx {

ColorVolumeMetadata::ColorVolumeMetadata() = default;
ColorVolumeMetadata::ColorVolumeMetadata(const ColorVolumeMetadata& rhs) =
    default;
ColorVolumeMetadata& ColorVolumeMetadata::operator=(
    const ColorVolumeMetadata& rhs) = default;

ColorVolumeMetadata::ColorVolumeMetadata(const SkColorSpacePrimaries& primaries,
                                         float luminance_max,
                                         float luminance_min)
    : primaries(primaries),
      luminance_max(luminance_max),
      luminance_min(luminance_min) {}

std::string ColorVolumeMetadata::ToString() const {
  std::stringstream ss;
  ss << std::fixed << std::setprecision(4);
  ss << "{";
  ss << "red:[" << primaries.fRX << ", " << primaries.fRY << "], ";
  ss << "green:[" << primaries.fGX << ", " << primaries.fGY << "], ";
  ss << "blue:[" << primaries.fBX << ", " << primaries.fBY << "], ";
  ss << "whitePoint:[" << primaries.fWX << ", " << primaries.fWY << "], ";
  ss << "minLum:" << luminance_min << ", "
     << "maxLum:" << luminance_max;
  ss << "}";
  return ss.str();
}

HDRMetadata::HDRMetadata() = default;
HDRMetadata::HDRMetadata(const ColorVolumeMetadata& color_volume_metadata,
                         unsigned max_content_light_level,
                         unsigned max_frame_average_light_level)
    : color_volume_metadata(color_volume_metadata),
      max_content_light_level(max_content_light_level),
      max_frame_average_light_level(max_frame_average_light_level) {}
HDRMetadata::HDRMetadata(const HDRMetadata& rhs) = default;
HDRMetadata& HDRMetadata::operator=(const HDRMetadata& rhs) = default;

// static
HDRMetadata HDRMetadata::PopulateUnspecifiedWithDefaults(
    const absl::optional<gfx::HDRMetadata>& hdr_metadata) {
  const HDRMetadata defaults(
      ColorVolumeMetadata(SkNamedPrimariesExt::kRec2020, 10000.f, 0.f), 0, 0);

  if (!hdr_metadata)
    return defaults;

  HDRMetadata result = *hdr_metadata;

  // If the gamut is unspecified, replace it with the default Rec2020.
  if (result.color_volume_metadata.primaries == SkNamedPrimariesExt::kInvalid) {
    result.color_volume_metadata.primaries =
        defaults.color_volume_metadata.primaries;
  }

  // If the max luminance is unspecified, replace it with the default 10,000
  // nits.
  if (result.color_volume_metadata.luminance_max == 0.f) {
    result.color_volume_metadata.luminance_max =
        defaults.color_volume_metadata.luminance_max;
  }

  return result;
}

std::string HDRMetadata::ToString() const {
  std::stringstream ss;
  ss << "{";
  ss << "smpteSt2086:" << color_volume_metadata.ToString() << ", ";
  ss << "maxCLL:" << max_content_light_level << ", ";
  ss << "maxFALL:" << max_frame_average_light_level;

  if (extended_range_brightness) {
    ss << "cur_ratio: " << extended_range_brightness->current_buffer_ratio;
    ss << "desired_ratio: " << extended_range_brightness->desired_ratio;
  }

  ss << "}";
  return ss.str();
}

}  // namespace gfx
