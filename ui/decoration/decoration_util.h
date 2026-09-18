// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DECORATION_DECORATION_UTIL_H_
#define UI_DECORATION_DECORATION_UTIL_H_

#include <cstddef>

#include "build/build_config.h"
#include "ui/decoration/decoration_details.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/outsets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/shadow_value.h"

namespace gfx {
class Canvas;
}  // namespace gfx

namespace ui::decoration {

// Generator for shadow decoration rendering and insets.
struct ShadowGenerator {
  static gfx::Insets GetMargins(const gfx::ShadowValues& shadows);

  // Returns the insets for the ninebox aperture given the shadows and corner
  // radius. Represents the total space need to draw the full range of blur and
  // the corner rounding around the aperture.
  static gfx::Insets GetNineboxApertureInsets(
      const gfx::ShadowValues& shadows,
      const gfx::RoundedCornersF& rounded_corners);

  static void Draw(gfx::Canvas* canvas,
                   const gfx::ShadowValues& shadows,
                   const gfx::RoundedCornersF& rounded_corners,
                   const gfx::Rect& content_rect);
};

using ShadowDetails = DecorationDetails<gfx::ShadowValues, ShadowGenerator>;

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
gfx::Insets GetInsetsForRoundedCorners(
    const gfx::RoundedCornersF& rounded_corners);

}  // namespace ui::decoration

#endif  // UI_DECORATION_DECORATION_UTIL_H_
