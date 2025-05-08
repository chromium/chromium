// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_HDR_METADATA_AGTM_H_
#define UI_GFX_HDR_METADATA_AGTM_H_

#include <vector>

#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/color_space_export.h"

namespace gfx {

struct HdrMetadataAgtm;

struct COLOR_SPACE_EXPORT HdrMetadataAgtmParsed {
  HdrMetadataAgtmParsed();
  HdrMetadataAgtmParsed(const HdrMetadataAgtmParsed&) = delete;
  HdrMetadataAgtmParsed& operator=(const HdrMetadataAgtmParsed&) = delete;
  ~HdrMetadataAgtmParsed();

  bool Parse(const HdrMetadataAgtm& agtm);

  // Compute the alternate indices and their weights for tone mapping targeting
  // H_target. If w_i==0, then i may be kBaselineIndex, indicating that it does
  // not refer to a valid alternate representation. Likewise for w_j. If i is
  // kBaselineIndex then j is also kBaselineIndex.
  static constexpr size_t kBaselineIndex = -1;
  void ComputeAlternateWeights(float H_target,
                               size_t& i,
                               float& w_i,
                               size_t& j,
                               float& w_j) const;

  // Luminance of HDR reference white in nits.
  float hdr_reference_white = 0.f;

  // Baseline representation HDR headroom.
  float baseline_hdr_headroom = 0.f;

  // The maximum component of the baseline image in the gain application space,
  // used to convert from gain application space to [0,1] for evaluation.
  float baseline_max_component = 0.f;

  // Gain parameters to convert from [0,1] to real values.
  float gain_min = 0.f;
  float gain_span = 0.f;

  // Gain application parameters.
  float gain_application_offset = 0.f;
  sk_sp<SkColorSpace> gain_application_color_space;

  // Alternate representations' headrooms and gain curves.
  struct Alternate {
    Alternate();
    Alternate(const Alternate&);
    Alternate(Alternate&&);
    Alternate& operator=(const Alternate&);
    Alternate& operator=(Alternate&&);
    ~Alternate();

    // HDR headroom for the alternate representation.
    float hdr_headroom;

    // Component mixing function and gain curve parameters.
    SkColor4f mix_rgbx{0.f, 0.f, 0.f, 0.f};
    SkColor4f mix_Mmcx{1.f, 0.f, 0.f, 0.f};
    sk_sp<SkImage> curve;
  };
  std::vector<Alternate> alternates;
};

}  // namespace gfx

#endif  // UI_GFX_HDR_METADATA_AGTM_H_
