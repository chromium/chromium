// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_HDR_STATIC_METADATA_H_
#define UI_GFX_HDR_STATIC_METADATA_H_

#include "ui/gfx/color_space_export.h"

namespace gfx {

// This structure is used to define the HDR static capabilities of a display.
// Reflects CEA 861.G-2018, Sec.7.5.13, "HDR Static Metadata Data Block"
// A value of 0.0 in any of the fields means that it's not indicated.
struct COLOR_SPACE_EXPORT HDRStaticMetadata {
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

  HDRStaticMetadata();
  HDRStaticMetadata(double max, double max_avg, double min);
  HDRStaticMetadata(const HDRStaticMetadata& rhs);
  HDRStaticMetadata& operator=(const HDRStaticMetadata& rhs);

  bool operator==(const HDRStaticMetadata& rhs) const {
    return ((max == rhs.max) && (max_avg == rhs.max_avg) && (min == rhs.min));
  }
};

}  // namespace gfx

#endif  // UI_GFX_HDR_STATIC_METADATA_H_
