// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_screen.h"

#include "ui/display/display.h"
#include "ui/display/display_finder.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor_position.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

WaylandScreen::WaylandScreen(WaylandConnection* connection)
    : connection_(connection), weak_factory_(this) {
  DCHECK(connection_);
}

WaylandScreen::~WaylandScreen() = default;

void WaylandScreen::OnOutputAdded(uint32_t output_id) {
  display_list_.AddDisplay(display::Display(output_id),
                           display::DisplayList::Type::NOT_PRIMARY);
}

void WaylandScreen::OnOutputRemoved(uint32_t output_id) {
  if (output_id == GetPrimaryDisplay().id()) {
    // First, set a new primary display as required by the |display_list_|. It's
    // safe to set any of the displays to be a primary one. Once the output is
    // completely removed, Wayland updates geometry of other displays. And a
    // display, which became the one to be nearest to the origin will become a
    // primary one.
    for (const auto& display : display_list_.displays()) {
      if (display.id() != output_id) {
        display_list_.AddOrUpdateDisplay(display,
                                         display::DisplayList::Type::PRIMARY);
        break;
      }
    }
  }
  display_list_.RemoveDisplay(output_id);
}

void WaylandScreen::OnOutputMetricsChanged(uint32_t output_id,
                                           const gfx::Rect& new_bounds,
                                           int32_t device_pixel_ratio) {
  display::Display changed_display(output_id);
  if (!display::Display::HasForceDeviceScaleFactor())
    changed_display.set_device_scale_factor(device_pixel_ratio);
  changed_display.set_bounds(new_bounds);
  changed_display.set_work_area(new_bounds);

  bool is_primary = false;
  display::Display display_nearest_origin =
      GetDisplayNearestPoint(gfx::Point(0, 0));
  // If bounds of the nearest to origin display are empty, it must have been the
  // very first and the same display added before.
  if (display_nearest_origin.bounds().IsEmpty()) {
    DCHECK_EQ(display_nearest_origin.id(), changed_display.id());
    is_primary = true;
  } else if (changed_display.bounds().origin() <
             display_nearest_origin.bounds().origin()) {
    // If changed display is nearer to the origin than the previous display,
    // that one must become a primary display.
    is_primary = true;
  } else if (changed_display.bounds().OffsetFromOrigin() ==
             display_nearest_origin.bounds().OffsetFromOrigin()) {
    // If changed display has the same origin as the nearest to origin display,
    // |changed_display| must become a primary one or it has already been the
    // primary one. If a user changed positions of two displays (the second at
    // x,x was set to 0,0), the second change will modify geometry of the
    // display, which used to be the one nearest to the origin.
    is_primary = true;
  }

  display_list_.UpdateDisplay(
      changed_display, is_primary ? display::DisplayList::Type::PRIMARY
                                  : display::DisplayList::Type::NOT_PRIMARY);

  auto* wayland_window_manager = connection_->wayland_window_manager();
  for (auto* window : wayland_window_manager->GetWindowsOnOutput(output_id))
    window->UpdateBufferScale(true);
}

base::WeakPtr<WaylandScreen> WaylandScreen::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

const std::vector<display::Display>& WaylandScreen::GetAllDisplays() const {
  return display_list_.displays();
}

display::Display WaylandScreen::GetPrimaryDisplay() const {
  auto iter = display_list_.GetPrimaryDisplayIterator();
  DCHECK(iter != display_list_.displays().end());
  return *iter;
}

display::Display WaylandScreen::GetDisplayForAcceleratedWidget(
    gfx::AcceleratedWidget widget) const {
  auto* window = connection_->wayland_window_manager()->GetWindow(widget);
  // A window might be destroyed by this time on shutting down the browser.
  if (!window)
    return GetPrimaryDisplay();

  const auto* parent_window = window->parent_window();
  const auto entered_outputs_ids = window->entered_outputs_ids();
  // Although spec says a surface receives enter/leave surface events on
  // create/move/resize actions, this might be called right after a window is
  // created, but it has not been configured by a Wayland compositor and it has
  // not received enter surface events yet. Another case is when a user switches
  // between displays in a single output mode - Wayland may not send enter
  // events immediately, which can result in empty container of entered ids
  // (check comments in WaylandWindow::RemoveEnteredOutputId). In this case,
  // it's also safe to return the primary display.
  // A child window will most probably enter the same display than its parent
  // so we return the parent's display if there is a parent.
  if (entered_outputs_ids.empty()) {
    if (parent_window)
      return GetDisplayForAcceleratedWidget(parent_window->GetWidget());
    return GetPrimaryDisplay();
  }

  DCHECK(!display_list_.displays().empty());

  // A widget can be located on two or more displays. It would be better if the
  // most in DIP occupied display was returned, but it's impossible to do so in
  // Wayland. Thus, return the one that was used the earliest.
  for (const auto& display : display_list_.displays()) {
    if (display.id() == *entered_outputs_ids.begin())
      return display;
  }

  NOTREACHED();
  return GetPrimaryDisplay();
}

gfx::Point WaylandScreen::GetCursorScreenPoint() const {
  auto* wayland_window_manager = connection_->wayland_window_manager();
  // Wayland does not provide either location of surfaces in global space
  // coordinate system or location of a pointer. Instead, only locations of
  // mouse/touch events are known. Given that Chromium assumes top-level windows
  // are located at origin, always provide a cursor point in regards to
  // surfaces' location.
  //
  // If a pointer is located in any of the existing wayland windows, return the
  // last known cursor position. Otherwise, return such a point, which is not
  // contained by any of the windows.
  auto* cursor_position = connection_->wayland_cursor_position();
  if (wayland_window_manager->GetCurrentFocusedWindow() && cursor_position)
    return cursor_position->GetCursorSurfacePoint();

  auto* window = wayland_window_manager->GetWindowWithLargestBounds();
  DCHECK(window);
  const gfx::Rect bounds = window->GetBounds();
  return gfx::Point(bounds.width() + 10, bounds.height() + 10);
}

gfx::AcceleratedWidget WaylandScreen::GetAcceleratedWidgetAtScreenPoint(
    const gfx::Point& point) const {
  // It is safe to check only for focused windows and test if they contain the
  // point or not.
  auto* window =
      connection_->wayland_window_manager()->GetCurrentFocusedWindow();
  if (window && window->GetBounds().Contains(point))
    return window->GetWidget();
  return gfx::kNullAcceleratedWidget;
}

display::Display WaylandScreen::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  return *FindDisplayNearestPoint(display_list_.displays(), point);
}

display::Display WaylandScreen::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  if (match_rect.IsEmpty())
    return GetDisplayNearestPoint(match_rect.origin());

  const display::Display* display_matching =
      display::FindDisplayWithBiggestIntersection(display_list_.displays(),
                                                  match_rect);
  return display_matching ? *display_matching : GetPrimaryDisplay();
}

void WaylandScreen::AddObserver(display::DisplayObserver* observer) {
  display_list_.AddObserver(observer);
}

void WaylandScreen::RemoveObserver(display::DisplayObserver* observer) {
  display_list_.RemoveObserver(observer);
}

}  // namespace ui
