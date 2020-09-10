// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_MOJOM_HDR_METADATA_MOJOM_TRAITS_H_
#define UI_GL_MOJOM_HDR_METADATA_MOJOM_TRAITS_H_

#include "ui/gl/hdr_metadata.h"
#include "ui/gl/mojom/hdr_metadata.mojom.h"

namespace mojo {

template <>
struct StructTraits<gl::mojom::MasteringMetadataDataView,
                    gl::MasteringMetadata> {
  static const gfx::PointF& primary_r(const gl::MasteringMetadata& input) {
    return input.primary_r;
  }
  static const gfx::PointF& primary_g(const gl::MasteringMetadata& input) {
    return input.primary_g;
  }
  static const gfx::PointF& primary_b(const gl::MasteringMetadata& input) {
    return input.primary_b;
  }
  static const gfx::PointF& white_point(const gl::MasteringMetadata& input) {
    return input.white_point;
  }
  static float luminance_max(const gl::MasteringMetadata& input) {
    return input.luminance_max;
  }
  static float luminance_min(const gl::MasteringMetadata& input) {
    return input.luminance_min;
  }

  static bool Read(gl::mojom::MasteringMetadataDataView data,
                   gl::MasteringMetadata* output);
};

template <>
struct StructTraits<gl::mojom::HDRMetadataDataView, gl::HDRMetadata> {
  static unsigned max_content_light_level(const gl::HDRMetadata& input) {
    return input.max_content_light_level;
  }
  static unsigned max_frame_average_light_level(const gl::HDRMetadata& input) {
    return input.max_frame_average_light_level;
  }
  static const gl::MasteringMetadata& mastering_metadata(
      const gl::HDRMetadata& input) {
    return input.mastering_metadata;
  }

  static bool Read(gl::mojom::HDRMetadataDataView data,
                   gl::HDRMetadata* output);
};

}  // namespace mojo

#endif  // UI_GL_MOJOM_HDR_METADATA_MOJOM_TRAITS_H_
