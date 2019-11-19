// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/screen_win.h"

#include <windows.h>
#include <shellscalingapi.h>

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/win/display_info.h"
#include "ui/display/win/dpi.h"
#include "ui/display/win/scaling_util.h"
#include "ui/display/win/screen_win_display.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/icc_profile.h"

namespace display {
namespace win {
namespace {

// TODO(robliao): http://crbug.com/615514 Remove when ScreenWin usage is
// resolved with Desktop Aura and WindowTreeHost.
ScreenWin* g_screen_win_instance = nullptr;

// Gets the DPI for a particular monitor, or 0 if per-monitor DPI is nuot
// supported or can't be read.
int GetPerMonitorDPI(HMONITOR monitor) {
  // Most versions of Windows we will encounter are DPI-aware.
  if (!base::win::IsProcessPerMonitorDpiAware())
    return 0;

  static auto get_dpi_for_monitor_func = []() {
    using GetDpiForMonitorPtr = decltype(::GetDpiForMonitor)*;
    HMODULE shcore_dll = ::LoadLibrary(L"shcore.dll");
    if (shcore_dll) {
      return reinterpret_cast<GetDpiForMonitorPtr>(
          ::GetProcAddress(shcore_dll, "GetDpiForMonitor"));
    }
    return static_cast<GetDpiForMonitorPtr>(nullptr);
  }();

  if (!get_dpi_for_monitor_func)
    return 0;

  UINT dpi_x;
  UINT dpi_y;
  if (!SUCCEEDED(get_dpi_for_monitor_func(monitor, MDT_EFFECTIVE_DPI, &dpi_x,
                                          &dpi_y))) {
    return 0;
  }

  DCHECK_EQ(dpi_x, dpi_y);
  return int{dpi_x};
}

// Gets the raw monitor scale factor.
//
// Respects the forced device scale factor, and will fall back to the global
// scale factor if per-monitor DPI is not supported.
float GetMonitorScaleFactor(HMONITOR monitor,
                            bool include_accessibility = true) {
  DCHECK(monitor);
  if (Display::HasForceDeviceScaleFactor())
    return Display::GetForcedDeviceScaleFactor();

  int dpi = GetPerMonitorDPI(monitor);
  if (!dpi)
    return GetDPIScale();

  float scale_factor = display::win::internal::GetScalingFactorFromDPI(dpi);
  if (include_accessibility) {
    float text_scale_factor =
        UwpTextScaleFactor::Instance()->GetTextScaleFactor();
    scale_factor *= text_scale_factor;
  }
  return scale_factor;
}

bool GetPathInfo(HMONITOR monitor, DISPLAYCONFIG_PATH_INFO* path_info) {
  LONG result;
  uint32_t num_path_array_elements = 0;
  uint32_t num_mode_info_array_elements = 0;
  std::vector<DISPLAYCONFIG_PATH_INFO> path_infos;
  std::vector<DISPLAYCONFIG_MODE_INFO> mode_infos;

  // Get the monitor name.
  MONITORINFOEXW view_info;
  view_info.cbSize = sizeof(view_info);
  if (!GetMonitorInfoW(monitor, &view_info))
    return false;

  // Get all path infos.
  do {
    if (GetDisplayConfigBufferSizes(
            QDC_ONLY_ACTIVE_PATHS, &num_path_array_elements,
            &num_mode_info_array_elements) != ERROR_SUCCESS) {
      return false;
    }
    path_infos.resize(num_path_array_elements);
    mode_infos.resize(num_mode_info_array_elements);
    result = QueryDisplayConfig(
        QDC_ONLY_ACTIVE_PATHS, &num_path_array_elements, path_infos.data(),
        &num_mode_info_array_elements, mode_infos.data(), nullptr);
  } while (result == ERROR_INSUFFICIENT_BUFFER);

  // Iterate of the path infos and see if we find one with a matching name.
  if (result == ERROR_SUCCESS) {
    for (uint32_t p = 0; p < num_path_array_elements; p++) {
      DISPLAYCONFIG_SOURCE_DEVICE_NAME device_name;
      device_name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
      device_name.header.size = sizeof(device_name);
      device_name.header.adapterId = path_infos[p].sourceInfo.adapterId;
      device_name.header.id = path_infos[p].sourceInfo.id;
      if (DisplayConfigGetDeviceInfo(&device_name.header) == ERROR_SUCCESS) {
        if (wcscmp(view_info.szDevice, device_name.viewGdiDeviceName) == 0) {
          *path_info = path_infos[p];
          return true;
        }
      }
    }
  }
  return false;
}

float GetMonitorSDRWhiteLevel(HMONITOR monitor) {
  float ret = 200.0;  // default value
  DISPLAYCONFIG_PATH_INFO path_info = {};
  if (!GetPathInfo(monitor, &path_info))
    return ret;

  DISPLAYCONFIG_SDR_WHITE_LEVEL white_level = {};
  white_level.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL;
  white_level.header.size = sizeof(white_level);
  white_level.header.adapterId = path_info.targetInfo.adapterId;
  white_level.header.id = path_info.targetInfo.id;
  if (DisplayConfigGetDeviceInfo(&white_level.header) != ERROR_SUCCESS)
    return ret;
  ret = white_level.SDRWhiteLevel * 80.0 / 1000.0;
  return ret;
}

void GetDisplaySettingsForDevice(const wchar_t* device_name,
                                 Display::Rotation* rotation,
                                 int* frequency) {
  *rotation = Display::ROTATE_0;
  *frequency = 0;
  DEVMODE mode = {};
  mode.dmSize = sizeof(mode);
  if (::EnumDisplaySettings(device_name, ENUM_CURRENT_SETTINGS, &mode)) {
    switch (mode.dmDisplayOrientation) {
      case DMDO_DEFAULT:
        *rotation = Display::ROTATE_0;
        break;
      case DMDO_90:
        *rotation = Display::ROTATE_90;
        break;
      case DMDO_180:
        *rotation = Display::ROTATE_180;
        break;
      case DMDO_270:
        *rotation = Display::ROTATE_270;
        break;
      default:
        NOTREACHED();
    }
    *frequency = mode.dmDisplayFrequency;
  }
}

std::vector<DisplayInfo> FindAndRemoveTouchingDisplayInfos(
    const DisplayInfo& ref_display_info,
    std::vector<DisplayInfo>* display_infos) {
  std::vector<DisplayInfo> touching_display_infos;
  base::EraseIf(*display_infos, [&touching_display_infos, ref_display_info](
      const DisplayInfo& display_info) {
    if (DisplayInfosTouch(ref_display_info, display_info)) {
      touching_display_infos.push_back(display_info);
      return true;
    }
    return false;
  });
  return touching_display_infos;
}

Display CreateDisplayFromDisplayInfo(const DisplayInfo& display_info,
                                     ColorProfileReader* color_profile_reader,
                                     bool hdr_enabled) {
  Display display(display_info.id());
  float scale_factor = display_info.device_scale_factor();
  display.set_device_scale_factor(scale_factor);
  display.set_work_area(
      gfx::ScaleToEnclosingRect(display_info.screen_work_rect(),
                                1.0f / scale_factor));
  display.set_bounds(gfx::ScaleToEnclosingRect(display_info.screen_rect(),
                     1.0f / scale_factor));
  display.set_rotation(display_info.rotation());
  display.set_display_frequency(display_info.display_frequency());
  if (!Display::HasForceDisplayColorProfile()) {
    if (hdr_enabled) {
      // Using RGBA F16 backbuffers required by SCRGB linear causes stuttering
      // on Windows RS3, but RGB10A2 with HDR10 color space works fine.
      gfx::ColorSpace hdr_color_space =
          base::win::GetVersion() > base::win::Version::WIN10_RS3
              ? gfx::ColorSpace::CreateSCRGBLinear()
              : gfx::ColorSpace::CreateHDR10();
      display.SetColorSpaceAndDepth(hdr_color_space,
                                    display_info.sdr_white_level());
    } else {
      display.SetColorSpaceAndDepth(
          color_profile_reader->GetDisplayColorSpace(display_info.id()));
    }
  }
  return display;
}

// Windows historically has had a hard time handling displays of DPIs higher
// than 96. Handling multiple DPI displays means we have to deal with Windows'
// monitor physical coordinates and map into Chrome's DIP coordinates.
//
// To do this, DisplayInfosToScreenWinDisplays reasons over monitors as a tree
// using the primary monitor as the root. All monitors touching this root are
// considered a children.
//
// This also presumes that all monitors are connected components. Windows, by UI
// construction restricts the layout of monitors to connected components except
// when DPI virtualization is happening. When this happens, we scale relative
// to (0, 0).
//
// Note that this does not handle cases where a scaled display may have
// insufficient room to lay out its children. In these cases, a DIP point could
// map to multiple screen points due to overlap. The first discovered screen
// will take precedence.
std::vector<ScreenWinDisplay> DisplayInfosToScreenWinDisplays(
    const std::vector<DisplayInfo>& display_infos,
    ColorProfileReader* color_profile_reader,
    bool hdr_enabled) {
  // Find and extract the primary display.
  std::vector<DisplayInfo> display_infos_remaining = display_infos;
  auto primary_display_iter = std::find_if(
      display_infos_remaining.begin(), display_infos_remaining.end(), [](
          const DisplayInfo& display_info) {
        return display_info.screen_rect().origin().IsOrigin();
      });
  DCHECK(primary_display_iter != display_infos_remaining.end()) <<
      "Missing primary display.";

  std::vector<DisplayInfo> available_parents;
  available_parents.push_back(*primary_display_iter);
  DisplayLayoutBuilder builder(primary_display_iter->id());
  display_infos_remaining.erase(primary_display_iter);
  // Build the tree and determine DisplayPlacements along the way.
  while (available_parents.size()) {
    const DisplayInfo parent = available_parents.back();
    available_parents.pop_back();
    for (const auto& child :
         FindAndRemoveTouchingDisplayInfos(parent, &display_infos_remaining)) {
      builder.AddDisplayPlacement(CalculateDisplayPlacement(parent, child));
      available_parents.push_back(child);
    }
  }

  // Layout and create the ScreenWinDisplays.
  std::vector<Display> displays;
  for (const auto& display_info : display_infos) {
    displays.push_back(CreateDisplayFromDisplayInfo(
        display_info, color_profile_reader, hdr_enabled));
  }

  std::unique_ptr<DisplayLayout> layout(builder.Build());
  layout->ApplyToDisplayList(&displays, nullptr, 0);

  std::vector<ScreenWinDisplay> screen_win_displays;
  const size_t num_displays = display_infos.size();
  for (size_t i = 0; i < num_displays; ++i)
    screen_win_displays.emplace_back(displays[i], display_infos[i]);

  return screen_win_displays;
}

std::vector<Display> ScreenWinDisplaysToDisplays(
    const std::vector<ScreenWinDisplay>& screen_win_displays) {
  std::vector<Display> displays;
  for (const auto& screen_win_display : screen_win_displays)
    displays.push_back(screen_win_display.display());
  return displays;
}

MONITORINFOEX MonitorInfoFromHMONITOR(HMONITOR monitor) {
  MONITORINFOEX monitor_info = {};
  monitor_info.cbSize = sizeof(monitor_info);
  ::GetMonitorInfo(monitor, &monitor_info);
  return monitor_info;
}

gfx::Vector2dF GetPixelsPerInchForPointerDevice(HANDLE source_device) {
  gfx::Vector2dF pixels_per_inch;
  static const auto get_pointer_device_rects =
      reinterpret_cast<decltype(&::GetPointerDeviceRects)>(
          base::win::GetUser32FunctionPointer("GetPointerDeviceRects"));
  if (!get_pointer_device_rects)
    return pixels_per_inch;

  RECT screen = {};
  RECT device = {};
  if (get_pointer_device_rects(source_device, &device, &screen)) {
    constexpr float kHimetricPerInch = 2540.f;
    float himetric_to_pixel_ratio_x =
        float{device.right - device.left} / float{screen.right - screen.left};
    float himetric_to_pixel_ratio_y =
        float{device.bottom - device.top} / float{screen.bottom - screen.top};
    pixels_per_inch.set_x(kHimetricPerInch / himetric_to_pixel_ratio_x);
    pixels_per_inch.set_y(kHimetricPerInch / himetric_to_pixel_ratio_y);
    return pixels_per_inch;
  }

  return pixels_per_inch;
}

BOOL CALLBACK EnumMonitorForDisplayInfoCallback(HMONITOR monitor,
                                                HDC hdc,
                                                LPRECT rect,
                                                LPARAM data) {
  std::vector<DisplayInfo>* display_infos =
      reinterpret_cast<std::vector<DisplayInfo>*>(data);
  DCHECK(display_infos);

  Display::Rotation rotation;
  int display_frequency;
  MONITORINFOEX monitor_info = MonitorInfoFromHMONITOR(monitor);
  GetDisplaySettingsForDevice(monitor_info.szDevice, &rotation,
                              &display_frequency);
  // Get the count of pointer devices.
  static const auto get_pointer_devices =
      reinterpret_cast<decltype(&::GetPointerDevices)>(
          base::win::GetUser32FunctionPointer("GetPointerDevices"));
  uint32_t pointer_device_count = 0;
  if (get_pointer_devices)
    get_pointer_devices(&pointer_device_count, nullptr);

  // Map touch pointer device to |monitor| and retrieve pixels per inch
  // for the |monitor| based on pointer device handle.
  gfx::Vector2dF pixels_per_inch;
  if (pointer_device_count != 0) {
    // Get all pointer devices.
    std::vector<POINTER_DEVICE_INFO> pointer_devices(pointer_device_count);
    if (get_pointer_devices(&pointer_device_count, &pointer_devices.front())) {
      for (uint32_t i = 0; i < pointer_device_count; i++) {
        if (pointer_devices[i].pointerDeviceType == POINTER_DEVICE_TYPE_TOUCH &&
            pointer_devices[i].monitor == monitor) {
          pixels_per_inch =
              GetPixelsPerInchForPointerDevice(pointer_devices[i].device);
          break;
        }
      }

      if (pixels_per_inch.IsZero()) {
        int default_pixels_per_inch = GetPerMonitorDPI(monitor);
        pixels_per_inch.set_x(default_pixels_per_inch);
        pixels_per_inch.set_y(default_pixels_per_inch);
      }
    }
  }

  display_infos->push_back(
      DisplayInfo(monitor_info, GetMonitorScaleFactor(monitor),
                  GetMonitorSDRWhiteLevel(monitor), rotation, display_frequency,
                  pixels_per_inch));
  return TRUE;
}

std::vector<DisplayInfo> GetDisplayInfosFromSystem() {
  std::vector<DisplayInfo> display_infos;
  EnumDisplayMonitors(nullptr, nullptr, EnumMonitorForDisplayInfoCallback,
                      reinterpret_cast<LPARAM>(&display_infos));
  DCHECK_EQ(static_cast<size_t>(::GetSystemMetrics(SM_CMONITORS)),
            display_infos.size());
  return display_infos;
}

// Returns a point in |to_origin|'s coordinates and position scaled by
// |scale_factor|.
gfx::PointF ScalePointRelative(const gfx::Point& from_origin,
                               const gfx::Point& to_origin,
                               const float scale_factor,
                               const gfx::PointF& point) {
  gfx::Vector2d from_origin_vector(from_origin.x(), from_origin.y());
  gfx::Vector2d to_origin_vector(to_origin.x(), to_origin.y());
  gfx::PointF scaled_relative_point(
      gfx::ScalePoint(point - from_origin_vector, scale_factor));
  return scaled_relative_point + to_origin_vector;
}
}  // namespace

ScreenWin::ScreenWin() : ScreenWin(true) {}

ScreenWin::ScreenWin(bool initialize)
    : color_profile_reader_(new ColorProfileReader(this)) {
  DCHECK(!g_screen_win_instance);
  g_screen_win_instance = this;
  if (initialize)
    Initialize();
}

ScreenWin::~ScreenWin() {
  DCHECK_EQ(g_screen_win_instance, this);
  if (uwp_text_scale_factor_)
    uwp_text_scale_factor_->RemoveObserver(this);

  g_screen_win_instance = nullptr;
}

// static
int ScreenWin::GetSystemMetricsForScaleFactor(float scale_factor, int metric) {
  if (base::win::IsProcessPerMonitorDpiAware()) {
    using GetSystemMetricsForDpiPtr = decltype(::GetSystemMetricsForDpi)*;
    static const auto get_metric_for_dpi_func =
        reinterpret_cast<GetSystemMetricsForDpiPtr>(
            base::win::GetUser32FunctionPointer("GetSystemMetricsForDpi"));
    if (get_metric_for_dpi_func) {
      return get_metric_for_dpi_func(metric,
                                     GetDPIFromScalingFactor(scale_factor));
    }
  }

  // Fallback for when we're running Windows 8.1, which doesn't support
  // GetSystemMetricsForDpi and yet does support per-process dpi awareness.
  Display primary_display(g_screen_win_instance->GetPrimaryDisplay());
  int system_metrics_result = g_screen_win_instance->GetSystemMetrics(metric);

  return static_cast<int>(std::round(scale_factor * system_metrics_result /
                                     primary_display.device_scale_factor()));
}

// static
gfx::PointF ScreenWin::ScreenToDIPPoint(const gfx::PointF& pixel_point) {
  const ScreenWinDisplay screen_win_display =
      GetScreenWinDisplayVia(&ScreenWin::GetScreenWinDisplayNearestScreenPoint,
                             gfx::ToFlooredPoint(pixel_point));
  const Display display = screen_win_display.display();
  return ScalePointRelative(screen_win_display.pixel_bounds().origin(),
                            display.bounds().origin(),
                            1.0f / display.device_scale_factor(), pixel_point);
}

// static
gfx::Point ScreenWin::DIPToScreenPoint(const gfx::Point& dip_point) {
  const ScreenWinDisplay screen_win_display =
      GetScreenWinDisplayVia(&ScreenWin::GetScreenWinDisplayNearestDIPPoint,
                             dip_point);
  const Display display = screen_win_display.display();
  return gfx::ToFlooredPoint(ScalePointRelative(
      display.bounds().origin(), screen_win_display.pixel_bounds().origin(),
      display.device_scale_factor(), gfx::PointF(dip_point)));
}

// static
gfx::Point ScreenWin::ClientToDIPPoint(HWND hwnd,
                                       const gfx::Point& client_point) {
  return ScaleToFlooredPoint(client_point, 1.0f / GetScaleFactorForHWND(hwnd));
}

// static
gfx::Point ScreenWin::DIPToClientPoint(HWND hwnd, const gfx::Point& dip_point) {
  float scale_factor = GetScaleFactorForHWND(hwnd);
  return ScaleToFlooredPoint(dip_point, scale_factor);
}

// static
gfx::Rect ScreenWin::ScreenToDIPRect(HWND hwnd, const gfx::Rect& pixel_bounds) {
  const ScreenWinDisplay screen_win_display = hwnd
      ? GetScreenWinDisplayVia(&ScreenWin::GetScreenWinDisplayNearestHWND, hwnd)
      : GetScreenWinDisplayVia(
            &ScreenWin::GetScreenWinDisplayNearestScreenRect, pixel_bounds);
  float scale_factor = screen_win_display.display().device_scale_factor();
  gfx::Rect dip_rect = ScaleToEnclosingRect(pixel_bounds, 1.0f / scale_factor);
  const Display display = screen_win_display.display();
  dip_rect.set_origin(gfx::ToFlooredPoint(ScalePointRelative(
      screen_win_display.pixel_bounds().origin(), display.bounds().origin(),
      1.0f / scale_factor, gfx::PointF(pixel_bounds.origin()))));
  return dip_rect;
}

// static
gfx::Rect ScreenWin::DIPToScreenRect(HWND hwnd, const gfx::Rect& dip_bounds) {
  const ScreenWinDisplay screen_win_display = hwnd
      ? GetScreenWinDisplayVia(&ScreenWin::GetScreenWinDisplayNearestHWND, hwnd)
      : GetScreenWinDisplayVia(
            &ScreenWin::GetScreenWinDisplayNearestDIPRect, dip_bounds);
  float scale_factor = screen_win_display.display().device_scale_factor();
  gfx::Rect screen_rect = ScaleToEnclosingRect(dip_bounds, scale_factor);
  const Display display = screen_win_display.display();
  screen_rect.set_origin(gfx::ToFlooredPoint(ScalePointRelative(
      display.bounds().origin(), screen_win_display.pixel_bounds().origin(),
      scale_factor, gfx::PointF(dip_bounds.origin()))));
  return screen_rect;
}

// static
gfx::Rect ScreenWin::ClientToDIPRect(HWND hwnd, const gfx::Rect& pixel_bounds) {
  return ScaleToEnclosingRect(pixel_bounds, 1.0f / GetScaleFactorForHWND(hwnd));
}

// static
gfx::Rect ScreenWin::DIPToClientRect(HWND hwnd, const gfx::Rect& dip_bounds) {
  return ScaleToEnclosingRect(dip_bounds, GetScaleFactorForHWND(hwnd));
}

// static
gfx::Size ScreenWin::ScreenToDIPSize(HWND hwnd,
                                     const gfx::Size& size_in_pixels) {
  // Always ceil sizes. Otherwise we may be leaving off part of the bounds.
  return ScaleToCeiledSize(size_in_pixels, 1.0f / GetScaleFactorForHWND(hwnd));
}

// static
gfx::Size ScreenWin::DIPToScreenSize(HWND hwnd, const gfx::Size& dip_size) {
  float scale_factor = GetScaleFactorForHWND(hwnd);
  // Always ceil sizes. Otherwise we may be leaving off part of the bounds.
  return ScaleToCeiledSize(dip_size, scale_factor);
}

// static
int ScreenWin::GetSystemMetricsForMonitor(HMONITOR monitor, int metric) {
  if (!g_screen_win_instance)
    return ::GetSystemMetrics(metric);

  // We don't include fudge factors stemming from accessiblility features when
  // dealing with system metrics associated with window elements drawn by the
  // operating system, since we will not be doing scaling of those metrics
  // ourselves.
  bool include_accessibility;
  switch (metric) {
    case SM_CXSIZEFRAME:
    case SM_CYSIZEFRAME:
    case SM_CXPADDEDBORDER:
      include_accessibility = false;
      break;
    default:
      include_accessibility = true;
      break;
  }

  // We'll want to use GetSafeMonitorScaleFactor(), so if the monitor is not
  // specified pull up the primary display's HMONITOR.
  if (!monitor)
    monitor = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);

  float scale_factor = GetMonitorScaleFactor(monitor, include_accessibility);

  // We'll then pull up the system metrics scaled by the appropriate amount.
  return GetSystemMetricsForScaleFactor(scale_factor, metric);
}

// static
int ScreenWin::GetSystemMetricsInDIP(int metric) {
  if (!g_screen_win_instance)
    return ::GetSystemMetrics(metric);

  return GetSystemMetricsForScaleFactor(1.0f, metric);
}

// static
float ScreenWin::GetScaleFactorForHWND(HWND hwnd) {
  if (!g_screen_win_instance)
    return ScreenWinDisplay().display().device_scale_factor();

  DCHECK(hwnd);
  HWND rootHwnd = g_screen_win_instance->GetRootWindow(hwnd);
  ScreenWinDisplay screen_win_display =
      g_screen_win_instance->GetScreenWinDisplayNearestHWND(rootHwnd);
  return screen_win_display.display().device_scale_factor();
}

// static
gfx::Vector2dF ScreenWin::GetPixelsPerInch(const gfx::PointF& point) {
  ScreenWinDisplay screen_win_display =
      GetScreenWinDisplayVia(&ScreenWin::GetScreenWinDisplayNearestDIPPoint,
                             gfx::ToFlooredPoint(point));
  return screen_win_display.pixels_per_inch();
}

// static
int ScreenWin::GetDPIForHWND(HWND hwnd) {
  if (Display::HasForceDeviceScaleFactor())
    return GetDPIFromScalingFactor(Display::GetForcedDeviceScaleFactor());

  HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  int dpi = GetPerMonitorDPI(monitor);
  return dpi ? dpi : display::win::internal::GetDefaultSystemDPI();
}

// static
float ScreenWin::GetScaleFactorForDPI(int dpi) {
  return display::win::internal::GetScalingFactorFromDPI(dpi) *
         UwpTextScaleFactor::Instance()->GetTextScaleFactor();
}

// static
float ScreenWin::GetSystemScaleFactor() {
  return display::win::internal::GetUnforcedDeviceScaleFactor();
}

// static
void ScreenWin::SetRequestHDRStatusCallback(
    RequestHDRStatusCallback request_hdr_status_callback) {
  if (!g_screen_win_instance)
    return;
  g_screen_win_instance->request_hdr_status_callback_ =
      std::move(request_hdr_status_callback);
  g_screen_win_instance->request_hdr_status_callback_.Run();
}

// static
void ScreenWin::SetHDREnabled(bool hdr_enabled) {
  if (!g_screen_win_instance)
    return;

  if (g_screen_win_instance->hdr_enabled_ == hdr_enabled)
    return;
  g_screen_win_instance->hdr_enabled_ = hdr_enabled;
  g_screen_win_instance->UpdateAllDisplaysAndNotify();
}

HWND ScreenWin::GetHWNDFromNativeView(gfx::NativeView window) const {
  NOTREACHED();
  return nullptr;
}

gfx::NativeWindow ScreenWin::GetNativeWindowFromHWND(HWND hwnd) const {
  NOTREACHED();
  return nullptr;
}

void ScreenWin::OnUwpTextScaleFactorChanged() {
  UpdateAllDisplaysAndNotify();
}

void ScreenWin::OnUwpTextScaleFactorCleanup(UwpTextScaleFactor* source) {
  if (source == uwp_text_scale_factor_)
    uwp_text_scale_factor_ = nullptr;

  UwpTextScaleFactor::Observer::OnUwpTextScaleFactorCleanup(source);
}

gfx::Point ScreenWin::GetCursorScreenPoint() {
  POINT pt;
  ::GetCursorPos(&pt);
  gfx::PointF cursor_pos_pixels(pt.x, pt.y);
  return gfx::ToFlooredPoint(ScreenToDIPPoint(cursor_pos_pixels));
}

bool ScreenWin::IsWindowUnderCursor(gfx::NativeWindow window) {
  POINT cursor_loc;
  HWND hwnd =
      ::GetCursorPos(&cursor_loc) ? ::WindowFromPoint(cursor_loc) : nullptr;
  return GetNativeWindowFromHWND(hwnd) == window;
}

gfx::NativeWindow ScreenWin::GetWindowAtScreenPoint(const gfx::Point& point) {
  gfx::Point point_in_pixels = DIPToScreenPoint(point);
  return GetNativeWindowFromHWND(WindowFromPoint(point_in_pixels.ToPOINT()));
}

int ScreenWin::GetNumDisplays() const {
  return static_cast<int>(screen_win_displays_.size());
}

const std::vector<Display>& ScreenWin::GetAllDisplays() const {
  return displays_;
}

Display ScreenWin::GetDisplayNearestWindow(gfx::NativeWindow window) const {
  if (!window)
    return GetPrimaryDisplay();
  HWND window_hwnd = GetHWNDFromNativeView(window);
  if (!window_hwnd) {
    // When |window| isn't rooted to a display, we should just return the
    // default display so we get some correct display information like the
    // scaling factor.
    return GetPrimaryDisplay();
  }
  ScreenWinDisplay screen_win_display =
      GetScreenWinDisplayNearestHWND(window_hwnd);
  return screen_win_display.display();
}

Display ScreenWin::GetDisplayNearestPoint(const gfx::Point& point) const {
  gfx::Point screen_point(DIPToScreenPoint(point));
  ScreenWinDisplay screen_win_display =
      GetScreenWinDisplayNearestScreenPoint(screen_point);
  return screen_win_display.display();
}

Display ScreenWin::GetDisplayMatching(const gfx::Rect& match_rect) const {
  ScreenWinDisplay screen_win_display =
      GetScreenWinDisplayNearestScreenRect(match_rect);
  return screen_win_display.display();
}

Display ScreenWin::GetPrimaryDisplay() const {
  return GetPrimaryScreenWinDisplay().display();
}

void ScreenWin::AddObserver(DisplayObserver* observer) {
  change_notifier_.AddObserver(observer);
}

void ScreenWin::RemoveObserver(DisplayObserver* observer) {
  change_notifier_.RemoveObserver(observer);
}

gfx::Rect ScreenWin::ScreenToDIPRectInWindow(
    gfx::NativeView view, const gfx::Rect& screen_rect) const {
  HWND hwnd = view ? GetHWNDFromNativeView(view) : nullptr;
  return ScreenToDIPRect(hwnd, screen_rect);
}

gfx::Rect ScreenWin::DIPToScreenRectInWindow(gfx::NativeView view,
                                             const gfx::Rect& dip_rect) const {
  HWND hwnd = view ? GetHWNDFromNativeView(view) : nullptr;
  return DIPToScreenRect(hwnd, dip_rect);
}

void ScreenWin::UpdateFromDisplayInfos(
    const std::vector<DisplayInfo>& display_infos) {
  screen_win_displays_ = DisplayInfosToScreenWinDisplays(
      display_infos, color_profile_reader_.get(), hdr_enabled_);
  displays_ = ScreenWinDisplaysToDisplays(screen_win_displays_);
}

void ScreenWin::Initialize() {
  color_profile_reader_->UpdateIfNeeded();
  singleton_hwnd_observer_.reset(new gfx::SingletonHwndObserver(
      base::BindRepeating(&ScreenWin::OnWndProc, base::Unretained(this))));
  UpdateFromDisplayInfos(GetDisplayInfosFromSystem());
  RecordDisplayScaleFactors();

  // We want to remember that we've observed a screen metrics object so that we
  // can remove ourselves as an observer at some later point (either when the
  // metrics object notifies us it's going away or when we are destructed).
  uwp_text_scale_factor_ = UwpTextScaleFactor::Instance();
  uwp_text_scale_factor_->AddObserver(this);
}

MONITORINFOEX ScreenWin::MonitorInfoFromScreenPoint(
    const gfx::Point& screen_point) const {
  POINT initial_loc = { screen_point.x(), screen_point.y() };
  return MonitorInfoFromHMONITOR(::MonitorFromPoint(initial_loc,
                                                    MONITOR_DEFAULTTONEAREST));
}

MONITORINFOEX ScreenWin::MonitorInfoFromScreenRect(const gfx::Rect& screen_rect)
    const {
  RECT win_rect = screen_rect.ToRECT();
  return MonitorInfoFromHMONITOR(::MonitorFromRect(&win_rect,
                                                   MONITOR_DEFAULTTONEAREST));
}

MONITORINFOEX ScreenWin::MonitorInfoFromWindow(HWND hwnd,
                                               DWORD default_options) const {
  return MonitorInfoFromHMONITOR(::MonitorFromWindow(hwnd, default_options));
}

HWND ScreenWin::GetRootWindow(HWND hwnd) const {
  return ::GetAncestor(hwnd, GA_ROOT);
}

int ScreenWin::GetSystemMetrics(int metric) const {
  return ::GetSystemMetrics(metric);
}

void ScreenWin::OnWndProc(HWND hwnd,
                          UINT message,
                          WPARAM wparam,
                          LPARAM lparam) {
  if (message != WM_DISPLAYCHANGE &&
      !(message == WM_ACTIVATEAPP && wparam == TRUE) &&
      !(message == WM_SETTINGCHANGE && wparam == SPI_SETWORKAREA)) {
    return;
  }

  color_profile_reader_->UpdateIfNeeded();
  if (request_hdr_status_callback_)
    request_hdr_status_callback_.Run();
  UpdateAllDisplaysAndNotify();
}

void ScreenWin::OnColorProfilesChanged() {
  // The color profile reader will often just confirm that our guess that the
  // color profile was sRGB was indeed correct. Avoid doing an update in these
  // cases.
  bool changed = false;
  for (const auto& display : displays_) {
    if (display.color_space() !=
        color_profile_reader_->GetDisplayColorSpace(display.id())) {
      changed = true;
      break;
    }
  }
  if (!changed)
    return;

  UpdateAllDisplaysAndNotify();
}

void ScreenWin::UpdateAllDisplaysAndNotify() {
  std::vector<Display> old_displays = std::move(displays_);
  UpdateFromDisplayInfos(GetDisplayInfosFromSystem());
  change_notifier_.NotifyDisplaysChanged(old_displays, displays_);
}

ScreenWinDisplay ScreenWin::GetScreenWinDisplayNearestHWND(HWND hwnd)
    const {
  return GetScreenWinDisplay(MonitorInfoFromWindow(hwnd,
                                                   MONITOR_DEFAULTTONEAREST));
}

ScreenWinDisplay ScreenWin::GetScreenWinDisplayNearestScreenRect(
    const gfx::Rect& screen_rect) const {
  return GetScreenWinDisplay(MonitorInfoFromScreenRect(screen_rect));
}

ScreenWinDisplay ScreenWin::GetScreenWinDisplayNearestScreenPoint(
    const gfx::Point& screen_point) const {
  return GetScreenWinDisplay(MonitorInfoFromScreenPoint(screen_point));
}

ScreenWinDisplay ScreenWin::GetScreenWinDisplayNearestDIPPoint(
    const gfx::Point& dip_point) const {
  ScreenWinDisplay primary_screen_win_display;
  for (const auto& screen_win_display : screen_win_displays_) {
    Display display = screen_win_display.display();
    const gfx::Rect dip_bounds = display.bounds();
    if (dip_bounds.Contains(dip_point))
      return screen_win_display;
    else if (dip_bounds.origin().IsOrigin())
      primary_screen_win_display = screen_win_display;
  }
  return primary_screen_win_display;
}

ScreenWinDisplay ScreenWin::GetScreenWinDisplayNearestDIPRect(
    const gfx::Rect& dip_rect) const {
  ScreenWinDisplay closest_screen_win_display;
  int64_t closest_distance_squared = INT64_MAX;
  for (const auto& screen_win_display : screen_win_displays_) {
    Display display = screen_win_display.display();
    gfx::Rect dip_bounds = display.bounds();
    if (dip_rect.Intersects(dip_bounds))
      return screen_win_display;
    int64_t distance_squared = SquaredDistanceBetweenRects(dip_rect,
                                                           dip_bounds);
    if (distance_squared < closest_distance_squared) {
      closest_distance_squared = distance_squared;
      closest_screen_win_display = screen_win_display;
    }
  }
  return closest_screen_win_display;
}

ScreenWinDisplay ScreenWin::GetPrimaryScreenWinDisplay() const {
  MONITORINFOEX monitor_info = MonitorInfoFromWindow(nullptr,
                                                     MONITOR_DEFAULTTOPRIMARY);
  ScreenWinDisplay screen_win_display = GetScreenWinDisplay(monitor_info);
  Display display = screen_win_display.display();
  // The Windows primary monitor is defined to have an origin of (0, 0).
  DCHECK_EQ(0, display.bounds().origin().x());
  DCHECK_EQ(0, display.bounds().origin().y());
  return screen_win_display;
}

ScreenWinDisplay ScreenWin::GetScreenWinDisplay(
    const MONITORINFOEX& monitor_info) const {
  int64_t id = DisplayInfo::DeviceIdFromDeviceName(monitor_info.szDevice);
  for (const auto& screen_win_display : screen_win_displays_) {
    if (screen_win_display.display().id() == id)
      return screen_win_display;
  }
  // There is 1:1 correspondence between MONITORINFOEX and ScreenWinDisplay.
  // If we make it here, it means we have no displays and we should hand out the
  // default display. [Sometimes we get here anyway: crbug.com/768845]
  // DCHECK_EQ(screen_win_displays_.size(), 0u);
  return ScreenWinDisplay();
}

// static
template <typename Getter, typename GetterType>
ScreenWinDisplay ScreenWin::GetScreenWinDisplayVia(Getter getter,
                                                   GetterType value) {
  if (!g_screen_win_instance)
    return ScreenWinDisplay();

  return (g_screen_win_instance->*getter)(value);
}

void ScreenWin::RecordDisplayScaleFactors() const {
  std::vector<int> unique_scale_factors;
  for (const auto& screen_win_display : screen_win_displays_) {
    const float scale_factor =
        screen_win_display.display().device_scale_factor();
    // Multiply the reported value by 100 to display it as a percentage. Clamp
    // it so that if it's wildly out-of-band we won't send it to the backend.
    const int reported_scale = std::min(
        std::max(base::checked_cast<int>(scale_factor * 100), 0), 1000);
    if (!base::Contains(unique_scale_factors, reported_scale)) {
      unique_scale_factors.push_back(reported_scale);
      base::UmaHistogramSparse("UI.DeviceScale", reported_scale);
    }
  }
}

}  // namespace win
}  // namespace display
