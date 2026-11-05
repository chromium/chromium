// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/coordinate_conversion.h"

#include "ui/aura/client/screen_position_client.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"

namespace wm {

void ConvertPointToScreen(const aura::Window* window, gfx::Point* point) {
  CHECK(window, base::NotFatalUntil::M163);
  CHECK(window->GetRootWindow(), base::NotFatalUntil::M163);
  CHECK(aura::client::GetScreenPositionClient(window->GetRootWindow()),
        base::NotFatalUntil::M163);
  aura::client::GetScreenPositionClient(window->GetRootWindow())->
      ConvertPointToScreen(window, point);
}

void ConvertPointToScreen(const aura::Window* window, gfx::PointF* point) {
  CHECK(window, base::NotFatalUntil::M163);
  CHECK(window->GetRootWindow(), base::NotFatalUntil::M163);
  CHECK(aura::client::GetScreenPositionClient(window->GetRootWindow()),
        base::NotFatalUntil::M163);
  aura::client::GetScreenPositionClient(window->GetRootWindow())
      ->ConvertPointToScreen(window, point);
}

void ConvertPointFromScreen(const aura::Window* window,
                            gfx::Point* point_in_screen) {
  CHECK(window, base::NotFatalUntil::M163);
  CHECK(window->GetRootWindow(), base::NotFatalUntil::M163);
  CHECK(aura::client::GetScreenPositionClient(window->GetRootWindow()),
        base::NotFatalUntil::M163);
  aura::client::GetScreenPositionClient(window->GetRootWindow())->
      ConvertPointFromScreen(window, point_in_screen);
}

void ConvertPointFromScreen(const aura::Window* window,
                            gfx::PointF* point_in_screen) {
  CHECK(window, base::NotFatalUntil::M163);
  CHECK(window->GetRootWindow(), base::NotFatalUntil::M163);
  CHECK(aura::client::GetScreenPositionClient(window->GetRootWindow()),
        base::NotFatalUntil::M163);
  aura::client::GetScreenPositionClient(window->GetRootWindow())
      ->ConvertPointFromScreen(window, point_in_screen);
}

void ConvertRectToScreen(const aura::Window* window, gfx::Rect* rect) {
  gfx::Point origin = rect->origin();
  ConvertPointToScreen(window, &origin);
  rect->set_origin(origin);
}

void TranslateRectToScreen(const aura::Window* window, gfx::RectF* rect) {
  gfx::PointF origin = rect->origin();
  ConvertPointToScreen(window, &origin);
  rect->set_origin(origin);
}

void ConvertRectFromScreen(const aura::Window* window,
                           gfx::Rect* rect_in_screen) {
  gfx::Point origin = rect_in_screen->origin();
  ConvertPointFromScreen(window, &origin);
  rect_in_screen->set_origin(origin);
}

void TranslateRectFromScreen(const aura::Window* window, gfx::RectF* rect) {
  gfx::PointF origin = rect->origin();
  ConvertPointFromScreen(window, &origin);
  rect->set_origin(origin);
}

}  // namespace wm
