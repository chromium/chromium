// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_CONVERSIONS_H_
#define UI_GFX_COLOR_CONVERSIONS_H_

#include <tuple>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

// Method exposed for testing purposes.
GFX_EXPORT std::tuple<float, float, float> LabToXYZD50(float l,
                                                       float a,
                                                       float b);

// Method exposed for testing purposes.
GFX_EXPORT std::tuple<float, float, float> XYZD50toD65(float x,
                                                       float y,
                                                       float z);

// Method exposed for testing purposes.
GFX_EXPORT std::tuple<float, float, float> XYZD65tosRGBLinear(float x,
                                                              float y,
                                                              float z);

// Method exposed for testing purposes.
GFX_EXPORT std::tuple<float, float, float> XYZD50tosRGBLinear(float r,
                                                              float g,
                                                              float b);

// Method exposed for blink::color conversions.
GFX_EXPORT SkColor4f LabToSkColor4f(float l, float a, float b, float alpha);

}  // namespace gfx

#endif  // UI_GFX_COLOR_CONVERSIONS_H_
