// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/hdr_metadata.h"

#include <iomanip>
#include <sstream>

namespace gfx {

ColorVolumeMetadata::ColorVolumeMetadata() = default;
ColorVolumeMetadata::ColorVolumeMetadata(const ColorVolumeMetadata& rhs) =
    default;
ColorVolumeMetadata& ColorVolumeMetadata::operator=(
    const ColorVolumeMetadata& rhs) = default;

std::string ColorVolumeMetadata::ToString() const {
  std::stringstream ss;
  ss << std::fixed << std::setprecision(4);
  ss << "{";
  ss << "red:[" << primary_r.x() << ", " << primary_r.y() << "], ";
  ss << "green:[" << primary_g.x() << ", " << primary_g.y() << "], ";
  ss << "blue:[" << primary_b.x() << ", " << primary_b.y() << "], ";
  ss << "whitePoint:[" << white_point.x() << ", " << white_point.y() << "], ";
  ss << "minLum:" << luminance_min << ", "
     << "maxLum:" << luminance_max;
  ss << "}";
  return ss.str();
}

HDRMetadata::HDRMetadata() = default;
HDRMetadata::HDRMetadata(const HDRMetadata& rhs) = default;
HDRMetadata& HDRMetadata::operator=(const HDRMetadata& rhs) = default;

std::string HDRMetadata::ToString() const {
  std::stringstream ss;
  ss << "{";
  ss << "smpteSt2086:" << color_volume_metadata.ToString() << ", ";
  ss << "maxCLL:" << max_content_light_level << ", ";
  ss << "maxFALL:" << max_frame_average_light_level;
  ss << "}";
  return ss.str();
}

}  // namespace gfx
