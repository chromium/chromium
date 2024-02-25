// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_screen_ozone.h"

#include <memory>

#include "base/containers/flat_set.h"
#include "ui/base/linux/linux_desktop.h"
#include "ui/base/x/x11_display_util.h"
#include "ui/base/x/x11_idle_query.h"
#include "ui/base/x/x11_screensaver.h"
#include "ui/base/x/x11_util.h"
#include "ui/display/display.h"
#include "ui/display/display_finder.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/window_cache.h"
#include "ui/ozone/platform/x11/x11_window.h"
#include "ui/ozone/platform/x11/x11_window_manager.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/linux/linux_ui.h"
#endif

namespace ui {

namespace {

using DisplayList = std::vector<display::Display>;

gfx::Rect DisplayBoundsInPixels(const display::Display& display) {
  return {display.native_origin(), display.GetSizeInPixel()};
}

const display::Display& GetDisplayForRect(const DisplayList& displays,
                                          const gfx::Rect& rect,
                                          bool rect_in_px) {
  DCHECK(!displays.empty());
  constexpr auto kMaxDist = std::make_pair(INT_MAX, INT_MAX);
  auto min_dist_display = std::make_pair(kMaxDist, displays.data());
  for (const auto& display : displays) {
    auto bounds =
        rect_in_px ? DisplayBoundsInPixels(display) : display.bounds();
    min_dist_display = std::min(
        min_dist_display, std::make_pair(RectDistance(bounds, rect), &display));
  }
  return *min_dist_display.second;
}

gfx::PointF PointPxToDip(const DisplayList& displays, gfx::Point point_px) {
  const auto& display =
      GetDisplayForRect(displays, gfx::Rect(point_px, gfx::Size(1, 1)), true);
  gfx::Vector2d delta_px = point_px - display.native_origin();
  gfx::Vector2dF delta_dip =
      gfx::ScaleVector2d(delta_px, 1.0 / display.device_scale_factor());
  return gfx::PointF(display.bounds().origin()) + delta_dip;
}

gfx::PointF PointDipToPx(const DisplayList& displays, gfx::Point point_dip) {
  const auto& display =
      GetDisplayForRect(displays, gfx::Rect(point_dip, gfx::Size(1, 1)), false);
  gfx::Rect bounds_dip = display.bounds();
  gfx::Vector2d delta_dip = point_dip - bounds_dip.origin();
  gfx::Vector2dF delta_px =
      gfx::ScaleVector2d(delta_dip, display.device_scale_factor());
  return gfx::PointF(display.native_origin()) + delta_px;
}

gfx::Point GetCursorScreenPointPx(x11::Connection& connection) {
  if (ui::X11EventSource::HasInstance()) {
    auto point_in_pixels =
        ui::X11EventSource::GetInstance()->last_cursor_location();
    if (point_in_pixels.has_value()) {
      return point_in_pixels.value();
    }
  }
  // This call is expensive so we explicitly only call it when
  // X11EventSource doesn't have a last_cursor_location set.
  auto response = connection.QueryPointer({connection.default_root()}).Sync();
  auto point_in_pixels =
      response ? gfx::Point(response->root_x, response->root_y) : gfx::Point();
  if (ui::X11EventSource::HasInstance()) {
    ui::X11EventSource::GetInstance()->set_last_cursor_location(
        point_in_pixels);
  }
  return point_in_pixels;
}

}  // namespace

X11ScreenOzone::X11ScreenOzone()
    : connection_(x11::Connection::Get()),
      window_manager_(X11WindowManager::GetInstance()),
      x11_display_manager_(std::make_unique<XDisplayManager>(this)) {
  DCHECK(window_manager_);
#if BUILDFLAG(IS_LINUX)
  if (auto* linux_ui = ui::LinuxUi::instance()) {
    display_scale_factor_observer_.Observe(linux_ui);
  }
#endif
}

X11ScreenOzone::~X11ScreenOzone() {
  if (x11_display_manager_->IsXrandrAvailable()) {
    connection_->RemoveEventObserver(this);
  }
}

void X11ScreenOzone::Init() {
  initialized_ = true;
  if (x11_display_manager_->IsXrandrAvailable()) {
    connection_->AddEventObserver(this);
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
  if (widget == gfx::kNullAcceleratedWidget) {
    return GetPrimaryDisplay();
  }

  X11Window* window = window_manager_->GetWindow(widget);
  if (window) {
    return GetDisplayForRect(GetAllDisplays(), window->GetBoundsInPixels(),
                             true);
  }
  return GetPrimaryDisplay();
}

gfx::Point X11ScreenOzone::GetCursorScreenPoint() const {
  // TODO(danakj): Should this be rounded? Or kept as a floating point?
  return gfx::ToFlooredPoint(
      PointPxToDip(GetAllDisplays(), GetCursorScreenPointPx(*connection_)));
}

bool X11ScreenOzone::IsAcceleratedWidgetUnderCursor(
    gfx::AcceleratedWidget widget) const {
  // Only ask the X11Window for its pointer state when some other window does
  // not have mouse capture because capture disrupts pointer event tracking.
  if (!window_manager_->located_events_grabber()) {
    if (X11Window* window = window_manager_->GetWindow(widget)) {
      return window->has_pointer();
    }
  }
  return GetAcceleratedWidgetAtScreenPoint(GetCursorScreenPoint()) == widget;
}

gfx::AcceleratedWidget X11ScreenOzone::GetAcceleratedWidgetAtScreenPoint(
    const gfx::Point& point) const {
  gfx::Point point_in_pixels =
      gfx::ToFlooredPoint(PointDipToPx(GetAllDisplays(), point));
  return static_cast<gfx::AcceleratedWidget>(
      x11::GetWindowAtPoint(point_in_pixels));
}

gfx::AcceleratedWidget X11ScreenOzone::GetLocalProcessWidgetAtPoint(
    const gfx::Point& point,
    const std::set<gfx::AcceleratedWidget>& ignore) const {
  gfx::AcceleratedWidget widget{};
  if (ignore.empty()) {
    widget = GetAcceleratedWidgetAtScreenPoint(point);
  } else {
    gfx::Point point_in_pixels =
        gfx::ToFlooredPoint(PointDipToPx(GetAllDisplays(), point));
    base::flat_set<x11::Window> ignore_windows;
    for (auto ignore_widget : ignore) {
      ignore_windows.insert(static_cast<x11::Window>(ignore_widget));
    }
    widget = static_cast<gfx::AcceleratedWidget>(
        x11::GetWindowAtPoint(point_in_pixels, &ignore_windows));
  }
  return window_manager_->GetWindow(widget) ? widget : gfx::AcceleratedWidget{};
}

display::Display X11ScreenOzone::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  auto displays = GetAllDisplays();
  if (displays.size() <= 1) {
    return GetPrimaryDisplay();
  }
  return *display::FindDisplayNearestPoint(displays, point);
}

display::Display X11ScreenOzone::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  const display::Display* matching_display =
      display::FindDisplayWithBiggestIntersection(
          x11_display_manager_->displays(), match_rect);
  return matching_display ? *matching_display : GetPrimaryDisplay();
}

X11ScreenOzone::X11ScreenSaverSuspender::X11ScreenSaverSuspender() {
  is_suspending_ = SuspendX11ScreenSaver(true);
}

std::unique_ptr<X11ScreenOzone::X11ScreenSaverSuspender>
X11ScreenOzone::X11ScreenSaverSuspender::Create() {
  auto suspender = base::WrapUnique(new X11ScreenSaverSuspender());
  if (suspender->is_suspending_) {
    return suspender;
  }

  return nullptr;
}

X11ScreenOzone::X11ScreenSaverSuspender::~X11ScreenSaverSuspender() {
  if (is_suspending_) {
    SuspendX11ScreenSaver(false);
  }
}

std::unique_ptr<PlatformScreen::PlatformScreenSaverSuspender>
X11ScreenOzone::SuspendScreenSaver() {
  return X11ScreenSaverSuspender::Create();
}

bool X11ScreenOzone::IsScreenSaverActive() const {
  // Usually the screensaver is used to lock the screen.
  return IsXScreensaverActive();
}

base::TimeDelta X11ScreenOzone::CalculateIdleTime() const {
  IdleQueryX11 idle_query;
  return base::Seconds(idle_query.IdleTime());
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

base::Value::List X11ScreenOzone::GetGpuExtraInfo(
    const gfx::GpuExtraInfo& gpu_extra_info) {
  auto result = GetDesktopEnvironmentInfo();
  StorePlatformNameIntoListOfValues(result, "x11");
  return result;
}

void X11ScreenOzone::OnEvent(const x11::Event& xev) {
  x11_display_manager_->OnEvent(xev);
}

#if BUILDFLAG(IS_LINUX)
void X11ScreenOzone::OnDeviceScaleFactorChanged() {
  x11_display_manager_->DispatchDelayedDisplayListUpdate();
}
#endif

void X11ScreenOzone::OnXDisplayListUpdated() {
  float scale_factor =
      x11_display_manager_->GetPrimaryDisplay().device_scale_factor();
  gfx::SetFontRenderParamsDeviceScaleFactor(scale_factor);
}

}  // namespace ui
