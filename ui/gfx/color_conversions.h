// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_CONVERSIONS_H_
#define UI_GFX_COLOR_CONVERSIONS_H_

#include <optional>
#include <tuple>

#include "base/component_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {

// All the methods below are exposed for blink::color conversions.

COMPONENT_EXPORT(GFX)
std::tuple<float, float, float> LabToXYZD50(float l, float a, float b);

COMPONENT_EXPORT(GFX)
std::tuple<float, float, float> XYZD50ToLab(float x, float y, float z);

COMPONENT_EXPORT(GFX)
std::tuple<float, float, float> OklabToXYZD50(float l,
                                              float a,
                                              float b,
                                              bool gamut_map = false);

COMPONENT_EXPORT(GFX)
std::tuple<float, float, float> XYZD50ToOklab(float x, float y, float z);

COMPONENT_EXPORT(GFX)
std::tuple<float, float, float> LchToLab(float l, float c, float h);

COMPONENT_EXPORT(GFX)
std::tuple<float, float, float> LabToLch(float l, float a, float b);

COMPONENT_EXPORT(GFX)
std::tuple<float, float, float> SRGBToSRGBLegacy(float r, float g, float b);

COMPONENT_EXPORT(GFX)
std::tuple<float, float, float> SRGBLegacyToSRGB(float r, float g, float b);

COMPONENT_EXPORT(GFX)
std::tuple<float, float, float> XYZD50ToSRGB(float x, float y, float z);

COMPONENT_EXPORT(GFX)
std::tuple<float, float, float> SRGBToXYZD50(float r, float g, float b);

COMPONENT_EXPORT(GFX)
std::tuple<float, float, float> HSLToSRGB(float h, float s, float l);

COMPONENT_EXPORT(GFX)
std::tuple<float, float, float> SRGBToHSL(float r, float g, float b);

COMPONENT_EXPORT(GFX)
std::tuple<float, float, float> HWBToSRGB(float h, float w, float b);

COMPONENT_EXPORT(GFX)
std::tuple<float, float, float> SRGBToHWB(float r, float g, float b);

}  // namespace gfx

#endif  // UI_GFX_COLOR_CONVERSIONS_H_
