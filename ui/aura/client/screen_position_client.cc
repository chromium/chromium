// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/screen_position_client.h"

#include "ui/base/class_property.h"
#include "ui/gfx/geometry/point_conversions.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(aura::client::ScreenPositionClient*)

namespace aura {
namespace client {

DEFINE_UI_CLASS_PROPERTY_KEY(ScreenPositionClient*,
                             kScreenPositionClientKey,
                             nullptr)

void ScreenPositionClient::ConvertPointToScreen(const Window* window,
                                                gfx::Point* point) {
  gfx::PointF point_float(*point);
  ConvertPointToScreen(window, &point_float);
  *point = gfx::ToFlooredPoint(point_float);
}

void ScreenPositionClient::ConvertPointFromScreen(const Window* window,
                                                  gfx::Point* point) {
  gfx::PointF point_float(*point);
  ConvertPointFromScreen(window, &point_float);
  *point = gfx::ToFlooredPoint(point_float);
}

void ScreenPositionClient::ConvertPointToRootWindowIgnoringTransforms(
    const Window* window,
    gfx::Point* point) {
  DCHECK(window);
  DCHECK(window->GetRootWindow());
  Window* ancestor = const_cast<Window*>(window);
  while (ancestor && !ancestor->IsRootWindow()) {
    const gfx::Point origin = ancestor->bounds().origin();
    point->Offset(origin.x(), origin.y());
    ancestor = ancestor->parent();
  }
}

void ScreenPositionClient::ConvertPointToScreenIgnoringTransforms(
    const aura::Window* window,
    gfx::Point* point) {
  const aura::Window* root_window = window->GetRootWindow();
  ConvertPointToRootWindowIgnoringTransforms(window, point);
  gfx::Point origin = GetRootWindowOriginInScreen(root_window);
  point->Offset(origin.x(), origin.y());
}

void SetScreenPositionClient(Window* root_window,
                             ScreenPositionClient* client) {
  DCHECK_EQ(root_window->GetRootWindow(), root_window);
  root_window->SetProperty(kScreenPositionClientKey, client);
}

ScreenPositionClient* GetScreenPositionClient(const Window* root_window) {
#if DCHECK_IS_ON()
  if (root_window) {
    DCHECK_EQ(root_window->GetRootWindow(), root_window);
  }
#endif
  return root_window ? root_window->GetProperty(kScreenPositionClientKey)
                     : nullptr;
}

}  // namespace client
}  // namespace aura
