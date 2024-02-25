// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_CONVERSIONS_H_
#define UI_GFX_COLOR_CONVERSIONS_H_

#include <optional>
#include <tuple>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

// All the methods below are exposed for blink::color conversions.

GFX_EXPORT std::tuple<float, float, float> LabToXYZD50(float l,
                                                       float a,
                                                       float b);

GFX_EXPORT std::tuple<float, float, float> XYZD50ToLab(float x,
                                                       float y,
                                                       float z);

GFX_EXPORT std::tuple<float, float, float> OklabToXYZD65(float l,
                                                         float a,
                                                         float b);

GFX_EXPORT std::tuple<float, float, float> XYZD65ToOklab(float x,
                                                         float y,
                                                         float z);

GFX_EXPORT std::tuple<float, float, float> LchToLab(float l, float c, float h);

GFX_EXPORT std::tuple<float, float, float> LabToLch(float l, float a, float b);

GFX_EXPORT std::tuple<float, float, float> DisplayP3ToXYZD50(float r,
                                                             float g,
                                                             float b);

GFX_EXPORT std::tuple<float, float, float> XYZD50ToDisplayP3(float x,
                                                             float y,
                                                             float z);

GFX_EXPORT std::tuple<float, float, float> ProPhotoToXYZD50(float r,
                                                            float g,
                                                            float b);

GFX_EXPORT std::tuple<float, float, float> XYZD50ToProPhoto(float x,
                                                            float y,
                                                            float z);

GFX_EXPORT std::tuple<float, float, float> AdobeRGBToXYZD50(float r,
                                                            float g,
                                                            float b);

GFX_EXPORT std::tuple<float, float, float> XYZD50ToAdobeRGB(float x,
                                                            float y,
                                                            float z);

GFX_EXPORT std::tuple<float, float, float> Rec2020ToXYZD50(float r,
                                                           float g,
                                                           float b);

GFX_EXPORT std::tuple<float, float, float> XYZD50ToRec2020(float x,
                                                           float y,
                                                           float z);

GFX_EXPORT std::tuple<float, float, float> XYZD50ToD65(float x,
                                                       float y,
                                                       float z);

GFX_EXPORT std::tuple<float, float, float> XYZD65ToD50(float x,
                                                       float y,
                                                       float z);

GFX_EXPORT std::tuple<float, float, float> XYZD65TosRGBLinear(float x,
                                                              float y,
                                                              float z);

GFX_EXPORT std::tuple<float, float, float> SRGBToSRGBLegacy(float r,
                                                            float g,
                                                            float b);

GFX_EXPORT std::tuple<float, float, float> SRGBLegacyToSRGB(float r,
                                                            float g,
                                                            float b);

GFX_EXPORT std::tuple<float, float, float> XYZD50TosRGB(float x,
                                                        float y,
                                                        float z);

GFX_EXPORT std::tuple<float, float, float> XYZD50TosRGBLinear(float x,
                                                              float y,
                                                              float z);

GFX_EXPORT std::tuple<float, float, float> SRGBLinearToXYZD50(float r,
                                                              float g,
                                                              float b);

GFX_EXPORT std::tuple<float, float, float> SRGBToXYZD50(float r,
                                                        float g,
                                                        float b);

GFX_EXPORT std::tuple<float, float, float> HSLToSRGB(float h, float s, float l);
GFX_EXPORT std::tuple<float, float, float> SRGBToHSL(float r, float g, float b);

GFX_EXPORT std::tuple<float, float, float> HWBToSRGB(float h, float w, float b);
GFX_EXPORT std::tuple<float, float, float> SRGBToHWB(float r, float g, float b);

GFX_EXPORT SkColor4f XYZD50ToSkColor4f(float x, float y, float z, float alpha);

GFX_EXPORT SkColor4f XYZD65ToSkColor4f(float x, float y, float z, float alpha);

GFX_EXPORT SkColor4f LabToSkColor4f(float l, float a, float b, float alpha);

GFX_EXPORT SkColor4f OklabToSkColor4f(float l, float a, float b, float alpha);

GFX_EXPORT SkColor4f OklabGamutMapToSkColor4f(float l,
                                              float a,
                                              float b,
                                              float alpha);

GFX_EXPORT SkColor4f LchToSkColor4f(float l, float a, float b, float alpha);

GFX_EXPORT SkColor4f OklchToSkColor4f(float l, float a, float h, float alpha);

GFX_EXPORT SkColor4f OklchGamutMapToSkColor4f(float l,
                                              float a,
                                              float h,
                                              float alpha);

GFX_EXPORT SkColor4f SRGBLinearToSkColor4f(float r,
                                           float g,
                                           float b,
                                           float alpha);

GFX_EXPORT SkColor4f ProPhotoToSkColor4f(float r,
                                         float g,
                                         float b,
                                         float alpha);

GFX_EXPORT SkColor4f DisplayP3ToSkColor4f(float r,
                                          float g,
                                          float b,
                                          float alpha);

GFX_EXPORT SkColor4f AdobeRGBToSkColor4f(float r,
                                         float g,
                                         float b,
                                         float alpha);

GFX_EXPORT SkColor4f Rec2020ToSkColor4f(float r, float g, float b, float alpha);

GFX_EXPORT SkColor4f HSLToSkColor4f(float h, float s, float l, float alpha);

GFX_EXPORT SkColor4f HWBToSkColor4f(float h, float w, float b, float alpha);

}  // namespace gfx

#endif  // UI_GFX_COLOR_CONVERSIONS_H_
