// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_screen_x11.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/layout.h"
#include "ui/base/x/x11_display_util.h"
#include "ui/base/x/x11_util.h"
#include "ui/display/display.h"
#include "ui/display/display_finder.h"
#include "ui/display/screen.h"
#include "ui/display/util/display_util.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/switches.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_x11.h"
#include "ui/views/widget/desktop_aura/x11_topmost_window_finder.h"

namespace {

constexpr int kMinVersionXrandr = 103;  // Need at least xrandr version 1.3.

float GetDeviceScaleFactor() {
  float device_scale_factor = 1.0f;
  if (views::LinuxUI::instance()) {
    device_scale_factor =
      views::LinuxUI::instance()->GetDeviceScaleFactor();
  } else if (display::Display::HasForceDeviceScaleFactor()) {
    device_scale_factor = display::Display::GetForcedDeviceScaleFactor();
  }
  return device_scale_factor;
}

gfx::Point PixelToDIPPoint(const gfx::Point& pixel_point) {
  return gfx::ScaleToFlooredPoint(pixel_point, 1.0f / GetDeviceScaleFactor());
}

gfx::Point DIPToPixelPoint(const gfx::Point& dip_point) {
  return gfx::ScaleToFlooredPoint(dip_point, GetDeviceScaleFactor());
}

}  // namespace

namespace views {

////////////////////////////////////////////////////////////////////////////////
// DesktopScreenX11, public:

DesktopScreenX11::DesktopScreenX11()
    : xdisplay_(gfx::GetXDisplay()),
      x_root_window_(DefaultRootWindow(xdisplay_)),
      xrandr_version_(ui::GetXrandrVersion(xdisplay_)),
      weak_factory_(this) {
  if (views::LinuxUI::instance())
    views::LinuxUI::instance()->AddDeviceScaleFactorObserver(this);
  float scale = GetDeviceScaleFactor();
  // Need at least xrandr version 1.3.
  if (xrandr_version_ >= kMinVersionXrandr) {
    int error_base_ignored = 0;
    XRRQueryExtension(xdisplay_, &xrandr_event_base_, &error_base_ignored);

    if (ui::PlatformEventSource::GetInstance())
      ui::PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
    XRRSelectInput(xdisplay_,
                   x_root_window_,
                   RRScreenChangeNotifyMask |
                   RROutputChangeNotifyMask |
                   RRCrtcChangeNotifyMask);

    SetDisplaysInternal(ui::BuildDisplaysFromXRandRInfo(
        xrandr_version_, scale, &primary_display_index_));
  } else {
    SetDisplaysInternal(ui::GetFallbackDisplayList(scale));
  }
}

DesktopScreenX11::~DesktopScreenX11() {
  if (views::LinuxUI::instance())
    views::LinuxUI::instance()->AddDeviceScaleFactorObserver(this);
  if (xrandr_version_ >= kMinVersionXrandr &&
      ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
}

////////////////////////////////////////////////////////////////////////////////
// DesktopScreenX11, display::Screen implementation:

gfx::Point DesktopScreenX11::GetCursorScreenPoint() {
  TRACE_EVENT0("views", "DesktopScreenX11::GetCursorScreenPoint()");

  if (ui::X11EventSource::HasInstance()) {
    auto point = ui::X11EventSource::GetInstance()
                     ->GetRootCursorLocationFromCurrentEvent();
    if (point)
      return PixelToDIPPoint(point.value());
  }

  ::Window root, child;
  int root_x, root_y, win_x, win_y;
  unsigned int mask;
  XQueryPointer(xdisplay_, x_root_window_, &root, &child, &root_x, &root_y,
                &win_x, &win_y, &mask);

  return PixelToDIPPoint(gfx::Point(root_x, root_y));
}

bool DesktopScreenX11::IsWindowUnderCursor(gfx::NativeWindow window) {
  return GetWindowAtScreenPoint(GetCursorScreenPoint()) == window;
}

gfx::NativeWindow DesktopScreenX11::GetWindowAtScreenPoint(
    const gfx::Point& point) {
  X11TopmostWindowFinder finder;
  return finder.FindLocalProcessWindowAt(
      DIPToPixelPoint(point), std::set<aura::Window*>());
}

int DesktopScreenX11::GetNumDisplays() const {
  return displays_.size();
}

const std::vector<display::Display>& DesktopScreenX11::GetAllDisplays() const {
  return displays_;
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
    DesktopWindowTreeHostX11* rwh = DesktopWindowTreeHostX11::GetHostForXID(
        host->GetAcceleratedWidget());
    if (rwh) {
      const float scale = 1.0f / GetDeviceScaleFactor();
      const gfx::Rect pixel_rect = rwh->GetX11RootWindowBounds();
      return GetDisplayMatching(
          gfx::Rect(gfx::ScaleToFlooredPoint(pixel_rect.origin(), scale),
                    gfx::ScaleToCeiledSize(pixel_rect.size(), scale)));
    }
  }

  return GetPrimaryDisplay();
}

display::Display DesktopScreenX11::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  if (displays_.size() <= 1)
    return GetPrimaryDisplay();
  for (auto it = displays_.begin(); it != displays_.end(); ++it) {
    if (it->bounds().Contains(point))
      return *it;
  }
  return *FindDisplayNearestPoint(displays_, point);
}

display::Display DesktopScreenX11::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  int max_area = 0;
  const display::Display* matching = NULL;
  for (auto it = displays_.begin(); it != displays_.end(); ++it) {
    gfx::Rect intersect = gfx::IntersectRects(it->bounds(), match_rect);
    int area = intersect.width() * intersect.height();
    if (area > max_area) {
      max_area = area;
      matching = &*it;
    }
  }
  // Fallback to the primary display if there is no matching display.
  return matching ? *matching : GetPrimaryDisplay();
}

display::Display DesktopScreenX11::GetPrimaryDisplay() const {
  DCHECK(!displays_.empty());
  return displays_[primary_display_index_];
}

void DesktopScreenX11::AddObserver(display::DisplayObserver* observer) {
  change_notifier_.AddObserver(observer);
}

void DesktopScreenX11::RemoveObserver(display::DisplayObserver* observer) {
  change_notifier_.RemoveObserver(observer);
}

bool DesktopScreenX11::CanDispatchEvent(const ui::PlatformEvent& event) {
  return event->type - xrandr_event_base_ == RRScreenChangeNotify ||
         event->type - xrandr_event_base_ == RRNotify ||
         (event->type == PropertyNotify &&
          event->xproperty.window == x_root_window_ &&
          event->xproperty.atom == gfx::GetAtom("_NET_WORKAREA"));
}

uint32_t DesktopScreenX11::DispatchEvent(const ui::PlatformEvent& event) {
  if (event->type - xrandr_event_base_ == RRScreenChangeNotify) {
    // Pass the event through to xlib.
    XRRUpdateConfiguration(event);
  } else if (event->type - xrandr_event_base_ == RRNotify ||
             (event->type == PropertyNotify &&
              event->xproperty.atom == gfx::GetAtom("_NET_WORKAREA"))) {
    RestartDelayedConfigurationTask();
  } else {
    NOTREACHED();
  }

  return ui::POST_DISPATCH_NONE;
}

void DesktopScreenX11::OnDeviceScaleFactorChanged() {
  RestartDelayedConfigurationTask();
}

// static
void DesktopScreenX11::UpdateDeviceScaleFactorForTest() {
  DesktopScreenX11* screen =
      static_cast<DesktopScreenX11*>(display::Screen::GetScreen());
  screen->UpdateDisplays();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopScreenX11, private:

DesktopScreenX11::DesktopScreenX11(
    const std::vector<display::Display>& test_displays)
    : xdisplay_(gfx::GetXDisplay()),
      x_root_window_(DefaultRootWindow(xdisplay_)),
      xrandr_version_(ui::GetXrandrVersion(xdisplay_)),
      displays_(test_displays),
      weak_factory_(this) {
  if (views::LinuxUI::instance())
    views::LinuxUI::instance()->AddDeviceScaleFactorObserver(this);
}

void DesktopScreenX11::RestartDelayedConfigurationTask() {
  delayed_configuration_task_.Reset(base::Bind(
      &DesktopScreenX11::UpdateDisplays, weak_factory_.GetWeakPtr()));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, delayed_configuration_task_.callback());
}

void DesktopScreenX11::UpdateDisplays() {
  std::vector<display::Display> old_displays = displays_;
  float scale = GetDeviceScaleFactor();
  if (xrandr_version_ > kMinVersionXrandr) {
    SetDisplaysInternal(ui::BuildDisplaysFromXRandRInfo(
        xrandr_version_, scale, &primary_display_index_));
  } else {
    SetDisplaysInternal(ui::GetFallbackDisplayList(scale));
  }
  change_notifier_.NotifyDisplaysChanged(old_displays, displays_);
}

void DesktopScreenX11::SetDisplaysInternal(
    const std::vector<display::Display>& displays) {
  displays_ = displays;
  gfx::SetFontRenderParamsDeviceScaleFactor(
      GetPrimaryDisplay().device_scale_factor());
}

////////////////////////////////////////////////////////////////////////////////

display::Screen* CreateDesktopScreen() {
  return new DesktopScreenX11;
}

}  // namespace views
