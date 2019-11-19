// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BACKGROUND_H_
#define UI_VIEWS_BACKGROUND_H_

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/views_export.h"

#if defined(OS_WIN)
#include <windows.h>
#endif  // defined(OS_WIN)

namespace gfx {
class Canvas;
}

namespace views {

class Painter;
class View;

/////////////////////////////////////////////////////////////////////////////
//
// Background class
//
// A background implements a way for views to paint a background. The
// background can be either solid or based on a gradient. Of course,
// Background can be subclassed to implement various effects.
//
// Any View can have a background. See View::SetBackground() and
// View::OnPaintBackground()
//
/////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT Background {
 public:
  Background();
  virtual ~Background();

  // Render the background for the provided view
  virtual void Paint(gfx::Canvas* canvas, View* view) const = 0;

  // Set a solid, opaque color to be used when drawing backgrounds of native
  // controls.  Unfortunately alpha=0 is not an option.
  void SetNativeControlColor(SkColor color);

  // Returns the "background color".  This is equivalent to the color set in
  // SetNativeControlColor().  For solid backgrounds, this is the color; for
  // gradient backgrounds, it's the midpoint of the gradient; for painter
  // backgrounds, this is not useful (returns a default color).
  SkColor get_color() const { return color_; }

 private:
  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(Background);
};

// Creates a background that fills the canvas in the specified color.
VIEWS_EXPORT std::unique_ptr<Background> CreateSolidBackground(SkColor color);

// Creates a background that fills the canvas with rounded corners.
VIEWS_EXPORT std::unique_ptr<Background> CreateRoundedRectBackground(
    SkColor color,
    float radius);

// Creates a background that fills the canvas in the color specified by the
// view's NativeTheme and the given color identifier.
VIEWS_EXPORT std::unique_ptr<Background> CreateThemedSolidBackground(
    View* view,
    ui::NativeTheme::ColorId color_id);

// Creates Chrome's standard panel background
VIEWS_EXPORT std::unique_ptr<Background> CreateStandardPanelBackground();

// Creates a Background from the specified Painter.
VIEWS_EXPORT std::unique_ptr<Background> CreateBackgroundFromPainter(
    std::unique_ptr<Painter> painter);

}  // namespace views

#endif  // UI_VIEWS_BACKGROUND_H_
