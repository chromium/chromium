// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SHADOW_UTIL_H_
#define UI_GFX_SHADOW_UTIL_H_

#include "build/build_config.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/shadow_value.h"

namespace gfx {

// The shadow style for different UI components.
enum class ShadowStyle {
  // The MD style is mainly used for view's shadow.
  kMaterialDesign,
#if BUILDFLAG(IS_CHROMEOS)
  // The system style is mainly used for Chrome OS UI components.
  kChromeOSSystemUI,
#endif
};

// A struct that describes a vector of shadows and their depiction as an image
// suitable for ninebox tiling.
struct GFX_EXPORT ShadowDetails {
  ShadowDetails(const gfx::ShadowValues& values,
                const gfx::ImageSkia& nine_patch_image);
  ShadowDetails(const ShadowDetails& other);
  ~ShadowDetails();

  // Returns a cached ShadowDetails for the given elevation, corner radius, and
  // shadow style. Creates the ShadowDetails first if necessary.
  static const ShadowDetails& Get(
      int elevation,
      int radius,
      ShadowStyle style = ShadowStyle::kMaterialDesign);
  // Returns a cached ShadowDetails for the given elevation, corner radius,
  // key shadow color, ambient shadow color, and shadow style.
  static const ShadowDetails& Get(int elevation,
                                  int radius,
                                  SkColor key_color,
                                  SkColor ambient_color,
                                  ShadowStyle style);
  // Returns a cached ShadowDetails for given corner radius and shadow values.
  static const ShadowDetails& Get(int radius, const gfx::ShadowValues& values);

  static size_t GetDetailsCacheSizeForTest();

  // Description of the shadows.
  const gfx::ShadowValues values;
  // Cached ninebox image based on |values|.
  const gfx::ImageSkia nine_patch_image;
};

}  // namespace gfx

#endif  // UI_GFX_SHADOW_UTIL_H_
