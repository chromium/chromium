// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_screen_x11.h"

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
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/switches.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_x11.h"
#include "ui/views/widget/desktop_aura/x11_topmost_window_finder.h"

namespace {

float GetDeviceScaleFactor() {
  float device_scale_factor = 1.0f;
  if (views::LinuxUI::instance()) {
    device_scale_factor = views::LinuxUI::instance()->GetDeviceScaleFactor();
  } else if (display::Display::HasForceDeviceScaleFactor()) {
    device_scale_factor = display::Display::GetForcedDeviceScaleFactor();
  }
  return device_scale_factor;
}

}  // namespace

namespace views {

////////////////////////////////////////////////////////////////////////////////
// DesktopScreenX11, public:

DesktopScreenX11::DesktopScreenX11()
    : x11_display_manager_(std::make_unique<ui::XDisplayManager>(this)) {
  if (auto* linux_ui = views::LinuxUI::instance())
    linux_ui->AddDeviceScaleFactorObserver(this);
}

DesktopScreenX11::~DesktopScreenX11() {
  if (auto* linux_ui = views::LinuxUI::instance())
    linux_ui->RemoveDeviceScaleFactorObserver(this);
  if (x11_display_manager_->IsXrandrAvailable() &&
      ui::PlatformEventSource::GetInstance()) {
    ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
  }
}

void DesktopScreenX11::Init() {
  if (x11_display_manager_->IsXrandrAvailable() &&
      ui::PlatformEventSource::GetInstance()) {
    ui::PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  }
  x11_display_manager_->Init();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopScreenX11, display::Screen implementation:

gfx::Point DesktopScreenX11::GetCursorScreenPoint() {
  TRACE_EVENT0("views", "DesktopScreenX11::GetCursorScreenPoint()");

  if (auto* event_source = ui::X11EventSource::GetInstance()) {
    auto point = event_source->GetRootCursorLocationFromCurrentEvent();
    if (point)
      return gfx::ConvertPointToDIP(GetDeviceScaleFactor(), point.value());
  }
  return gfx::ConvertPointToDIP(GetDeviceScaleFactor(),
                                x11_display_manager_->GetCursorLocation());
}

bool DesktopScreenX11::IsWindowUnderCursor(gfx::NativeWindow window) {
  return GetWindowAtScreenPoint(GetCursorScreenPoint()) == window;
}

gfx::NativeWindow DesktopScreenX11::GetWindowAtScreenPoint(
    const gfx::Point& point) {
  X11TopmostWindowFinder finder;
  return finder.FindLocalProcessWindowAt(
      gfx::ConvertPointToPixel(GetDeviceScaleFactor(), point), {});
}

int DesktopScreenX11::GetNumDisplays() const {
  return x11_display_manager_->displays().size();
}

const std::vector<display::Display>& DesktopScreenX11::GetAllDisplays() const {
  return x11_display_manager_->displays();
}

display::Display DesktopScreenX11::GetDisplayNearestWindow(
    gfx::NativeView window) const {
  if (!window)
    return GetPrimaryDisplay();

  // Getting screen bounds here safely is hard.
  //
  // You'd think we'd be able to just call window->GetBoundsInScreen(), but we
  // can't because |window| (and the associated WindowEventDispatcher*) can be
  // partially initialized at this point; WindowEventDispatcher initializations
  // call through into GetDisplayNearestWindow(). But the X11 resources are
  // created before we create the aura::WindowEventDispatcher. So we ask what
  // the DRWHX11 believes the window bounds are instead of going through the
  // aura::Window's screen bounds.
  aura::WindowTreeHost* host = window->GetHost();
  if (host) {
    auto* rwh = DesktopWindowTreeHostLinux::GetHostForWidget(
        host->GetAcceleratedWidget());
    if (rwh) {
      const gfx::Rect pixel_rect = rwh->GetBoundsInPixels();
      const gfx::Rect dip_rect =
          gfx::ConvertRectToDIP(GetDeviceScaleFactor(), pixel_rect);
      return GetDisplayMatching(dip_rect);
    }
  }

  return GetPrimaryDisplay();
}

display::Display DesktopScreenX11::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  if (GetNumDisplays() <= 1)
    return GetPrimaryDisplay();
  return *FindDisplayNearestPoint(GetAllDisplays(), point);
}

display::Display DesktopScreenX11::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  const display::Display* matching =
      display::FindDisplayWithBiggestIntersection(GetAllDisplays(), match_rect);
  // Fallback to the primary display if there is no matching display.
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

bool DesktopScreenX11::CanDispatchEvent(const ui::PlatformEvent& event) {
  return x11_display_manager_->CanProcessEvent(*event);
}

uint32_t DesktopScreenX11::DispatchEvent(const ui::PlatformEvent& event) {
  ignore_result(x11_display_manager_->ProcessEvent(event));
  return ui::POST_DISPATCH_NONE;
}

void DesktopScreenX11::OnDeviceScaleFactorChanged() {
  x11_display_manager_->DispatchDelayedDisplayListUpdate();
}

// static
void DesktopScreenX11::UpdateDeviceScaleFactorForTest() {
  DesktopScreenX11* screen =
      static_cast<DesktopScreenX11*>(display::Screen::GetScreen());
  screen->x11_display_manager_->UpdateDisplayList();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopScreenX11, private:

void DesktopScreenX11::OnXDisplayListUpdated() {
  gfx::SetFontRenderParamsDeviceScaleFactor(
      GetPrimaryDisplay().device_scale_factor());
}

float DesktopScreenX11::GetXDisplayScaleFactor() {
  return GetDeviceScaleFactor();
}

////////////////////////////////////////////////////////////////////////////////

display::Screen* CreateDesktopScreen() {
  auto* screen = new DesktopScreenX11;
  screen->Init();
  return screen;
}

}  // namespace views
