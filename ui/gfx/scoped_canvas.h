// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SCOPED_CANVAS_H_
#define UI_GFX_SCOPED_CANVAS_H_

#include "base/macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

// Saves the drawing state, and restores the state when going out of scope.
class GFX_EXPORT ScopedCanvas {
 public:
  explicit ScopedCanvas(gfx::Canvas* canvas);
  ScopedCanvas(const ScopedCanvas&) = delete;
  ScopedCanvas& operator=(const ScopedCanvas&) = delete;
  virtual ~ScopedCanvas();

  // If the UI is in RTL layout, applies a transform such that anything drawn
  // inside the supplied width will be flipped horizontally.
  void FlipIfRTL(int width);

 private:
  gfx::Canvas* canvas_;
};

}  // namespace gfx

#endif  // UI_GFX_SCOPED_CANVAS_H_
