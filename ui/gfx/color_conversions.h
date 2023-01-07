// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_CONVERSIONS_H_
#define UI_GFX_COLOR_CONVERSIONS_H_

#include <tuple>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

// Method exposed for testing purposes.
GFX_EXPORT std::tuple<float, float, float> LabToXYZD50(float l,
                                                       float a,
                                                       float b);

// Method exposed for testing purposes.
GFX_EXPORT std::tuple<float, float, float> OKLabToXYZD65(float l,
                                                         float a,
                                                         float b);

// Method exposed for testing purposes.
GFX_EXPORT std::tuple<float, float, float> DisplayP3ToXYZD50(float r,
                                                             float g,
                                                             float b);

// Method exposed for testing purposes.
GFX_EXPORT std::tuple<float, float, float> XYZD50ToDisplayP3(float x,
                                                             float y,
                                                             float z);

// Method exposed for testing purposes.
GFX_EXPORT std::tuple<float, float, float> ProPhotoToXYZD50(float r,
                                                            float g,
                                                            float b);

// Method exposed for testing purposes.
GFX_EXPORT std::tuple<float, float, float> AdobeRGBToXYZD50(float r,
                                                            float g,
                                                            float b);

// Method exposed for testing purposes.
GFX_EXPORT std::tuple<float, float, float> XYZD50ToAdobeRGB(float x,
                                                            float y,
                                                            float z);

// Method exposed for testing purposes.
GFX_EXPORT std::tuple<float, float, float> Rec2020ToXYZD50(float r,
                                                           float g,
                                                           float b);

// Method exposed for testing purposes.
GFX_EXPORT std::tuple<float, float, float> XYZD50ToRec2020(float x,
                                                           float y,
                                                           float z);

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

// Method exposed for testing purposes.
GFX_EXPORT std::tuple<float, float, float> LchToLab(float l,
                                                    float c,
                                                    absl::optional<float> h);

// Method exposed for testing purposes.
GFX_EXPORT std::tuple<float, float, float> LabToLCH(float l, float a, float b);

// Method exposed for testing purposes.
GFX_EXPORT std::tuple<float, float, float> OKLchToLab(float l,
                                                      float c,
                                                      absl::optional<float> h);

// Method exposed for blink::color conversions.
GFX_EXPORT SkColor4f XYZD50ToSkColor4f(float x, float y, float z, float alpha);

// Method exposed for blink::color conversions.
GFX_EXPORT SkColor4f XYZD65ToSkColor4f(float x, float y, float z, float alpha);

// Method exposed for blink::color conversions.
GFX_EXPORT SkColor4f LabToSkColor4f(float l, float a, float b, float alpha);

// Method exposed for blink::color conversions.
GFX_EXPORT SkColor4f OKLabToSkColor4f(float l, float a, float b, float alpha);

// Method exposed for blink::color conversions.
GFX_EXPORT SkColor4f SRGBLinearToSkColor4f(float r,
                                           float g,
                                           float b,
                                           float alpha);

// Method exposed for blink::color conversions.
GFX_EXPORT SkColor4f ProPhotoToSkColor4f(float r,
                                         float g,
                                         float b,
                                         float alpha);

// Method exposed for blink::color conversions.
GFX_EXPORT SkColor4f DisplayP3ToSkColor4f(float r,
                                          float g,
                                          float b,
                                          float alpha);

// Method exposed for blink::color conversions.
GFX_EXPORT SkColor4f AdobeRGBToSkColor4f(float r,
                                         float g,
                                         float b,
                                         float alpha);

// Method exposed for blink::color conversions.
GFX_EXPORT SkColor4f Rec2020ToSkColor4f(float r, float g, float b, float alpha);

// Method exposed for blink::color conversions.
GFX_EXPORT SkColor4f LchToSkColor4f(float l,
                                    float a,
                                    absl::optional<float> b,
                                    float alpha);

// Method exposed for blink::color conversions.
GFX_EXPORT SkColor4f OKLchToSkColor4f(float l,
                                      float a,
                                      absl::optional<float> b,
                                      float alpha);

}  // namespace gfx

#endif  // UI_GFX_COLOR_CONVERSIONS_H_
