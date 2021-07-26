// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_screen.h"

#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/point.h"

namespace ui {

FlatlandScreen::FlatlandScreen() : weak_factory_(this) {}

FlatlandScreen::~FlatlandScreen() = default;

void FlatlandScreen::OnWindowAdded(int32_t window_id) {
  // Ensure that |window_id| is greater than the id of all other windows. This
  // allows pushing the new entry to the end of the list while keeping it
  // sorted.
  DCHECK(displays_.empty() || (window_id > displays_.back().id()));
  displays_.push_back(display::Display(window_id));

  for (auto& observer : observers_)
    observer.OnDisplayAdded(displays_.back());
}

void FlatlandScreen::OnWindowRemoved(int32_t window_id) {
  auto display_it = std::find_if(displays_.begin(), displays_.end(),
                                 [window_id](display::Display& display) {
                                   return display.id() == window_id;
                                 });
  DCHECK(display_it != displays_.end());
  display::Display removed_display = *display_it;
  displays_.erase(display_it);

  for (auto& observer : observers_) {
    observer.OnDisplayRemoved(removed_display);
    observer.OnDidRemoveDisplays();
  }
}

void FlatlandScreen::OnWindowBoundsChanged(int32_t window_id,
                                           gfx::Rect bounds) {
  auto display_it = std::find_if(displays_.begin(), displays_.end(),
                                 [window_id](display::Display& display) {
                                   return display.id() == window_id;
                                 });
  DCHECK(display_it != displays_.end());
  display_it->set_bounds(bounds);
}

base::WeakPtr<FlatlandScreen> FlatlandScreen::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

const std::vector<display::Display>& FlatlandScreen::GetAllDisplays() const {
  return displays_;
}

display::Display FlatlandScreen::GetPrimaryDisplay() const {
  // There is no primary display.
  return display::Display();
}

display::Display FlatlandScreen::GetDisplayForAcceleratedWidget(
    gfx::AcceleratedWidget widget) const {
  auto display_it = std::find_if(displays_.begin(), displays_.end(),
                                 [widget](const display::Display& display) {
                                   return display.id() == widget;
                                 });
  if (display_it == displays_.end()) {
    NOTREACHED();
    return display::Display();
  }

  return *display_it;
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
  NOTREACHED();
  return display::Display();
}

display::Display FlatlandScreen::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  NOTREACHED();
  return display::Display();
}

void FlatlandScreen::AddObserver(display::DisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void FlatlandScreen::RemoveObserver(display::DisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ui
