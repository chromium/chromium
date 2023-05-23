// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_HDR_STATIC_METADATA_H_
#define UI_GFX_HDR_STATIC_METADATA_H_

#include <cstdint>
#include <vector>

#include "base/numerics/safe_conversions.h"
#include "ui/gfx/color_space_export.h"

namespace gfx {

// This structure is used to define the HDR static capabilities of a display.
// Reflects CEA 861.G-2018, Sec.7.5.13, "HDR Static Metadata Data Block"
// A value of 0.0 in any of the fields means that it's not indicated.
struct COLOR_SPACE_EXPORT HDRStaticMetadata {
  // See Table 43 Data Byte 1 - Electro-Optical Transfer Function
  enum class Eotf {
    // "If “Traditional Gamma - SDR Luminance Range” is indicated, then the
    // maximum encoded luminance is typically mastered to 100 cd/m2"
    kGammaSdrRange = 0,
    // "If “Traditional Gamma – HDR Luminance Range” is indicated, then the
    // maximum encoded luminance is understood to be the maximum luminance of
    // the Sink device."
    kGammaHdrRange = 1,
    kPq = 2,
    kHlg = 3,
  };

  // "Desired Content Max Luminance Data. This is the content’s absolute peak
  // luminance (in cd/m2) (likely only in a small area of the screen) that the
  // display prefers for optimal content rendering."
  double max;
  // "Desired Content Max Frame-average Luminance. This is the content’s max
  // frame-average luminance (in cd/m2) that the display prefers for optimal
  // content rendering."
  double max_avg;
  // "Desired Content Min Luminance. This is the minimum value of the content
  // (in cd/m2) that the display prefers for optimal content rendering."
  double min;
  // "Electro-Optical Transfer Functions supported by the Sink." See Table 85
  // Supported Electro-Optical Transfer Function.
  uint8_t supported_eotf_mask;

  HDRStaticMetadata();
  HDRStaticMetadata(double max,
                    double max_avg,
                    double min,
                    uint8_t supported_eotf_mask);
  HDRStaticMetadata(const HDRStaticMetadata& rhs);
  HDRStaticMetadata& operator=(const HDRStaticMetadata& rhs);

  bool operator==(const HDRStaticMetadata& rhs) const {
    return ((max == rhs.max) && (max_avg == rhs.max_avg) && (min == rhs.min) &&
            supported_eotf_mask == rhs.supported_eotf_mask);
  }

  bool IsEotfSupported(Eotf eotf) const {
    return (supported_eotf_mask & EotfMask({eotf})) != 0;
  }

  static uint8_t EotfMask(std::vector<Eotf> eotfs) {
    int eotf_mask = 0;
    for (const Eotf eotf : eotfs) {
      eotf_mask |= (1 << static_cast<int>(eotf));
    }
    return base::checked_cast<uint8_t>(eotf_mask);
  }
};

}  // namespace gfx

#endif  // UI_GFX_HDR_STATIC_METADATA_H_
