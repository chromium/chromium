// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_HDR_METADATA_H_
#define UI_GL_HDR_METADATA_H_

#include "ui/gfx/geometry/point_f.h"
#include "ui/gl/gl_export.h"

namespace gl {

// SMPTE ST 2086 mastering metadata.
struct GL_EXPORT MasteringMetadata {
  using Chromaticity = gfx::PointF;
  Chromaticity primary_r;
  Chromaticity primary_g;
  Chromaticity primary_b;
  Chromaticity white_point;
  float luminance_max = 0;
  float luminance_min = 0;

  MasteringMetadata();
  MasteringMetadata(const MasteringMetadata& rhs);

  bool operator==(const MasteringMetadata& rhs) const {
    return ((primary_r == rhs.primary_r) && (primary_g == rhs.primary_g) &&
            (primary_b == rhs.primary_b) && (white_point == rhs.white_point) &&
            (luminance_max == rhs.luminance_max) &&
            (luminance_min == rhs.luminance_min));
  }
};

// HDR metadata common for HDR10 and WebM/VP9-based HDR formats.
struct GL_EXPORT HDRMetadata {
  MasteringMetadata mastering_metadata;
  // Max content light level (CLL), i.e. maximum brightness level present in the
  // stream), in nits.
  unsigned max_content_light_level = 0;
  // Max frame-average light level (FALL), i.e. maximum average brightness of
  // the brightest frame in the stream), in nits.
  unsigned max_frame_average_light_level = 0;

  HDRMetadata();
  HDRMetadata(const HDRMetadata& rhs);

  bool IsValid() const {
    return !((max_content_light_level == 0) &&
             (max_frame_average_light_level == 0) &&
             (mastering_metadata == MasteringMetadata()));
  }

  bool operator==(const HDRMetadata& rhs) const {
    return (
        (max_content_light_level == rhs.max_content_light_level) &&
        (max_frame_average_light_level == rhs.max_frame_average_light_level) &&
        (mastering_metadata == rhs.mastering_metadata));
  }
};

// HDR metadata types as described in
// https://w3c.github.io/media-capabilities/#enumdef-hdrmetadatatype
enum class HdrMetadataType {
  kNone,
  kSmpteSt2086,
  kSmpteSt2094_10,
  kSmpteSt2094_40,
};

}  // namespace gl

#endif  // UI_GL_HDR_METADATA_H_
