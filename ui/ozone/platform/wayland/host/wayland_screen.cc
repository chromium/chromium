// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_screen.h"

#include <set>
#include <vector>

#include "base/containers/contains.h"
#include "base/observer_list.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/linux/linux_desktop.h"
#include "ui/display/display.h"
#include "ui/display/display_finder.h"
#include "ui/display/display_list.h"
#include "ui/display/util/gpu_info_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/host/org_kde_kwin_idle.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor_position.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/zwp_idle_inhibit_manager.h"

#if defined(USE_DBUS)
#include "ui/ozone/platform/wayland/host/org_gnome_mutter_idle_monitor.h"
#endif

namespace ui {
namespace {

display::Display::Rotation WaylandTransformToRotation(int32_t transform) {
  switch (transform) {
    case WL_OUTPUT_TRANSFORM_NORMAL:
      return display::Display::ROTATE_0;
    case WL_OUTPUT_TRANSFORM_90:
      return display::Display::ROTATE_270;
    case WL_OUTPUT_TRANSFORM_180:
      return display::Display::ROTATE_180;
    case WL_OUTPUT_TRANSFORM_270:
      return display::Display::ROTATE_90;
    // ui::display::Display does not support flipped rotation.
    // see ui::display::Display::Rotation comment.
    case WL_OUTPUT_TRANSFORM_FLIPPED:
    case WL_OUTPUT_TRANSFORM_FLIPPED_90:
    case WL_OUTPUT_TRANSFORM_FLIPPED_180:
    case WL_OUTPUT_TRANSFORM_FLIPPED_270:
      NOTIMPLEMENTED_LOG_ONCE();
      return display::Display::ROTATE_0;
  }
  NOTREACHED();
  return display::Display::ROTATE_0;
}

}  // namespace

WaylandScreen::WaylandScreen(WaylandConnection* connection)
    : connection_(connection), weak_factory_(this) {
  DCHECK(connection_);

  // Chromium specifies either RGBA_8888 or BGRA_8888 as initial image format
  // for alpha case and RGBX_8888 for no alpha case. Figure out
  // which one is supported and use that. If RGBX_8888 is not supported, the
  // format that |have_format_alpha| uses will be used by default (RGBA_8888 or
  // BGRA_8888).
  auto buffer_formats =
      connection_->buffer_manager_host()->GetSupportedBufferFormats();
  for (const auto& buffer_format : buffer_formats) {
    auto format = buffer_format.first;

    // TODO(crbug.com/1127822): Investigate a better fix for this.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
    // RGBA_8888 is the preferred format, except when running on ChromiumOS. See
    // crbug.com/1127558.
    if (format == gfx::BufferFormat::RGBA_8888)
      image_format_alpha_ = gfx::BufferFormat::RGBA_8888;

      // TODO(1128997): |image_format_no_alpha_| should use RGBX_8888 when it's
      // available, but for some reason Chromium gets broken when it's used.
      // Though,  we can import RGBX_8888 dma buffer to EGLImage successfully.
      // Enable that back when the issue is resolved.
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

    if (!image_format_alpha_ && format == gfx::BufferFormat::BGRA_8888)
      image_format_alpha_ = gfx::BufferFormat::BGRA_8888;

    if (image_format_alpha_ && image_format_no_alpha_)
      break;
  }

  // If no buffer formats are found (neither wl_drm nor zwp_linux_dmabuf are
  // supported or the system has very limited set of supported buffer formats),
  // RGBA_8888 is used by default. On Wayland, that seems to be the most
  // supported.
  if (!image_format_alpha_)
    image_format_alpha_ = gfx::BufferFormat::RGBA_8888;
  if (!image_format_no_alpha_)
    image_format_no_alpha_ = image_format_alpha_;
}

WaylandScreen::~WaylandScreen() = default;

void WaylandScreen::OnOutputAddedOrUpdated(uint32_t output_id,
                                           const gfx::Point& origin,
                                           const gfx::Size& logical_size,
                                           const gfx::Size& physical_size,
                                           const gfx::Insets& insets,
                                           float scale,
                                           int32_t panel_transform,
                                           int32_t logical_transform) {
  AddOrUpdateDisplay(output_id, origin, logical_size, physical_size, insets,
                     scale, panel_transform, logical_transform);
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
  // TODO(https://crbug.com/1299403): Work around the symptoms of a common
  // crash. Unclear if this is the proper long term solution.
  auto it = display_list_.FindDisplayById(output_id);
  DCHECK(it != display_list_.displays().end());
  if (it != display_list_.displays().end()) {
    display_list_.RemoveDisplay(output_id);
  } else {
    LOG(ERROR) << "output_id is not associated with a Display.";
  }
}

void WaylandScreen::AddOrUpdateDisplay(uint32_t output_id,
                                       const gfx::Point& origin,
                                       const gfx::Size& logical_size,
                                       const gfx::Size& physical_size,
                                       const gfx::Insets& insets,
                                       float scale_factor,
                                       int32_t panel_transform,
                                       int32_t logical_transform) {
  display::Display changed_display(output_id);

  DCHECK_GE(panel_transform, WL_OUTPUT_TRANSFORM_NORMAL);
  DCHECK_LE(panel_transform, WL_OUTPUT_TRANSFORM_FLIPPED_270);
  display::Display::Rotation panel_rotation =
      WaylandTransformToRotation(panel_transform);
  changed_display.set_panel_rotation(panel_rotation);

  DCHECK_GE(logical_transform, WL_OUTPUT_TRANSFORM_NORMAL);
  DCHECK_LE(logical_transform, WL_OUTPUT_TRANSFORM_FLIPPED_270);
  display::Display::Rotation rotation =
      WaylandTransformToRotation(logical_transform);
  changed_display.set_rotation(rotation);

  gfx::Size size_in_pixels(physical_size);
  if (panel_rotation == display::Display::Rotation::ROTATE_90 ||
      panel_rotation == display::Display::Rotation::ROTATE_270) {
    size_in_pixels.Transpose();
  }
  changed_display.set_size_in_pixels(size_in_pixels);

  if (!logical_size.IsEmpty()) {
    changed_display.set_bounds(gfx::Rect(origin, logical_size));
    changed_display.SetScale(scale_factor);
  } else {
    // Fallback to calculating using physical size.
    // This can happen if xdg_output.logical_size was not sent.
    changed_display.SetScaleAndBounds(scale_factor, gfx::Rect(size_in_pixels));
    gfx::Rect new_bounds(changed_display.bounds());
    new_bounds.set_origin(origin);
    changed_display.set_bounds(new_bounds);
  }
  changed_display.UpdateWorkAreaFromInsets(insets);

  gfx::DisplayColorSpaces color_spaces;
  color_spaces.SetOutputBufferFormats(image_format_no_alpha_.value(),
                                      image_format_alpha_.value());
  changed_display.set_color_spaces(color_spaces);

  // There are 2 cases where |changed_display| must be set as primary:
  // 1. When it is the first one being added to the |display_list_|. Or
  // 2. If it is nearest the origin than the previous primary or has the
  // same origin as it. When an user, for example, swaps two side-by-side
  // displays, at some point, as the notification come in, both will have
  // the same origin.
  auto type = display::DisplayList::Type::NOT_PRIMARY;
  if (display_list_.displays().empty()) {
    type = display::DisplayList::Type::PRIMARY;
  } else {
    auto nearest_origin = GetDisplayNearestPoint({0, 0}).bounds().origin();
    auto changed_origin = changed_display.bounds().origin();
    auto nearest_dist = nearest_origin.OffsetFromOrigin().LengthSquared();
    auto changed_dist = changed_origin.OffsetFromOrigin().LengthSquared();
    if (changed_dist < nearest_dist || changed_origin == nearest_origin)
      type = display::DisplayList::Type::PRIMARY;
  }

  display_list_.AddOrUpdateDisplay(changed_display, type);
}

void WaylandScreen::OnTabletStateChanged(display::TabletState tablet_state) {
  auto* observer_list = display_list_.observers();
  for (auto& observer : *observer_list) {
    observer.OnDisplayTabletStateChanged(tablet_state);
  }
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

  const auto entered_output_id = window->GetPreferredEnteredOutputId();
  // Although spec says a surface receives enter/leave surface events on
  // create/move/resize actions, this might be called right after a window is
  // created, but it has not been configured by a Wayland compositor and it
  // has not received enter surface events yet. Another case is when a user
  // switches between displays in a single output mode - Wayland may not send
  // enter events immediately, which can result in empty container of entered
  // ids (check comments in WaylandWindow::OnEnteredOutputIdRemoved). In this
  // case, it's also safe to return the primary display.
  if (entered_output_id == 0)
    return GetPrimaryDisplay();

  DCHECK(!display_list_.displays().empty());
  for (const auto& display : display_list_.displays()) {
    if (display.id() == entered_output_id)
      return display;
  }

  NOTREACHED();
  return GetPrimaryDisplay();
}

gfx::Point WaylandScreen::GetCursorScreenPoint() const {
  // wl_shell/xdg-shell do not provide either location of surfaces in global
  // space coordinate system or location of a pointer. Instead, only locations
  // of mouse/touch events are known. Given that Chromium assumes top-level
  // windows are located at origin when screen coordinates is not available,
  // always provide a cursor point in regards to surfaces' location.
  //
  // If a pointer is located in any of the existing wayland windows, return
  // the last known cursor position.
  auto* cursor_position = connection_->wayland_cursor_position();
  if (connection_->wayland_window_manager()
          ->GetCurrentPointerOrTouchFocusedWindow() &&
      cursor_position)
    return cursor_position->GetCursorSurfacePoint();

  // Make sure the cursor position does not overlap with any window by using the
  // outside of largest window bounds.
  // TODO(oshima): Change this for the case that screen coordinates is
  // available.
  auto* window =
      connection_->wayland_window_manager()->GetWindowWithLargestBounds();
  DCHECK(window);
  const gfx::Rect bounds = window->GetBoundsInDIP();
  return gfx::Point(bounds.width() + 10, bounds.height() + 10);
}

gfx::AcceleratedWidget WaylandScreen::GetAcceleratedWidgetAtScreenPoint(
    const gfx::Point& point) const {
  // It is safe to check only for focused windows and test if they contain the
  // point or not.
  auto* window = connection_->wayland_window_manager()
                     ->GetCurrentPointerOrTouchFocusedWindow();
  if (window && window->GetBoundsInDIP().Contains(point))
    return window->GetWidget();
  return gfx::kNullAcceleratedWidget;
}

gfx::AcceleratedWidget WaylandScreen::GetLocalProcessWidgetAtPoint(
    const gfx::Point& point,
    const std::set<gfx::AcceleratedWidget>& ignore) const {
  auto widget = GetAcceleratedWidgetAtScreenPoint(point);
  return !widget || base::Contains(ignore, widget) ? gfx::kNullAcceleratedWidget
                                                   : widget;
}

display::Display WaylandScreen::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  auto displays = GetAllDisplays();
  if (displays.size() <= 1)
    return GetPrimaryDisplay();
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

bool WaylandScreen::SetScreenSaverSuspended(bool suspend) {
  if (!connection_->zwp_idle_inhibit_manager())
    return false;

  if (suspend) {
    // Wayland inhibits idle behaviour on certain output, and implies that a
    // surface bound to that output should obtain the inhibitor and hold it
    // until it no longer needs to prevent the output to go idle.
    // We assume that the idle lock is initiated by the user, and therefore the
    // surface that we should use is the one owned by the window that is focused
    // currently.
    const auto* window_manager = connection_->wayland_window_manager();
    DCHECK(window_manager);
    const auto* current_window = window_manager->GetCurrentFocusedWindow();
    if (!current_window) {
      LOG(WARNING) << "Cannot inhibit going idle when no window is focused";
      return false;
    }
    DCHECK(current_window->root_surface());
    idle_inhibitor_ = connection_->zwp_idle_inhibit_manager()->CreateInhibitor(
        current_window->root_surface()->surface());
  } else {
    idle_inhibitor_.reset();
  }

  return true;
}

bool WaylandScreen::IsScreenSaverActive() const {
  return idle_inhibitor_ != nullptr;
}

base::TimeDelta WaylandScreen::CalculateIdleTime() const {
  // Try the org_kde_kwin_idle Wayland protocol extension (KWin).
  if (const auto* kde_idle = connection_->org_kde_kwin_idle()) {
    const auto idle_time = kde_idle->GetIdleTime();
    if (idle_time)
      return *idle_time;
  }

#if defined(USE_DBUS)
  // Try the org.gnome.Mutter.IdleMonitor D-Bus service (Mutter).
  if (!org_gnome_mutter_idle_monitor_)
    org_gnome_mutter_idle_monitor_ =
        std::make_unique<OrgGnomeMutterIdleMonitor>();
  const auto idle_time = org_gnome_mutter_idle_monitor_->GetIdleTime();
  if (idle_time)
    return *idle_time;
#endif  // defined(USE_DBUS)

  NOTIMPLEMENTED_LOG_ONCE();

  // No providers.  Return 0 which means the system never gets idle.
  return base::Seconds(0);
}

void WaylandScreen::AddObserver(display::DisplayObserver* observer) {
  display_list_.AddObserver(observer);
}

void WaylandScreen::RemoveObserver(display::DisplayObserver* observer) {
  display_list_.RemoveObserver(observer);
}

std::vector<base::Value> WaylandScreen::GetGpuExtraInfo(
    const gfx::GpuExtraInfo& gpu_extra_info) {
  auto values = GetDesktopEnvironmentInfo();
  std::vector<std::string> protocols;
  for (const auto& protocol_and_version : connection_->available_globals()) {
    protocols.push_back(base::StringPrintf("%s:%u",
                                           protocol_and_version.first.c_str(),
                                           protocol_and_version.second));
  }
  values.push_back(
      display::BuildGpuInfoEntry("Interfaces exposed by the Wayland compositor",
                                 base::JoinString(protocols, " ")));
  StorePlatformNameIntoListOfValues(values, "wayland");
  return values;
}

}  // namespace ui
