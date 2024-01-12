// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LINUX_WINDOW_FRAME_PROVIDER_H_
#define UI_LINUX_WINDOW_FRAME_PROVIDER_H_

#include "ui/base/ui_base_types.h"

namespace gfx {
class Canvas;
class Insets;
class Rect;
}  // namespace gfx

namespace ui {

class WindowFrameProvider {
 public:
  virtual ~WindowFrameProvider() = default;

  // Returns the radius of the top-left and top-right corners in DIPs.  Used
  // only as a hint to the compositor so it knows to redraw the part of the
  // window behind the corners.
  virtual int GetTopCornerRadiusDip() = 0;

  // Returns true iff any parts of the top frame are translucent.
  virtual bool IsTopFrameTranslucent() = 0;

  // Returns the shadow and border drawn around the window in DIPs.
  virtual gfx::Insets GetFrameThicknessDip() = 0;

  // Draws a native window border and shadow.  |rect| is the bounds of the
  // window.  The decoration will be drawn outside of that region.
  virtual void PaintWindowFrame(gfx::Canvas* canvas,
                                const gfx::Rect& rect,
                                int top_area_height,
                                bool focused,
                                const gfx::Insets& input_insets) = 0;
};

}  // namespace ui

#endif  // UI_LINUX_WINDOW_FRAME_PROVIDER_H_
