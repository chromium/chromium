// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/shadow_value.h"

#include <stddef.h>

#include <algorithm>

#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace gfx {

namespace {

// To match the CSS notion of blur (spread outside the bounding box) to the
// Skia notion of blur (spread outside and inside the bounding box), we have
// to double the designer-provided blur values.
constexpr int kBlurCorrection = 2;

Insets GetInsets(const ShadowValues& shadows, bool include_inner_blur) {
  int left = 0;
  int top = 0;
  int right = 0;
  int bottom = 0;

  for (size_t i = 0; i < shadows.size(); ++i) {
    const ShadowValue& shadow = shadows[i];

    double blur = shadow.blur();
    if (!include_inner_blur)
      blur /= 2;
    int blur_length = base::ClampRound(blur);

    left = std::max(left, blur_length - shadow.x());
    top = std::max(top, blur_length - shadow.y());
    right = std::max(right, blur_length + shadow.x());
    bottom = std::max(bottom, blur_length + shadow.y());
  }

  return Insets::TLBR(top, left, bottom, right);
}

}  // namespace

ShadowValue ShadowValue::Scale(float scale) const {
  Vector2d scaled_offset = ToFlooredVector2d(ScaleVector2d(offset_, scale));
  return ShadowValue(scaled_offset, blur_ * scale, color_);
}

std::string ShadowValue::ToString() const {
  return base::StringPrintf(
      "(%d,%d),%.2f,rgba(%d,%d,%d,%d)",
      offset_.x(), offset_.y(),
      blur_,
      SkColorGetR(color_),
      SkColorGetG(color_),
      SkColorGetB(color_),
      SkColorGetA(color_));
}

// static
Insets ShadowValue::GetMargin(const ShadowValues& shadows) {
  Insets margins = GetInsets(shadows, false);
  return -margins;
}

// static
Insets ShadowValue::GetBlurRegion(const ShadowValues& shadows) {
  return GetInsets(shadows, true);
}

// static
ShadowValues ShadowValue::MakeShadowValues(int elevation,
                                           SkColor key_shadow_color,
                                           SkColor ambient_shadow_color) {
  // Refresh uses hand-tweaked shadows corresponding to a small set of
  // elevations. Use the Refresh spec and designer input to add missing shadow
  // values.

  switch (elevation) {
    case 3: {
      ShadowValue key = {Vector2d(0, 1), 12, key_shadow_color};
      ShadowValue ambient = {Vector2d(0, 4), 64, ambient_shadow_color};
      return {key, ambient};
    }
    case 16: {
      ShadowValue key = {Vector2d(0, 0), kBlurCorrection * 16,
                         key_shadow_color};
      ShadowValue ambient = {Vector2d(0, 12), kBlurCorrection * 16,
                             ambient_shadow_color};
      return {key, ambient};
    }
    default:
      // This surface has not been updated for Refresh. Fall back to the
      // deprecated style.
      DCHECK_EQ(key_shadow_color, ambient_shadow_color);
      return MakeMdShadowValues(elevation, key_shadow_color);
  }
}

// static
ShadowValues ShadowValue::MakeMdShadowValues(int elevation, SkColor color) {
  // To see what this looks like for elevation 24, try this CSS:
  //   box-shadow: 0 24px 48px rgba(0, 0, 0, .24),
  //               0 0 24px rgba(0, 0, 0, .12);
  return MakeMdShadowValues(elevation, SkColorSetA(color, 0x3d),
                            SkColorSetA(color, 0x1f));
}

// static
ShadowValues ShadowValue::MakeMdShadowValues(int elevation,
                                             SkColor key_shadow_color,
                                             SkColor ambient_shadow_color) {
  ShadowValues shadow_values;
  // "Key shadow": y offset is elevation and blur is twice the elevation.
  shadow_values.emplace_back(Vector2d(0, elevation),
                             kBlurCorrection * elevation * 2, key_shadow_color);
  // "Ambient shadow": no offset and blur matches the elevation.
  shadow_values.emplace_back(Vector2d(), kBlurCorrection * elevation,
                             ambient_shadow_color);
  return shadow_values;
}

#if BUILDFLAG(IS_CHROMEOS)
// static
ShadowValues ShadowValue::MakeChromeOSSystemUIShadowValues(int elevation,
                                                           SkColor color) {
  // To see what this looks like for elevation 24, try this CSS:
  //   box-shadow: 0 24px 24px rgba(0, 0, 0, .24),
  //               0 0 24px rgba(0, 0, 0, .10);
  return MakeChromeOSSystemUIShadowValues(elevation, SkColorSetA(color, 0x3d),
                                          SkColorSetA(color, 0x1a));
}

// static
ShadowValues ShadowValue::MakeChromeOSSystemUIShadowValues(
    int elevation,
    SkColor key_shadow_color,
    SkColor ambient_shadow_color) {
  ShadowValues shadow_values;
  // "Key shadow": y offset is elevation and blur equals to the elevation.
  shadow_values.emplace_back(Vector2d(0, elevation),
                             kBlurCorrection * elevation, key_shadow_color);
  // "Ambient shadow": no offset and blur matches the elevation.
  shadow_values.emplace_back(Vector2d(), kBlurCorrection * elevation,
                             ambient_shadow_color);
  return shadow_values;
}
#endif

}  // namespace gfx
