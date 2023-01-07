// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_NATIVE_THEME_DELEGATE_H_
#define UI_VIEWS_NATIVE_THEME_DELEGATE_H_

#include "ui/gfx/geometry/rect.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/views_export.h"

namespace views {

// A delagate that supports animating transtions between different native
// theme states.  This delegate can be used to control a native theme Border
// or Painter object.
//
// If animation is ongoing, the native theme border or painter will
// composite the foreground state over the backgroud state using an alpha
// between 0 and 255 based on the current value of the animation.
class VIEWS_EXPORT NativeThemeDelegate {
 public:
  virtual ~NativeThemeDelegate() = default;

  // Get the native theme part that should be drawn.
  virtual ui::NativeTheme::Part GetThemePart() const = 0;

  // Get the rectangle that should be painted.
  virtual gfx::Rect GetThemePaintRect() const = 0;

  // Get the state of the part, along with any extra data needed for drawing.
  virtual ui::NativeTheme::State GetThemeState(
      ui::NativeTheme::ExtraParams* params) const = 0;

  // If the native theme drawign should be animated, return the Animation object
  // that controlls it.  If no animation is ongoing, NULL may be returned.
  virtual const gfx::Animation* GetThemeAnimation() const = 0;

  // If animation is onging, this returns the background native theme state.
  virtual ui::NativeTheme::State GetBackgroundThemeState(
      ui::NativeTheme::ExtraParams* params) const = 0;

  // If animation is onging, this returns the foreground native theme state.
  // This state will be composited over the background using an alpha value
  // based on the current value of the animation.
  virtual ui::NativeTheme::State GetForegroundThemeState(
      ui::NativeTheme::ExtraParams* params) const = 0;
};

}  // namespace views

#endif  // UI_VIEWS_NATIVE_THEME_DELEGATE_H_
