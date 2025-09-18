// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/screen_win_headless.h"

#include <windows.h>

#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_number_conversions_win.h"
#include "base/strings/string_util.h"
#include "base/strings/string_util_win.h"
#include "components/headless/screen_info/headless_screen_info.h"
#include "ui/display/display_finder.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/win/display_info.h"
#include "ui/display/win/dpi.h"
#include "ui/display/win/screen_win_display.h"

namespace display::win {

namespace {

// Headless display device names are fakes that look similar to the real display
// device names.
constexpr WCHAR kHeadlessDisplayDeviceNamePrefix[] = LR"(\\.\HEADLESS_DISPLAY)";

std::wstring GetHeadlessDisplayDeviceNameFromDisplayId(int64_t id) {
  return base::StrCat(
      {kHeadlessDisplayDeviceNamePrefix, base::NumberToWString(id)});
}

int64_t GetHeadlessDisplayIdFromMonitorInfo(const MONITORINFOEX& monitor_info) {
  CHECK(base::StartsWith(monitor_info.szDevice,
                         kHeadlessDisplayDeviceNamePrefix));
  int64_t id;
  CHECK(base::StringToInt64(
      &monitor_info.szDevice[std::size(kHeadlessDisplayDeviceNamePrefix) - 1],
      &id));

  return id;
}

gfx::Vector2dF GetDisplayPhysicalPixelsPerInch(float device_scaling_factor) {
  const int dpi = GetDPIFromScalingFactor(device_scaling_factor);
  return gfx::Vector2dF(dpi, dpi);
}

void SetHeadlessDisplayDeviceName(MONITORINFOEX& monitor_info,
                                  int64_t display_id) {
  const std::wstring device_name =
      GetHeadlessDisplayDeviceNameFromDisplayId(display_id);
  CHECK_LT(device_name.length() + 1, std::size(monitor_info.szDevice));

  base::span<WCHAR> device_name_buf = monitor_info.szDevice;
  std::copy(device_name.begin(), device_name.end(), device_name_buf.begin());
  device_name_buf[device_name.length()] = L'\0';
}

}  // namespace

ScreenWinHeadless::ScreenWinHeadless(
    const std::vector<headless::HeadlessScreenInfo>& screen_infos)
    : ScreenWin(/*initialize_from_system=*/false) {
  CHECK(!screen_infos.empty());

  UpdateFromDisplayInfos(DisplayInfosFromScreenInfo(screen_infos));

  HeadlessScreenManager::Get()->SetDelegate(this);
}

ScreenWinHeadless::~ScreenWinHeadless() {
  HeadlessScreenManager::Get()->SetDelegate(nullptr);
}

int64_t ScreenWinHeadless::GetDisplayIdFromWindow(HWND hwnd,
                                                  DWORD default_options) {
  if (auto monitor_info = MonitorInfoFromWindow(hwnd, default_options)) {
    return GetDisplayIdFromMonitorInfo(monitor_info.value());
  }

  return kInvalidDisplayId;
}

int64_t ScreenWinHeadless::GetDisplayIdFromScreenRect(
    const gfx::Rect& screen_rect) {
  if (auto monitor_info = MonitorInfoFromScreenRect(screen_rect)) {
    return GetDisplayIdFromMonitorInfo(monitor_info.value());
  }

  return GetPrimaryDisplay().id();
}

int ScreenWinHeadless::GetSystemMetricsForDisplayId(int64_t id, int metric) {
  const Display display = GetScreenWinDisplayWithDisplayId(id).display();
  return base::ClampRound(::GetSystemMetrics(metric) *
                          display.device_scale_factor());
}

void ScreenWinHeadless::SetCursorScreenPointForTesting(
    const gfx::Point& point) {
  cursor_screen_point_ = point;
}

gfx::Point ScreenWinHeadless::GetCursorScreenPoint() {
  return cursor_screen_point_;
}

bool ScreenWinHeadless::IsWindowUnderCursor(gfx::NativeWindow window) {
  return GetWindowAtScreenPoint(GetCursorScreenPoint()) == window;
}

gfx::NativeWindow ScreenWinHeadless::GetWindowAtScreenPoint(
    const gfx::Point& point) {
  return GetNativeWindowAtScreenPoint(point, std::set<gfx::NativeWindow>());
}

gfx::NativeWindow ScreenWinHeadless::GetLocalProcessWindowAtPoint(
    const gfx::Point& point,
    const std::set<gfx::NativeWindow>& ignore) {
  return GetRootWindow(GetNativeWindowAtScreenPoint(point, ignore));
}

int ScreenWinHeadless::GetNumDisplays() const {
  return GetAllDisplays().size();
}

const std::vector<Display>& ScreenWinHeadless::GetAllDisplays() const {
  return ScreenWin::GetAllDisplays();
}

Display ScreenWinHeadless::GetDisplayNearestWindow(
    gfx::NativeWindow window) const {
  if (window) {
    return GetDisplayFromScreenRect(GetNativeWindowBoundsInScreen(window));
  }

  return GetPrimaryDisplay();
}

Display ScreenWinHeadless::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  return GetDisplayFromScreenPoint(point);
}

Display ScreenWinHeadless::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  return GetDisplayFromScreenRect(match_rect);
}

Display ScreenWinHeadless::GetPrimaryDisplay() const {
  // In headless the primary display is always the first display.
  return GetNumDisplays() ? GetAllDisplays()[0] : Display::GetDefaultDisplay();
}

HMONITOR ScreenWinHeadless::HMONITORFromScreenPoint(
    const gfx::Point& screen_point) const {
  NOTREACHED();
}

HMONITOR ScreenWinHeadless::HMONITORFromScreenRect(
    const gfx::Rect& screen_rect) const {
  NOTREACHED();
}

HMONITOR ScreenWinHeadless::HMONITORFromWindow(HWND hwnd,
                                               DWORD default_options) const {
  NOTREACHED();
}

std::optional<MONITORINFOEX> ScreenWinHeadless::MonitorInfoFromScreenPoint(
    const gfx::Point& screen_point) const {
  // ScreenWin::MonitorInfoFromScreenPoint() uses Win32 ::MonitorFromPoint()
  // with MONITOR_DEFAULTTONEAREST flag.
  if (const Display* display =
          FindDisplayNearestPoint(GetAllDisplays(), screen_point)) {
    return GetMONITORINFOFromDisplayId(display->id());
  }

  return std::nullopt;
}

std::optional<MONITORINFOEX> ScreenWinHeadless::MonitorInfoFromHMONITOR(
    HMONITOR monitor) const {
  return std::nullopt;
}

gfx::Rect ScreenWinHeadless::ScreenToDIPRectInWindow(
    gfx::NativeWindow window,
    const gfx::Rect& screen_rect) const {
  // The base class implementation does the right thing, but we want this to be
  // exposed publicly as the rest of display::Screen overrides.
  return ScreenWin::ScreenToDIPRectInWindow(window, screen_rect);
}

gfx::Rect ScreenWinHeadless::DIPToScreenRectInWindow(
    gfx::NativeWindow window,
    const gfx::Rect& dip_rect) const {
  // The base class implementation does the right thing, but we want this to be
  // exposed publicly as the rest of display::Screen overrides.
  return ScreenWin::DIPToScreenRectInWindow(window, dip_rect);
}

bool ScreenWinHeadless::IsHeadless() const {
  return true;
}

std::optional<MONITORINFOEX> ScreenWinHeadless::MonitorInfoFromScreenRect(
    const gfx::Rect& screen_rect) const {
  // ScreenWin::MonitorInfoFromScreenRect() uses Win32 ::MonitorFromRect() with
  // MONITOR_DEFAULTTONEAREST flag.
  if (const Display* display =
          FindDisplayWithBiggestIntersection(GetAllDisplays(), screen_rect)) {
    return GetMONITORINFOFromDisplayId(display->id());
  }

  if (const Display* display = FindDisplayNearestPoint(
          GetAllDisplays(), screen_rect.CenterPoint())) {
    return GetMONITORINFOFromDisplayId(display->id());
  }

  return std::nullopt;
}

std::optional<MONITORINFOEX> ScreenWinHeadless::MonitorInfoFromWindow(
    HWND hwnd,
    DWORD default_options) const {
  CHECK(hwnd);
  const gfx::Rect bounds = GetHeadlessWindowBounds(hwnd);

  // ScreenWin::MonitorInfoFromWindow() calls Win32 ::MonitorFromWindow() so
  // replicate its behavior according to
  // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-monitorfromwindow
  if (const Display* display =
          FindDisplayWithBiggestIntersection(GetAllDisplays(), bounds)) {
    return GetMONITORINFOFromDisplayId(display->id());
  }

  if (default_options == MONITOR_DEFAULTTONEAREST) {
    if (const Display* display =
            FindDisplayNearestPoint(GetAllDisplays(), bounds.CenterPoint())) {
      return GetMONITORINFOFromDisplayId(display->id());
    }
  } else if (default_options == MONITOR_DEFAULTTOPRIMARY) {
    return GetMONITORINFOFromDisplayId(GetPrimaryDisplay().id());
  }

  return std::nullopt;
}

HWND ScreenWinHeadless::GetRootWindow(HWND hwnd) const {
  // Headless windows don't have hierarchy, so return self.
  return hwnd;
}

int64_t ScreenWinHeadless::GetDisplayIdFromMonitorInfo(
    const MONITORINFOEX& monitor_info) const {
  // This will crash if called with the real Windows monitor info.
  return GetHeadlessDisplayIdFromMonitorInfo(monitor_info);
}

void ScreenWinHeadless::UpdateAllDisplaysAndNotify() {
  // Ignore all display update requests because the underlying implementation
  // requests display infos from the system and overrides headless screen
  // configuration. Headless screen configuration is defined in ctor and never
  // changes.
}

void ScreenWinHeadless::UpdateAllDisplaysIfPrimaryMonitorChanged() {
  // Headless primary monitor never changes, so intercept and ignore.
}

void ScreenWinHeadless::OnColorProfilesChanged() {
  // Just ignore as we don't expect any on the fly color profile changes in
  // headless mode.
}

gfx::NativeWindow ScreenWinHeadless::GetNativeWindowAtScreenPoint(
    const gfx::Point& point,
    const std::set<gfx::NativeWindow>& ignore) const {
  NOTREACHED();
}

gfx::Rect ScreenWinHeadless::GetNativeWindowBoundsInScreen(
    gfx::NativeWindow window) const {
  NOTREACHED();
}

gfx::Rect ScreenWinHeadless::GetHeadlessWindowBounds(
    gfx::AcceleratedWidget window) const {
  NOTREACHED();
}

gfx::NativeWindow ScreenWinHeadless::GetRootWindow(
    gfx::NativeWindow window) const {
  NOTREACHED();
}

ScreenWinDisplay ScreenWinHeadless::GetScreenWinDisplayNearestHWND(
    HWND hwnd) const {
  return GetScreenWinDisplay(
      MonitorInfoFromWindow(hwnd, MONITOR_DEFAULTTONEAREST));
}

ScreenWinDisplay ScreenWinHeadless::GetScreenWinDisplayNearestScreenRect(
    const gfx::Rect& screen_rect) const {
  return GetScreenWinDisplay(MonitorInfoFromScreenRect(screen_rect));
}

ScreenWinDisplay ScreenWinHeadless::GetScreenWinDisplayNearestScreenPoint(
    const gfx::Point& screen_point) const {
  return GetScreenWinDisplay(MonitorInfoFromScreenPoint(screen_point));
}

ScreenWinDisplay ScreenWinHeadless::GetPrimaryScreenWinDisplay() const {
  // ScreenWin::GetPrimaryScreenWinDisplay() searches the ScreenWinDisplay
  // table for a display with origin at (0,0), however, for headless primary
  // display is always the first registered display.
  const int64_t id = GetPrimaryDisplay().id();
  return GetScreenWinDisplayWithDisplayId(id);
}

ScreenWinDisplay ScreenWinHeadless::GetScreenWinDisplay(
    std::optional<MONITORINFOEX> monitor_info) const {
  // ScreenWin::GetScreenWinDisplay() calls
  // DisplayInfo::DisplayIdFromMonitorInfo() to derive display id from monitor
  // info. Headless display ids are synthesized, so retrieve display id from
  // the headless display device name.
  if (monitor_info) {
    const int64_t id = GetHeadlessDisplayIdFromMonitorInfo(*monitor_info);
    return GetScreenWinDisplayWithDisplayId(id);
  }

  return GetPrimaryScreenWinDisplay();
}

ScreenWinDisplay ScreenWinHeadless::GetScreenWinDisplayForHMONITOR(
    HMONITOR monitor) const {
  // Headless displays don't have a real HMONITOR, so all paths that call this
  // method should be overridden.
  NOTREACHED();
}

int64_t ScreenWinHeadless::AddDisplay(const Display& display) {
  int64_t display_id = HeadlessScreenManager::GetNewDisplayId();

  MONITORINFOEX monitor_info = {};
  monitor_info.cbSize = sizeof(monitor_info);

  // Display's bounds and work area have scale factor already applied, so we
  // have to unscale them to get the correct monitor info geometry.
  if (display.device_scale_factor() == 1.0f) {
    monitor_info.rcMonitor = display.bounds().ToRECT();
    monitor_info.rcWork = display.work_area().ToRECT();
  } else {
    const float scale_factor = display.device_scale_factor();
    monitor_info.rcMonitor =
        gfx::ScaleToEnclosingRect(display.bounds(), scale_factor).ToRECT();
    monitor_info.rcWork =
        gfx::ScaleToEnclosingRect(display.work_area(), scale_factor).ToRECT();
  }

  SetHeadlessDisplayDeviceName(monitor_info, display_id);

  headless_monitor_info_.insert({display_id, monitor_info});

  internal::DisplayInfo display_info(
      display_id, monitor_info, display.device_scale_factor(),
      /*sdr_white_level=*/200.0,
      /*rotation=*/display.rotation(),
      /*display_frequency=*/60.0,
      /*pixels_per_inch=*/
      GetDisplayPhysicalPixelsPerInch(display.device_scale_factor()),
      /*output_technology=*/display.IsInternal()
          ? DISPLAYCONFIG_OUTPUT_TECHNOLOGY_INTERNAL
          : DISPLAYCONFIG_OUTPUT_TECHNOLOGY_OTHER,
      display.label());

  // Get the existing display infos and append the new one.
  std::vector<internal::DisplayInfo> display_infos = GetExistingDisplayInfos();
  display_infos.push_back(display_info);

  UpdateFromDisplayInfos(display_infos);

  return display_id;
}

void ScreenWinHeadless::RemoveDisplay(int64_t display_id) {
  CHECK_NE(display_id, GetPrimaryDisplay().id());

  if (!headless_monitor_info_.erase(display_id)) {
    return;
  }

  // Get the existing display infos except for the one being removed.
  std::vector<internal::DisplayInfo> display_infos =
      GetExistingDisplayInfos(/*except_display_id=*/display_id);

  UpdateFromDisplayInfos(display_infos);
}

std::vector<internal::DisplayInfo>
ScreenWinHeadless::DisplayInfosFromScreenInfo(
    const std::vector<headless::HeadlessScreenInfo>& screen_infos) {
  CHECK(!screen_infos.empty());

  std::optional<float> forced_device_scale_factor;
  if (Display::HasForceDeviceScaleFactor()) {
    forced_device_scale_factor = Display::GetForcedDeviceScaleFactor();
  }

  bool is_primary = true;
  std::vector<internal::DisplayInfo> display_infos;
  for (const auto& screen_info : screen_infos) {
    int64_t display_id = HeadlessScreenManager::GetNewDisplayId();

    MONITORINFOEX monitor_info;
    monitor_info.cbSize = sizeof(monitor_info);
    monitor_info.rcMonitor = screen_info.bounds.ToRECT();

    if (screen_info.work_area_insets.IsEmpty()) {
      monitor_info.rcWork = monitor_info.rcMonitor;
    } else {
      gfx::Rect work_area = screen_info.bounds;
      work_area.Inset(screen_info.work_area_insets);
      monitor_info.rcWork = work_area.ToRECT();
    }

    monitor_info.dwFlags = is_primary ? MONITORINFOF_PRIMARY : 0;

    SetHeadlessDisplayDeviceName(monitor_info, display_id);

    const float device_scale_factor =
        forced_device_scale_factor.value_or(screen_info.device_pixel_ratio);

    // Maintain display id to monitor info association for all the
    // MonitorInfoFromScreen*() functions below.
    headless_monitor_info_.insert({display_id, monitor_info});

    internal::DisplayInfo display_info(
        display_id, monitor_info, device_scale_factor,
        /*sdr_white_level=*/200.0,
        /*rotation=*/Display::DegreesToRotation(screen_info.rotation),
        /*display_frequency=*/60.0,
        /*pixels_per_inch=*/
        GetDisplayPhysicalPixelsPerInch(screen_info.device_pixel_ratio),
        /*output_technology=*/screen_info.is_internal
            ? DISPLAYCONFIG_OUTPUT_TECHNOLOGY_INTERNAL
            : DISPLAYCONFIG_OUTPUT_TECHNOLOGY_OTHER,
        screen_info.label);

    display_infos.push_back(std::move(display_info));

    is_primary = false;
  }

  return display_infos;
}

std::vector<internal::DisplayInfo> ScreenWinHeadless::GetExistingDisplayInfos(
    int64_t except_display_id) {
  std::vector<internal::DisplayInfo> display_infos;

  for (const Display& display : GetAllDisplays()) {
    if (display.id() == except_display_id) {
      continue;
    }

    auto monitor_info = GetMONITORINFOFromDisplayId(display.id());
    CHECK(monitor_info);

    internal::DisplayInfo display_info(
        display.id(), *monitor_info, display.device_scale_factor(),
        /*sdr_white_level=*/200.0,
        /*rotation=*/display.rotation(),
        /*display_frequency=*/60.0,
        /*pixels_per_inch=*/
        GetDisplayPhysicalPixelsPerInch(display.device_scale_factor()),
        /*output_technology=*/display.IsInternal()
            ? DISPLAYCONFIG_OUTPUT_TECHNOLOGY_INTERNAL
            : DISPLAYCONFIG_OUTPUT_TECHNOLOGY_OTHER,
        display.label());

    display_infos.push_back(std::move(display_info));
  }

  return display_infos;
}

Display ScreenWinHeadless::GetDisplayFromScreenPoint(
    const gfx::Point& point) const {
  if (const Display* display =
          FindDisplayNearestPoint(GetAllDisplays(), point)) {
    return *display;
  }

  return GetPrimaryDisplay();
}

Display ScreenWinHeadless::GetDisplayFromScreenRect(
    const gfx::Rect& rect) const {
  if (const Display* display =
          FindDisplayWithBiggestIntersection(GetAllDisplays(), rect)) {
    return *display;
  }

  if (const Display* display =
          FindDisplayNearestPoint(GetAllDisplays(), rect.CenterPoint())) {
    return *display;
  }

  return GetPrimaryDisplay();
}

std::optional<MONITORINFOEX>
ScreenWinHeadless::GetMONITORINFOFromDisplayIdForTest(int64_t id) const {
  return GetMONITORINFOFromDisplayId(id);
}

std::optional<MONITORINFOEX> ScreenWinHeadless::GetMONITORINFOFromDisplayId(
    int64_t id) const {
  auto it = headless_monitor_info_.find(id);
  if (it == headless_monitor_info_.cend()) {
    return std::nullopt;
  }

  return it->second;
}

DISPLAY_EXPORT ScreenWinHeadless* GetScreenWinHeadless() {
  ScreenWin* screen_win = GetScreenWin();
  CHECK(screen_win->IsHeadless());
  return static_cast<ScreenWinHeadless*>(screen_win);
}

namespace internal {
bool VerifyHeadlessDisplayDeviceName(int64_t id,
                                     const MONITORINFOEX& monitor_info) {
  return GetHeadlessDisplayDeviceNameFromDisplayId(id) == monitor_info.szDevice;
}
}  // namespace internal

}  // namespace display::win
