// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_screen.h"

#include "ui/display/display.h"
#include "ui/display/display_finder.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

WaylandScreen::WaylandScreen() : weak_factory_(this) {}

WaylandScreen::~WaylandScreen() = default;

void WaylandScreen::OnOutputAdded(uint32_t output_id, bool is_primary) {
  display::Display new_display(output_id);
  display_list_.AddDisplay(std::move(new_display),
                           is_primary
                               ? display::DisplayList::Type::PRIMARY
                               : display::DisplayList::Type::NOT_PRIMARY);
}

void WaylandScreen::OnOutputRemoved(uint32_t output_id) {
  display_list_.RemoveDisplay(output_id);
}

void WaylandScreen::OnOutputMetricsChanged(uint32_t output_id,
                                           const gfx::Rect& new_bounds,
                                           float device_pixel_ratio,
                                           bool is_primary) {
  display::Display changed_display(output_id);
  changed_display.set_device_scale_factor(device_pixel_ratio);
  changed_display.set_bounds(new_bounds);
  changed_display.set_work_area(new_bounds);

  display_list_.UpdateDisplay(
      changed_display, is_primary ? display::DisplayList::Type::PRIMARY
                                  : display::DisplayList::Type::NOT_PRIMARY);
}

base::WeakPtr<WaylandScreen> WaylandScreen::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

const std::vector<display::Display>& WaylandScreen::GetAllDisplays() const {
  return display_list_.displays();
}

display::Display WaylandScreen::GetPrimaryDisplay() const {
  auto iter = display_list_.GetPrimaryDisplayIterator();
  if (iter == display_list_.displays().end())
    return display::Display::GetDefaultDisplay();
  return *iter;
}

display::Display WaylandScreen::GetDisplayForAcceleratedWidget(
    gfx::AcceleratedWidget widget) const {
  // TODO(msisov): implement wl_surface_listener::enter and
  // wl_surface_listener::leave for a wl_surface to know what surface the window
  // is located on.
  //
  // https://crbug.com/890271
  NOTIMPLEMENTED_LOG_ONCE();
  return GetPrimaryDisplay();
}

gfx::Point WaylandScreen::GetCursorScreenPoint() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::Point();
}

gfx::AcceleratedWidget WaylandScreen::GetAcceleratedWidgetAtScreenPoint(
    const gfx::Point& point) const {
  // TODO(msisov): implement this once wl_surface_listener::enter and ::leave
  // are used.
  //
  // https://crbug.com/890271
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::kNullAcceleratedWidget;
}

display::Display WaylandScreen::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  NOTIMPLEMENTED_LOG_ONCE();
  return GetPrimaryDisplay();
}

display::Display WaylandScreen::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  // TODO(msisov): https://crbug.com/890272
  NOTIMPLEMENTED_LOG_ONCE();
  return GetPrimaryDisplay();
}

void WaylandScreen::AddObserver(display::DisplayObserver* observer) {
  display_list_.AddObserver(observer);
}

void WaylandScreen::RemoveObserver(display::DisplayObserver* observer) {
  display_list_.RemoveObserver(observer);
}

}  // namespace ui
