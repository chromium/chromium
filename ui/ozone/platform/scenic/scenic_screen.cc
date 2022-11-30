// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_screen.h"

#include "base/notreached.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/point.h"

namespace ui {

ScenicScreen::ScenicScreen()
    : displays_({display::Display::GetDefaultDisplay()}) {}

ScenicScreen::~ScenicScreen() = default;

const std::vector<display::Display>& ScenicScreen::GetAllDisplays() const {
  return displays_;
}

display::Display ScenicScreen::GetPrimaryDisplay() const {
  return displays_[0];
}

display::Display ScenicScreen::GetDisplayForAcceleratedWidget(
    gfx::AcceleratedWidget widget) const {
  return displays_[0];
}

gfx::Point ScenicScreen::GetCursorScreenPoint() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::Point();
}

gfx::AcceleratedWidget ScenicScreen::GetAcceleratedWidgetAtScreenPoint(
    const gfx::Point& point) const {
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::kNullAcceleratedWidget;
}

display::Display ScenicScreen::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  return displays_[0];
}

display::Display ScenicScreen::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  return displays_[0];
}

void ScenicScreen::AddObserver(display::DisplayObserver* observer) {}

void ScenicScreen::RemoveObserver(display::DisplayObserver* observer) {}

}  // namespace ui
