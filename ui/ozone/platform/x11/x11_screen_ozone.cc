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
#include "ui/ozone/platform/x11/x11_window_ozone.h"
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

gfx::Point PixelToDIPPoint(const gfx::Point& pixel_point) {
  return gfx::ConvertPointToDIP(GetDeviceScaleFactor(), pixel_point);
}

// ui::EnumerateTopLevelWindows API is used here to retrieve the x11 window
// stack, so that windows are checked in descending z-order, covering window
// overlapping cases gracefully.
// TODO(nickdiego): Consider refactoring ui::EnumerateTopLevelWindows to use
// lambda/callback instead of Delegate interface.
class LocalProcessWindowFinder : public EnumerateWindowsDelegate {
 public:
  LocalProcessWindowFinder() = default;
  ~LocalProcessWindowFinder() override = default;

  X11Window* FindWindowAt(const gfx::Point& screen_point_in_pixels);

 private:
  // ui::EnumerateWindowsDelegate
  bool ShouldStopIterating(XID xid) override;

  // Returns true if |window| is visible and contains the
  // |screen_point_in_pixels_| within its bounds, even if custom shape is used.
  bool MatchWindow(X11Window* window) const;

  X11Window* window_found_ = nullptr;
  gfx::Point screen_point_in_pixels_;
};

X11Window* LocalProcessWindowFinder::FindWindowAt(
    const gfx::Point& screen_point_in_pixels) {
  screen_point_in_pixels_ = screen_point_in_pixels;
  ui::EnumerateTopLevelWindows(this);
  return window_found_;
}

bool LocalProcessWindowFinder::ShouldStopIterating(XID xid) {
  X11Window* window = X11WindowManager::GetInstance()->GetWindow(xid);
  if (!window || !MatchWindow(window))
    return false;

  window_found_ = window;
  return true;
}

bool LocalProcessWindowFinder::MatchWindow(X11Window* window) const {
  DCHECK(window);

  if (!window->IsVisible())
    return false;

  gfx::Rect window_bounds = window->GetOutterBounds();
  if (!window_bounds.Contains(screen_point_in_pixels_))
    return false;

  ::Region shape = window->shape();
  if (!shape)
    return true;

  gfx::Point window_point(screen_point_in_pixels_);
  window_point.Offset(-window_bounds.origin().x(), -window_bounds.origin().y());
  return XPointInRegion(shape, window_point.x(), window_point.y()) == x11::True;
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
  if (ui::X11EventSource::HasInstance()) {
    base::Optional<gfx::Point> point =
        ui::X11EventSource::GetInstance()
            ->GetRootCursorLocationFromCurrentEvent();
    if (point)
      return PixelToDIPPoint(point.value());
  }
  return PixelToDIPPoint(GetCursorLocation());
}

gfx::AcceleratedWidget X11ScreenOzone::GetAcceleratedWidgetAtScreenPoint(
    const gfx::Point& point) const {
  LocalProcessWindowFinder finder;
  X11Window* window = finder.FindWindowAt(point);
  return window ? window->GetWidget() : gfx::kNullAcceleratedWidget;
}

display::Display X11ScreenOzone::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  auto displays = GetAllDisplays();
  if (displays.size() <= 1)
    return GetPrimaryDisplay();
  return *display::FindDisplayNearestPoint(displays, point);
}

display::Display X11ScreenOzone::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  const display::Display* matching_display =
      display::FindDisplayWithBiggestIntersection(
          x11_display_manager_->displays(),
          gfx::ConvertRectToDIP(GetDeviceScaleFactor(), match_rect));
  return matching_display ? *matching_display : GetPrimaryDisplay();
}

void X11ScreenOzone::AddObserver(display::DisplayObserver* observer) {
  x11_display_manager_->AddObserver(observer);
}

void X11ScreenOzone::RemoveObserver(display::DisplayObserver* observer) {
  x11_display_manager_->RemoveObserver(observer);
}

bool X11ScreenOzone::DispatchXEvent(XEvent* xev) {
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

float X11ScreenOzone::GetXDisplayScaleFactor() {
  return GetDeviceScaleFactor();
}

}  // namespace ui
