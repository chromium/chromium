// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_TONE_MAP_UTIL_H_
#define UI_GFX_TONE_MAP_UTIL_H_

#include <optional>

#include "ui/gfx/color_space_export.h"

class SkColorSpace;
class SkImage;
class SkPaint;

namespace gfx {

class ColorSpace;
struct HDRMetadata;

// Helper class for applying tone mapping on the fly in DrawImage and
// DrawImageRect.
class COLOR_SPACE_EXPORT ToneMapUtil {
 public:
  // Return true if images that have the specified color space should be drawn
  // using a tone mapping shader. If `targeted_hdr_headroom` is specified, then
  // determine if the filter is required when drawing at the specified HDR
  // headroom.
  static bool UseGlobalToneMapFilter(
      const SkColorSpace* cs,
      const HDRMetadata& metadata,
      std::optional<float> targeted_hdr_headroom = std::nullopt);
  static bool UseGlobalToneMapFilter(
      const ColorSpace& cs,
      const HDRMetadata& metadata,
      std::optional<float> targeted_hdr_headroom = std::nullopt);

  // Return the maximum HDR headroom that this content will render to.
  static float GetMaxHdrHeadroom(const SkColorSpace* cs,
                                 const HDRMetadata& metadata);
  static float GetMaxHdrHeadroom(const ColorSpace& cs,
                                 const HDRMetadata& metadata);

  // Return true if this content will render with HDR headroom greater than 0.
  static bool IsHDR(const SkColorSpace* cs, const HDRMetadata& metadata);
  static bool IsHDR(const ColorSpace& cs, const HDRMetadata& metadata);

  // Add a color filter to `paint` that will perform tone mapping.
  static void AddGlobalToneMapFilterToPaint(SkPaint& paint,
                                            const SkImage* image,
                                            const HDRMetadata& metadata,
                                            float target_hdr_headroom);
};

}  // namespace gfx

#endif  // UI_GFX_TONE_MAP_UTIL_H_
