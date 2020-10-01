// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_screen_ozone.h"

#include "ui/base/x/x11_util.h"
#include "ui/display/display_finder.h"
#include "ui/display/util/display_util.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/platform_window/x11/x11_topmost_window_finder.h"
#include "ui/platform_window/x11/x11_window.h"
#include "ui/platform_window/x11/x11_window_manager.h"

namespace ui {

namespace {

float GetDeviceScaleFactor() {
  float device_scale_factor = 1.0f;
  // TODO(crbug.com/891175): Implement PlatformScreen for X11
  // Get device scale factor using scale factor and resolution like
  // 'GtkUi::GetRawDeviceScaleFactor'.
  if (display::Display::HasForceDeviceScaleFactor())
    device_scale_factor = display::Display::GetForcedDeviceScaleFactor();
  return device_scale_factor;
}

}  // namespace

X11ScreenOzone::X11ScreenOzone()
    : window_manager_(X11WindowManager::GetInstance()),
      x11_display_manager_(std::make_unique<XDisplayManager>(this)) {
  DCHECK(window_manager_);
}

X11ScreenOzone::~X11ScreenOzone() {
  if (x11_display_manager_->IsXrandrAvailable() &&
      X11EventSource::HasInstance()) {
    X11EventSource::GetInstance()->RemoveXEventDispatcher(this);
  }
}

void X11ScreenOzone::Init() {
  if (x11_display_manager_->IsXrandrAvailable() &&
      X11EventSource::HasInstance()) {
    X11EventSource::GetInstance()->AddXEventDispatcher(this);
  }
  x11_display_manager_->Init();
}

const std::vector<display::Display>& X11ScreenOzone::GetAllDisplays() const {
  return x11_display_manager_->displays();
}

display::Display X11ScreenOzone::GetPrimaryDisplay() const {
  return x11_display_manager_->GetPrimaryDisplay();
}

display::Display X11ScreenOzone::GetDisplayForAcceleratedWidget(
    gfx::AcceleratedWidget widget) const {
  if (widget == gfx::kNullAcceleratedWidget)
    return GetPrimaryDisplay();

  X11Window* window = window_manager_->GetWindow(widget);
  return window ? GetDisplayMatching(window->GetBounds()) : GetPrimaryDisplay();
}

gfx::Point X11ScreenOzone::GetCursorScreenPoint() const {
  base::Optional<gfx::Point> point_in_pixels;
  if (ui::X11EventSource::HasInstance()) {
    point_in_pixels = ui::X11EventSource::GetInstance()
                          ->GetRootCursorLocationFromCurrentEvent();
  }
  if (!point_in_pixels) {
    // This call is expensive so we explicitly only call it when
    // |point_in_pixels| is not set. We note that base::Optional::value_or()
    // would cause it to be called regardless.
    point_in_pixels = GetCursorLocation();
  }
  // TODO(danakj): Should this be rounded? Or kept as a floating point?
  return gfx::ToFlooredPoint(
      gfx::ConvertPointToDips(*point_in_pixels, GetDeviceScaleFactor()));
}

gfx::AcceleratedWidget X11ScreenOzone::GetAcceleratedWidgetAtScreenPoint(
    const gfx::Point& point) const {
  X11TopmostWindowFinder finder;
  return static_cast<gfx::AcceleratedWidget>(finder.FindWindowAt(point));
}

gfx::AcceleratedWidget X11ScreenOzone::GetLocalProcessWidgetAtPoint(
    const gfx::Point& point,
    const std::set<gfx::AcceleratedWidget>& ignore) const {
  X11TopmostWindowFinder finder;
  return static_cast<gfx::AcceleratedWidget>(
      finder.FindLocalProcessWindowAt(point, ignore));
}

display::Display X11ScreenOzone::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  auto displays = GetAllDisplays();
  if (displays.size() <= 1)
    return GetPrimaryDisplay();
  return *display::FindDisplayNearestPoint(displays, point);
}

display::Display X11ScreenOzone::GetDisplayMatching(
    const gfx::Rect& match_rect_in_pixels) const {
  gfx::Rect match_rect = gfx::ToEnclosingRect(
      gfx::ConvertRectToDips(match_rect_in_pixels, GetDeviceScaleFactor()));
  const display::Display* matching_display =
      display::FindDisplayWithBiggestIntersection(
          x11_display_manager_->displays(), match_rect);
  return matching_display ? *matching_display : GetPrimaryDisplay();
}

void X11ScreenOzone::SetScreenSaverSuspended(bool suspend) {
  SuspendX11ScreenSaver(suspend);
}

void X11ScreenOzone::AddObserver(display::DisplayObserver* observer) {
  x11_display_manager_->AddObserver(observer);
}

void X11ScreenOzone::RemoveObserver(display::DisplayObserver* observer) {
  x11_display_manager_->RemoveObserver(observer);
}

std::string X11ScreenOzone::GetCurrentWorkspace() {
  return x11_display_manager_->GetCurrentWorkspace();
}

bool X11ScreenOzone::DispatchXEvent(x11::Event* xev) {
  return x11_display_manager_->ProcessEvent(xev);
}

gfx::Point X11ScreenOzone::GetCursorLocation() const {
  return x11_display_manager_->GetCursorLocation();
}

void X11ScreenOzone::OnXDisplayListUpdated() {
  float scale_factor =
      x11_display_manager_->GetPrimaryDisplay().device_scale_factor();
  gfx::SetFontRenderParamsDeviceScaleFactor(scale_factor);
}

float X11ScreenOzone::GetXDisplayScaleFactor() const {
  return GetDeviceScaleFactor();
}

}  // namespace ui
