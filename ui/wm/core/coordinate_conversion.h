// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_COORDINATE_CONVERSION_H_
#define UI_WM_CORE_COORDINATE_CONVERSION_H_

#include "base/component_export.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class Point;
class PointF;
class Rect;
class RectF;
}  // namespace gfx

namespace wm {

// Converts the |point| from a given |window|'s coordinates into the screen
// coordinates.
// TODO: Remove the integer versions of these functions. See crbug.com/773331.
COMPONENT_EXPORT(UI_WM)
void ConvertPointToScreen(const aura::Window* window, gfx::Point* point);

COMPONENT_EXPORT(UI_WM)
void ConvertPointToScreen(const aura::Window* window, gfx::PointF* point);

// Converts the |point| from the screen coordinates to a given |window|'s
// coordinates.
COMPONENT_EXPORT(UI_WM)
void ConvertPointFromScreen(const aura::Window* window,
                            gfx::Point* point_in_screen);

COMPONENT_EXPORT(UI_WM)
void ConvertPointFromScreen(const aura::Window* window,
                            gfx::PointF* point_in_screen);

// Converts |rect| from |window|'s coordinates to the virtual screen
// coordinates.
// TODO: Change the Rect versions to TranslateRect(To|From)Screen since they
// do not handle size changes.
COMPONENT_EXPORT(UI_WM)
void ConvertRectToScreen(const aura::Window* window, gfx::Rect* rect);

COMPONENT_EXPORT(UI_WM)
void TranslateRectToScreen(const aura::Window* window, gfx::RectF* rect);

// Converts |rect| from virtual screen coordinates to the |window|'s
// coordinates.
COMPONENT_EXPORT(UI_WM)
void ConvertRectFromScreen(const aura::Window* window,
                           gfx::Rect* rect_in_screen);

COMPONENT_EXPORT(UI_WM)
void TranslateRectFromScreen(const aura::Window* window,
                             gfx::RectF* rect_in_screen);

}  // namespace wm

#endif  // UI_WM_CORE_COORDINATE_CONVERSION_H_
