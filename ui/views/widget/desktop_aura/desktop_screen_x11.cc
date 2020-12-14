// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_screen_x11.h"

#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/trace_event/trace_event.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/layout.h"
#include "ui/base/x/x11_display_util.h"
#include "ui/base/x/x11_util.h"
#include "ui/display/display.h"
#include "ui/display/display_finder.h"
#include "ui/display/util/display_util.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/switches.h"
#include "ui/platform_window/x11/x11_topmost_window_finder.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"

namespace views {

DesktopScreenX11::DesktopScreenX11() {
  if (LinuxUI::instance())
    display_scale_factor_observer_.Observe(LinuxUI::instance());
}

DesktopScreenX11::~DesktopScreenX11() {
  display::Screen::SetScreenInstance(old_screen_);
}

void DesktopScreenX11::Init() {
  if (x11_display_manager_->IsXrandrAvailable())
    event_observer_.Observe(x11::Connection::Get());
  x11_display_manager_->Init();
}

gfx::Point DesktopScreenX11::GetCursorScreenPoint() {
  TRACE_EVENT0("views", "DesktopScreenX11::GetCursorScreenPoint()");

  base::Optional<gfx::Point> point_in_pixels;
  if (const auto* const event_source = ui::X11EventSource::GetInstance())
    point_in_pixels = event_source->GetRootCursorLocationFromCurrentEvent();
  if (!point_in_pixels) {
    // This call is expensive so we explicitly only call it when
    // |point_in_pixels| is not set. We note that base::Optional::value_or()
    // would cause it to be called regardless.
    point_in_pixels = x11_display_manager_->GetCursorLocation();
  }
  // TODO(danakj): Should this be rounded? Or kept as a floating point?
  return gfx::ToFlooredPoint(
      gfx::ConvertPointToDips(*point_in_pixels, GetXDisplayScaleFactor()));
}

bool DesktopScreenX11::IsWindowUnderCursor(gfx::NativeWindow window) {
  return GetWindowAtScreenPoint(GetCursorScreenPoint()) == window;
}

gfx::NativeWindow DesktopScreenX11::GetWindowAtScreenPoint(
    const gfx::Point& point) {
  // TODO(danakj): Should this be rounded?
  gfx::Point point_in_pixels = gfx::ToFlooredPoint(
      gfx::ConvertPointToPixels(point, GetXDisplayScaleFactor()));
  auto window = ui::X11TopmostWindowFinder().FindLocalProcessWindowAt(
      point_in_pixels, {});
  return window != x11::Window::None
             ? views::DesktopWindowTreeHostPlatform::GetContentWindowForWidget(
                   static_cast<gfx::AcceleratedWidget>(window))
             : nullptr;
}

gfx::NativeWindow DesktopScreenX11::GetLocalProcessWindowAtPoint(
    const gfx::Point& point,
    const std::set<gfx::NativeWindow>& ignore) {
  std::set<gfx::AcceleratedWidget> ignore_widgets;
  for (auto* const window : ignore)
    ignore_widgets.emplace(window->GetHost()->GetAcceleratedWidget());
  // TODO(danakj): Should this be rounded?
  gfx::Point point_in_pixels = gfx::ToFlooredPoint(
      gfx::ConvertPointToPixels(point, GetXDisplayScaleFactor()));
  auto window = ui::X11TopmostWindowFinder().FindLocalProcessWindowAt(
      point_in_pixels, ignore_widgets);
  return window != x11::Window::None
             ? views::DesktopWindowTreeHostPlatform::GetContentWindowForWidget(
                   static_cast<gfx::AcceleratedWidget>(window))
             : nullptr;
}

int DesktopScreenX11::GetNumDisplays() const {
  return int{x11_display_manager_->displays().size()};
}

const std::vector<display::Display>& DesktopScreenX11::GetAllDisplays() const {
  return x11_display_manager_->displays();
}

display::Display DesktopScreenX11::GetDisplayNearestWindow(
    gfx::NativeView window) const {
  // Getting screen bounds here safely is hard.
  //
  // You'd think we'd be able to just call window->GetBoundsInScreen(), but we
  // can't because |window| (and the associated WindowEventDispatcher*) can be
  // partially initialized at this point; WindowEventDispatcher initializations
  // call through into GetDisplayNearestWindow(). But the X11 resources are
  // created before we create the aura::WindowEventDispatcher. So we ask what
  // the DRWHX11 believes the window bounds are instead of going through the
  // aura::Window's screen bounds.
  if (aura::WindowTreeHost* host = window ? window->GetHost() : nullptr) {
    const auto* const desktop_host =
        DesktopWindowTreeHostLinux::GetHostForWidget(
            host->GetAcceleratedWidget());
    if (desktop_host) {
      gfx::Rect match_rect_in_pixels = desktop_host->GetBoundsInPixels();
      gfx::Rect match_rect = gfx::ToEnclosingRect(gfx::ConvertRectToDips(
          match_rect_in_pixels, GetXDisplayScaleFactor()));
      return GetDisplayMatching(match_rect);
    }
  }

  return GetPrimaryDisplay();
}

display::Display DesktopScreenX11::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  return (GetNumDisplays() <= 1)
             ? GetPrimaryDisplay()
             : *FindDisplayNearestPoint(GetAllDisplays(), point);
}

display::Display DesktopScreenX11::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  const display::Display* const matching =
      display::FindDisplayWithBiggestIntersection(GetAllDisplays(), match_rect);
  return matching ? *matching : GetPrimaryDisplay();
}

display::Display DesktopScreenX11::GetPrimaryDisplay() const {
  return x11_display_manager_->GetPrimaryDisplay();
}

void DesktopScreenX11::AddObserver(display::DisplayObserver* observer) {
  x11_display_manager_->AddObserver(observer);
}

void DesktopScreenX11::RemoveObserver(display::DisplayObserver* observer) {
  x11_display_manager_->RemoveObserver(observer);
}

std::string DesktopScreenX11::GetCurrentWorkspace() {
  return x11_display_manager_->GetCurrentWorkspace();
}

void DesktopScreenX11::OnEvent(const x11::Event& event) {
  x11_display_manager_->OnEvent(event);
}

void DesktopScreenX11::OnDeviceScaleFactorChanged() {
  x11_display_manager_->DispatchDelayedDisplayListUpdate();
}

// static
void DesktopScreenX11::UpdateDeviceScaleFactorForTest() {
  auto* screen = static_cast<DesktopScreenX11*>(display::Screen::GetScreen());
  screen->x11_display_manager_->UpdateDisplayList();
}

void DesktopScreenX11::OnXDisplayListUpdated() {
  gfx::SetFontRenderParamsDeviceScaleFactor(
      GetPrimaryDisplay().device_scale_factor());
}

float DesktopScreenX11::GetXDisplayScaleFactor() const {
  if (LinuxUI::instance())
    return LinuxUI::instance()->GetDeviceScaleFactor();
  return display::Display::HasForceDeviceScaleFactor()
             ? display::Display::GetForcedDeviceScaleFactor()
             : 1.0f;
}

}  // namespace views
