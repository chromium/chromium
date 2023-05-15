// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_HDR_METADATA_H_
#define UI_GFX_HDR_METADATA_H_

#include <stdint.h>
#include <string>

#include "skia/ext/skcolorspace_primaries.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/color_space_export.h"
#include "ui/gfx/geometry/point_f.h"

struct SkColorSpacePrimaries;

namespace gfx {

// High dynamic range mode.
enum class HDRMode : uint8_t {
  // HLG and PQ content is HDR and tone mapped. All other content is clipped to
  // SDR luminance.
  kDefault,
  // Values that extend beyond SDR luminance are shown as HDR. No tone mapping
  // is performed.
  kExtended,
};

// SMPTE ST 2086 color volume metadata.
struct COLOR_SPACE_EXPORT ColorVolumeMetadata {
  SkColorSpacePrimaries primaries = SkNamedPrimariesExt::kInvalid;
  float luminance_max = 0;
  float luminance_min = 0;

  ColorVolumeMetadata();
  ColorVolumeMetadata(const ColorVolumeMetadata& rhs);
  ColorVolumeMetadata(const SkColorSpacePrimaries& primaries,
                      float luminance_max,
                      float luminance_min);
  ColorVolumeMetadata& operator=(const ColorVolumeMetadata& rhs);

  std::string ToString() const;

  bool operator==(const ColorVolumeMetadata& rhs) const {
    return (primaries == rhs.primaries && luminance_max == rhs.luminance_max &&
            luminance_min == rhs.luminance_min);
  }

  bool operator!=(const ColorVolumeMetadata& rhs) const {
    return !(*this == rhs);
  }
};

// HDR metadata for extended range color spaces.
struct COLOR_SPACE_EXPORT ExtendedRangeBrightness {
  // The current hdr/sdr ratio of the current buffer. For example if the buffer
  // was rendered with a target SDR whitepoint of 100 nits and a max display
  // brightness of 200 nits, this should be set to 2.0f.
  float current_buffer_ratio = 1.0f;

  // The desired hdr/sdr ratio. This can be used to communicate the max desired
  // brightness range. This is similar to the "max luminance" value in other HDR
  // metadata formats, but represented as a ratio of the target SDR whitepoint
  // to the max display brightness.
  float desired_ratio = 1.0f;

  bool operator==(const ExtendedRangeBrightness& rhs) const {
    return (current_buffer_ratio == rhs.current_buffer_ratio &&
            desired_ratio == rhs.desired_ratio);
  }

  bool operator!=(const ExtendedRangeBrightness& rhs) const {
    return !(*this == rhs);
  }
};

// HDR metadata common for HDR10 and WebM/VP9-based HDR formats.
struct COLOR_SPACE_EXPORT HDRMetadata {
  ColorVolumeMetadata color_volume_metadata;
  // Max content light level (CLL), i.e. maximum brightness level present in the
  // stream), in nits.
  unsigned max_content_light_level = 0;
  // Max frame-average light level (FALL), i.e. maximum average brightness of
  // the brightest frame in the stream), in nits.
  unsigned max_frame_average_light_level = 0;

  // Brightness points for extended range color spaces.
  // NOTE: Is not serialized over IPC.
  absl::optional<ExtendedRangeBrightness> extended_range_brightness;

  HDRMetadata();
  HDRMetadata(const ColorVolumeMetadata& color_volume_metadata,
              unsigned max_content_light_level,
              unsigned max_frame_average_light_level);
  HDRMetadata(const HDRMetadata& rhs);
  HDRMetadata& operator=(const HDRMetadata& rhs);

  bool IsValid() const {
    return !((max_content_light_level == 0) &&
             (max_frame_average_light_level == 0) &&
             (color_volume_metadata == ColorVolumeMetadata()) &&
             !extended_range_brightness);
  }

  // Return a copy of `hdr_metadata` with its `color_volume_metadata` fully
  // populated. Any unspecified values are set to default values (in particular,
  // the gamut is set to rec2020, minimum luminance to 0 nits, and maximum
  // luminance to 10,000 nits). The `max_content_light_level` and
  // `max_frame_average_light_level` values are not changed (they may stay
  // zero).
  static HDRMetadata PopulateUnspecifiedWithDefaults(
      const absl::optional<gfx::HDRMetadata>& hdr_metadata);

  std::string ToString() const;

  bool operator==(const HDRMetadata& rhs) const {
    return (
        (max_content_light_level == rhs.max_content_light_level) &&
        (max_frame_average_light_level == rhs.max_frame_average_light_level) &&
        (color_volume_metadata == rhs.color_volume_metadata) &&
        (extended_range_brightness == rhs.extended_range_brightness));
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
