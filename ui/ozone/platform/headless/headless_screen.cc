// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/headless/headless_screen.h"

namespace ui {

HeadlessScreen::HeadlessScreen() {
  static constexpr int64_t kHeadlessDisplayId = 1;
  static constexpr float kHeadlessDisplayScale = 1.0f;
  static constexpr gfx::Rect kHeadlessDisplayBounds(gfx::Size(1, 1));
  display::Display display(kHeadlessDisplayId);
  display.SetScaleAndBounds(kHeadlessDisplayScale, kHeadlessDisplayBounds);
  display_list_.AddDisplay(display, display::DisplayList::Type::PRIMARY);
}

HeadlessScreen::~HeadlessScreen() = default;

const std::vector<display::Display>& HeadlessScreen::GetAllDisplays() const {
  return display_list_.displays();
}

display::Display HeadlessScreen::GetPrimaryDisplay() const {
  auto iter = display_list_.GetPrimaryDisplayIterator();
  DCHECK(iter != display_list_.displays().end());
  return *iter;
}

display::Display HeadlessScreen::GetDisplayForAcceleratedWidget(
    gfx::AcceleratedWidget widget) const {
  return GetPrimaryDisplay();
}

gfx::Point HeadlessScreen::GetCursorScreenPoint() const {
  return gfx::Point();
}

gfx::AcceleratedWidget HeadlessScreen::GetAcceleratedWidgetAtScreenPoint(
    const gfx::Point& point) const {
  return gfx::kNullAcceleratedWidget;
}

display::Display HeadlessScreen::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  return GetPrimaryDisplay();
}

display::Display HeadlessScreen::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  return GetPrimaryDisplay();
}

void HeadlessScreen::AddObserver(display::DisplayObserver* observer) {
  display_list_.AddObserver(observer);
}

void HeadlessScreen::RemoveObserver(display::DisplayObserver* observer) {
  display_list_.RemoveObserver(observer);
}

}  // namespace ui
