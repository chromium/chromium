// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MAC_COCOA_SCROLLBAR_PAINTER_H_
#define UI_GFX_MAC_COCOA_SCROLLBAR_PAINTER_H_

#include "cc/paint/paint_canvas.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

class GFX_EXPORT CocoaScrollbarPainter {
 public:
  enum class Orientation {
    // Vertical scrollbar on the right side of content.
    kVerticalOnRight,
    // Vertical scrollbar on the left side of content.
    kVerticalOnLeft,
    // Horizontal scrollbar (on the bottom of content).
    kHorizontal,
  };

  struct Params {
    // The orientation of the scrollbar.
    Orientation orientation = Orientation::kVerticalOnRight;

    // Whether or not this is an overlay scrollbar.
    bool overlay = false;

    // Scrollbars change color in dark mode.
    bool dark_mode = false;

    // Non-overlay scrollbars change thumb color when they are hovered (or
    // pressed).
    bool hovered = false;
  };

  // Paint the thumb. The |thumb_bounds| changes over time when the thumb
  // engorges during hover.
  static void PaintThumb(cc::PaintCanvas* canvas,
                         const SkIRect& thumb_bounds,
                         const Params& params);
  // Paint the track. |track_bounds| is the bounds for the track.
  static void PaintTrack(cc::PaintCanvas* canvas,
                         const SkIRect& track_bounds,
                         const Params& params);
  // Paint the corner. |corner_bounds| is the bounds for the corner.
  static void PaintCorner(cc::PaintCanvas* canvas,
                          const SkIRect& corner_bounds,
                          const Params& params);
};

}  // namespace gfx

#endif  // UI_GFX_MAC_COCOA_SCROLLBAR_PAINTER_H_
