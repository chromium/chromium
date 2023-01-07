// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_screen.h"

#include "base/notreached.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/point.h"

namespace ui {

// TODO(crbug.com/1242052): Integrate with platform APIs for screen enumeration
// and management, when available.

FlatlandScreen::FlatlandScreen()
    : displays_({display::Display::GetDefaultDisplay()}) {}

FlatlandScreen::~FlatlandScreen() = default;

const std::vector<display::Display>& FlatlandScreen::GetAllDisplays() const {
  return displays_;
}

display::Display FlatlandScreen::GetPrimaryDisplay() const {
  return displays_[0];
}

display::Display FlatlandScreen::GetDisplayForAcceleratedWidget(
    gfx::AcceleratedWidget widget) const {
  return displays_[0];
}

gfx::Point FlatlandScreen::GetCursorScreenPoint() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::Point();
}

gfx::AcceleratedWidget FlatlandScreen::GetAcceleratedWidgetAtScreenPoint(
    const gfx::Point& point) const {
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::kNullAcceleratedWidget;
}

display::Display FlatlandScreen::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  return displays_[0];
}

display::Display FlatlandScreen::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  return displays_[0];
}

void FlatlandScreen::AddObserver(display::DisplayObserver* observer) {}

void FlatlandScreen::RemoveObserver(display::DisplayObserver* observer) {}

}  // namespace ui
