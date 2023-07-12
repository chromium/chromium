// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SHADOW_VALUE_H_
#define UI_GFX_SHADOW_VALUE_H_

#include <string>
#include <tuple>
#include <vector>

#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

class Insets;

class ShadowValue;

typedef std::vector<ShadowValue> ShadowValues;

// ShadowValue encapsulates parameters needed to define a shadow, including the
// shadow's offset, blur amount and color.
class GFX_EXPORT ShadowValue {
 public:
  constexpr ShadowValue() = default;
  constexpr ShadowValue(const gfx::Vector2d& offset, double blur, SkColor color)
      : offset_(offset), blur_(blur), color_(color) {}

  constexpr int x() const { return offset_.x(); }
  constexpr int y() const { return offset_.y(); }
  constexpr const gfx::Vector2d& offset() const { return offset_; }
  constexpr double blur() const { return blur_; }
  constexpr SkColor color() const { return color_; }

  constexpr bool operator==(const ShadowValue& other) const {
    return offset_ == other.offset_ && blur_ == other.blur_ &&
           color_ == other.color_;
  }

  constexpr bool operator<(const ShadowValue& other) const {
    return std::make_tuple(x(), y(), blur_, color_) <
           std::make_tuple(other.x(), other.y(), other.blur_, other.color_);
  }

  ShadowValue Scale(float scale) const;

  std::string ToString() const;

  // Gets margin space needed for shadows. Note that values in returned Insets
  // are negative because shadow margins are outside a boundary.
  static Insets GetMargin(const ShadowValues& shadows);

  // Gets the area inside a rectangle that would be affected by shadow blur.
  // This is similar to the margin except it's positive (the blur region is
  // inside a hypothetical rectangle) and it accounts for the blur both inside
  // and outside the bounding box. The region inside the "blur region" would be
  // a uniform color.
  static Insets GetBlurRegion(const ShadowValues& shadows);

  // Makes ShadowValues for the given elevation and color. Calls to
  // MakeShadowValues that expect to fallback to MakeMdShadowValues should pass
  // in the same base color for |key_shadow_color| and |ambient_shadow_color|
  // until MakeMdShadowValues is refactored to remove SkColorSetA calls and also
  // take in its own |key_shadow_color| and |ambient_shadow_color|.
  // TODO(elainechien): crbug.com/1056950.
  static ShadowValues MakeShadowValues(int elevation,
                                       SkColor key_shadow_color,
                                       SkColor ambient_shadow_color);
  // Makes ShadowValues for MD shadows. This style is deprecated.
  static ShadowValues MakeMdShadowValues(int elevation,
                                         SkColor color = SK_ColorBLACK);
  // Makes ShadowValues for MD shadows with customized key and ambient colors.
  static ShadowValues MakeMdShadowValues(int elevation,
                                         SkColor key_shadow_color,
                                         SkColor ambient_shadow_color);

#if BUILDFLAG(IS_CHROMEOS)
  // Makes ShadowValues for Chrome OS UI components with default colors.
  static ShadowValues MakeChromeOSSystemUIShadowValues(
      int elevation,
      SkColor color = SK_ColorBLACK);
  // Makes ShadowValues for chrome OS UI components with customized key and
  // ambient colors.
  static ShadowValues MakeChromeOSSystemUIShadowValues(
      int elevation,
      SkColor key_shadow_color,
      SkColor ambient_shadow_color);
#endif

 private:
  gfx::Vector2d offset_;

  // Blur amount of the shadow in pixels. If underlying implementation supports
  // (e.g. Skia), it can have fraction part such as 0.5 pixel. The value
  // defines a range from full shadow color at the start point inside the
  // shadow to fully transparent at the end point outside it. The range is
  // perpendicular to and centered on the shadow edge. For example, a blur
  // amount of 4.0 means to have a blurry shadow edge of 4 pixels that
  // transitions from full shadow color to fully transparent and with 2 pixels
  // inside the shadow and 2 pixels goes beyond the edge.
  double blur_ = 0.;

  SkColor color_ = SK_ColorTRANSPARENT;
};

}  // namespace gfx

#endif  // UI_GFX_SHADOW_VALUE_H_
