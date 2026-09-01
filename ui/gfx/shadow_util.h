// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SHADOW_UTIL_H_
#define UI_GFX_SHADOW_UTIL_H_

#include "base/component_export.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/outsets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/shadow_value.h"

namespace gfx {

// A struct that describes a vector of shadows and their depiction as an image
// suitable for ninebox tiling.
struct COMPONENT_EXPORT(GFX) ShadowDetails {
  ShadowDetails(const gfx::ShadowValues& values,
                const gfx::ImageSkia& nine_patch_image);

  ShadowDetails(const ShadowDetails& other);
  ShadowDetails& operator=(const ShadowDetails& other);

  ShadowDetails(ShadowDetails&& other);
  ShadowDetails& operator=(ShadowDetails&& other);

  ~ShadowDetails();

  bool operator==(const ShadowDetails& other) const;

  // Returns a cached ShadowDetails for the given elevation and rounded corners.
  // Creates the ShadowDetails first if necessary.
  static const ShadowDetails& Get(int elevation,
                                  const gfx::RoundedCornersF& rounded_corners,
                                  bool is_pill_shaped = false);

  // Returns a cached ShadowDetails for given corner radius and shadow values.
  static const ShadowDetails& Get(const gfx::RoundedCornersF& rounded_corners,
                                  const gfx::ShadowValues& values);

  // Returns the insets required to accommodate the corner radii.
  //
  // Left Inset = max(r_UL, r_LL)
  // ◄─────►
  // (r_UL: Large)                                         (r_UR: Medium)
  //       ╭──────────────────────────────────────────────────╮▲
  //    ╭──╯                                                  ││ Top Inset =
  //  ╭─╯                                                     ││  max(r_UL,
  // ╭╯                                                       │▼     r_UR)
  // │                                                        │
  // │                                                        │
  // │                                                        │
  // │                                                        │▲ Bottom Inset =
  // ╰──╮                                                     ││ max(r_LL,r_LR)
  //    ╰─────────────────────────────────────────────────────┘▼
  // (r_LL: Small)                                         (r_LR: Sharp)
  //                                                      ◄──►
  //                                          Right Inset =  max(r_UR, r_LR)
  //
  static gfx::Insets GetInsetsForRoundedCorners(
      const gfx::RoundedCornersF& rounded_corners);

  // Returns the insets for the ninebox aperture given the shadows and corner
  // radius. Represents the total space need to draw  the full range of blur and
  // the corner rounding around the aperture.
  static gfx::Insets GetNineboxApertureInsets(
      const gfx::ShadowValues& shadows,
      const gfx::RoundedCornersF& rounded_corners);

  static size_t GetDetailsCacheSizeForTest();

  // Description of the shadows.
  gfx::ShadowValues values;
  // Cached ninebox image based on |values|.
  gfx::ImageSkia nine_patch_image;
};

}  // namespace gfx

#endif  // UI_GFX_SHADOW_UTIL_H_
