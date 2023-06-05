// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SHADOW_UTIL_H_
#define UI_GFX_SHADOW_UTIL_H_

#include "build/chromeos_buildflags.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/shadow_value.h"

namespace gfx {

// The shadow style for different UI components.
enum class ShadowStyle {
  // The MD style is mainly used for view's shadow.
  kMaterialDesign,
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // The system style is mainly used for Chrome OS UI components.
  kChromeOSSystemUI,
#endif
};

// A struct that describes a vector of shadows and their depiction as an image
// suitable for ninebox tiling.
struct GFX_EXPORT ShadowDetails {
  ShadowDetails();
  ShadowDetails(const ShadowDetails& other);
  ~ShadowDetails();

  // Returns a cached ShadowDetails for the given elevation, corner radius, and
  // shadow style. Creates the ShadowDetails first if necessary.
  static const ShadowDetails& Get(
      int elevation,
      int radius,
      ShadowStyle style = ShadowStyle::kMaterialDesign);

  static size_t GetDetailsCacheSizeForTest();

  // Description of the shadows.
  gfx::ShadowValues values;
  // Cached ninebox image based on |values|.
  gfx::ImageSkia ninebox_image;
};

}  // namespace gfx

#endif  // UI_GFX_SHADOW_UTIL_H_
