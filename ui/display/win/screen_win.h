// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_WIN_SCREEN_WIN_H_
#define UI_DISPLAY_WIN_SCREEN_WIN_H_

#include <windows.h>

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/scoped_observation.h"
#include "ui/display/display_change_notifier.h"
#include "ui/display/display_export.h"
#include "ui/display/screen.h"
#include "ui/display/win/color_profile_reader.h"
#include "ui/display/win/screen_win_display.h"
#include "ui/display/win/uwp_text_scale_factor.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/mojom/dxgi_info.mojom.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/gfx/win/singleton_hwnd.h"

namespace display::win {

class ScreenWinDisplay;
class FallbackScreenWin;

namespace internal {
class DisplayInfo;
}  // namespace internal

class DISPLAY_EXPORT ScreenWin : public Screen,
                                 public ColorProfileReader::Client,
                                 public UwpTextScaleFactor::Observer {
 public:
  ScreenWin(const ScreenWin&) = delete;
  ScreenWin& operator=(const ScreenWin&) = delete;

  ~ScreenWin() override;

  // Converts a screen physical point to a screen DIP point.
  // The DPI scale is performed relative to the display containing the physical
  // point.
  virtual gfx::PointF ScreenToDIPPoint(const gfx::PointF& pixel_point) const;

  // Converts a screen DIP point to a screen physical point.
  // The DPI scale is performed relative to the display containing the DIP
  // point.
  virtual gfx::Point DIPToScreenPoint(const gfx::Point& dip_point) const;

  // Converts a client physical point relative to |hwnd| to a client DIP point.
  // The DPI scale is performed relative to |hwnd| using an origin of (0, 0).
  virtual gfx::Point ClientToDIPPoint(HWND hwnd,
                                      const gfx::Point& client_point) const;

  // Converts a client DIP point relative to |hwnd| to a client physical point.
  // The DPI scale is performed relative to |hwnd| using an origin of (0, 0).
  virtual gfx::Point DIPToClientPoint(HWND hwnd,
                                      const gfx::Point& dip_point) const;

  // WARNING: There is no right way to scale sizes and rects.
  // Sometimes you may need the enclosing rect (which favors transformations
  // that stretch the bounds towards integral values) or the enclosed rect
  // (transformations that shrink the bounds towards integral values).
  // This implementation favors the enclosing rect.
  //
  // Understand which you need before blindly assuming this is the right way.

  // Converts a screen physical rect to a screen DIP rect.
  // The DPI scale is performed relative to the display nearest to |hwnd|.
  // If |hwnd| is null, scaling will be performed to the display nearest to
  // |pixel_bounds|.
  virtual gfx::Rect ScreenToDIPRect(HWND hwnd,
                                    const gfx::Rect& pixel_bounds) const;

  // Converts a screen DIP rect to a screen physical rect.
  // If |hwnd| is null, scaling will be performed using the DSF of the display
  // nearest to |dip_bounds|; otherwise, scaling will be performed using the DSF
  // of the display nearest to |hwnd|.  Thus if an existing HWND is moving to a
  // different display, it's often more correct to pass null for |hwnd| to get
  // the new display's scale factor rather than the old one's.
  virtual gfx::Rect DIPToScreenRect(HWND hwnd,
                                    const gfx::Rect& dip_bounds) const;

  // Converts a client physical rect to a client DIP rect.
  // The DPI scale is performed relative to |hwnd| using an origin of (0, 0).
  virtual gfx::Rect ClientToDIPRect(HWND hwnd,
                                    const gfx::Rect& pixel_bounds) const;

  // Converts a client DIP rect to a client physical rect.
  // The DPI scale is performed relative to |hwnd| using an origin of (0, 0).
  virtual gfx::Rect DIPToClientRect(HWND hwnd,
                                    const gfx::Rect& dip_bounds) const;

  // Converts a physical size to a DIP size.
  // The DPI scale is performed relative to the display nearest to |hwnd|.
  virtual gfx::Size ScreenToDIPSize(HWND hwnd,
                                    const gfx::Size& size_in_pixels) const;

  // Converts a DIP size to a physical size.
  // The DPI scale is performed relative to the display nearest to |hwnd|.
  virtual gfx::Size DIPToScreenSize(HWND hwnd, const gfx::Size& dip_size) const;

  // Returns the number of physical pixels per inch for a display associated
  // with the point.
  virtual gfx::Vector2dF GetPixelsPerInch(const gfx::PointF& point) const;

  // Returns the result of GetSystemMetrics for |metric| scaled to |monitor|'s
  // DPI. Use this function if you're already working with screen pixels, as
  // this helps reduce any cascading rounding errors from DIP to the |monitor|'s
  // DPI.
  //
  // Note that metrics which correspond to elements drawn by Windows
  // (specifically frame and resize handles) will be scaled by DPI only and not
  // by Text Zoom or other accessibility features.
  virtual int GetSystemMetricsForMonitor(HMONITOR monitor, int metric) const;

  // Returns the result of GetSystemMetrics for |metric| in DIP.
  // Use this function if you need to work in DIP and can tolerate cascading
  // rounding errors towards screen pixels.
  virtual int GetSystemMetricsInDIP(int metric) const;

  // Returns |hwnd|'s scale factor, including accessibility adjustments.
  virtual float GetScaleFactorForHWND(HWND hwnd) const;

  // Returns raw scale factor for monitor excluding accessibility adjustments.
  virtual float GetScaleFactorForMonitor(HMONITOR monitor) const;

  // Returns the unmodified DPI for a particular |hwnd|, without accessibility
  // adjustments.
  virtual int GetDPIForHWND(HWND hwnd) const;

  // Converts dpi to scale factor, including accessibility adjustments.
  virtual float GetScaleFactorForDPI(int dpi) const;

  // Set a callback to use to query the status of HDR. This callback will be
  // called when the status of HDR may have changed.
  using RequestHDRStatusCallback = base::RepeatingClosure;
  virtual void SetRequestHDRStatusCallback(
      RequestHDRStatusCallback request_hdr_status_callback);

  // Set information gathered from DXGI adapters and outputs (e.g, HDR
  // parameters).
  virtual void SetDXGIInfo(gfx::mojom::DXGIInfoPtr dxgi_info);

  // Returns the ScreenWinDisplay with the given id, or a default object if an
  // unrecognized id was specified or if this was called during a screen update.
  virtual ScreenWinDisplay GetScreenWinDisplayWithDisplayId(int64_t id) const;

  // Returns the display id for the given monitor info.
  virtual int64_t DisplayIdFromMonitorInfo(
      const MONITORINFOEX& monitor_info) const;

  // Updates the display infos to make sure they have the right scale factors.
  // This is called before handling WM_DPICHANGED messages, to be sure that we
  // have the right scale factors for the screens.
  virtual void UpdateDisplayInfos();

  // Updates the display infos if it appears that Windows state has changed
  // in a way that requires the display infos to be updated. This currently
  // only detects when the primary monitor changes, which it does when a monitor
  // is added or removed.
  virtual void UpdateDisplayInfosIfNeeded();

  // Returns the HWND associated with the NativeWindow.
  virtual HWND GetHWNDFromNativeWindow(gfx::NativeWindow view) const;

  // Returns the NativeWindow associated with the HWND.
  virtual gfx::NativeWindow GetNativeWindowFromHWND(HWND hwnd) const;

  // Returns true if the native window is occluded.
  virtual bool IsNativeWindowOccluded(gfx::NativeWindow window) const;

  // Returns the cached on_current_workspace() value for the NativeWindow's
  // host.
  virtual std::optional<bool> IsWindowOnCurrentVirtualDesktop(
      gfx::NativeWindow window) const;

  // Resets cached fallback screen for testing. Has no effect if there is no
  // fallback screen. Fallback screen remembers forced device scale factor at
  // the time of creation and thus has to be reset in unit tests running in the
  // same process, similar to Display::ResetForceDeviceScaleFactorForTesting().
  static void ResetFallbackScreenForTesting();

 protected:
  friend class FallbackScreenWin;

  FRIEND_TEST_ALL_PREFIXES(ScreenWinTestSingleDisplay1x,
                           DisconnectPrimaryDisplay);

  ScreenWin();

  // `initialize_from_system` is true if the ScreenWin should be initialized
  // from the Windows desktop environment, e.g., the monitor information and
  // configuration. It is false in unit tests, true in Chrome and browser
  // tests.
  ScreenWin(bool initialize_from_system);

  // Screen:
  gfx::Point GetCursorScreenPoint() override;
  bool IsWindowUnderCursor(gfx::NativeWindow window) override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  gfx::NativeWindow GetLocalProcessWindowAtPoint(
      const gfx::Point& point,
      const std::set<gfx::NativeWindow>& ignore) override;
  int GetNumDisplays() const override;
  const std::vector<Display>& GetAllDisplays() const override;
  Display GetDisplayNearestWindow(gfx::NativeWindow window) const override;
  Display GetDisplayNearestPoint(const gfx::Point& point) const override;
  Display GetDisplayMatching(const gfx::Rect& match_rect) const override;
  Display GetPrimaryDisplay() const override;
  void AddObserver(DisplayObserver* observer) override;
  void RemoveObserver(DisplayObserver* observer) override;
  gfx::Rect ScreenToDIPRectInWindow(
      gfx::NativeWindow window,
      const gfx::Rect& screen_rect) const override;
  gfx::Rect DIPToScreenRectInWindow(gfx::NativeWindow window,
                                    const gfx::Rect& dip_rect) const override;

  // ColorProfileReader::Client:
  void OnColorProfilesChanged() override;

  void UpdateFromDisplayInfos(
      const std::vector<internal::DisplayInfo>& display_infos);

  // Virtual to support mocking by unit tests and headless screen.
  virtual HMONITOR HMONITORFromScreenPoint(
      const gfx::Point& screen_point) const;
  virtual HMONITOR HMONITORFromScreenRect(const gfx::Rect& screen_rect) const;
  virtual HMONITOR HMONITORFromWindow(HWND hwnd, DWORD default_options) const;
  virtual std::optional<MONITORINFOEX> MonitorInfoFromScreenPoint(
      const gfx::Point& screen_point) const;
  virtual std::optional<MONITORINFOEX> MonitorInfoFromScreenRect(
      const gfx::Rect& screen_rect) const;
  virtual std::optional<MONITORINFOEX> MonitorInfoFromWindow(
      HWND hwnd,
      DWORD default_options) const;
  virtual std::optional<MONITORINFOEX> MonitorInfoFromHMONITOR(
      HMONITOR monitor) const;

  virtual int64_t GetDisplayIdFromMonitorInfo(
      const MONITORINFOEX& monitor_info) const;
  virtual HWND GetRootWindow(HWND hwnd) const;
  virtual int GetSystemMetrics(int metric) const;
  virtual void UpdateAllDisplaysAndNotify();
  virtual void UpdateAllDisplaysIfPrimaryMonitorChanged();

  // Returns the ScreenWinDisplay closest to or enclosing |hwnd|.
  virtual ScreenWinDisplay GetScreenWinDisplayNearestHWND(HWND hwnd) const;

  // Returns the ScreenWinDisplay closest to or enclosing |screen_rect|.
  virtual ScreenWinDisplay GetScreenWinDisplayNearestScreenRect(
      const gfx::Rect& screen_rect) const;

  // Returns the ScreenWinDisplay closest to or enclosing |screen_point|.
  virtual ScreenWinDisplay GetScreenWinDisplayNearestScreenPoint(
      const gfx::Point& screen_point) const;

  // Returns the ScreenWinDisplay closest to or enclosing |dip_point|.
  ScreenWinDisplay GetScreenWinDisplayNearestDIPPoint(
      const gfx::Point& dip_point) const;

  // Returns the ScreenWinDisplay closest to or enclosing |dip_rect|.
  ScreenWinDisplay GetScreenWinDisplayNearestDIPRect(
      const gfx::Rect& dip_rect) const;

  // Returns the ScreenWinDisplay corresponding to the primary monitor.
  virtual ScreenWinDisplay GetPrimaryScreenWinDisplay() const;

  // Returns the ScreenWinDisplay corresponding to the given monitor info.
  virtual ScreenWinDisplay GetScreenWinDisplay(
      std::optional<MONITORINFOEX> monitor_info) const;

  // Returns the ScreenWinDisplay for the given `monitor`, first by matching on
  // ScreenWinDisplay::hmonitor(), then by looking up the monitor info if
  // there's no match.
  virtual ScreenWinDisplay GetScreenWinDisplayForHMONITOR(
      HMONITOR monitor) const;

  // Returns the result of GetSystemMetrics for |metric| scaled to the specified
  // |scale_factor|.
  int GetSystemMetricsForScaleFactor(float scale_factor, int metric) const;

 private:
  void Initialize();

  void OnWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  // Returns the result of calling |getter| with |value| on the global
  // ScreenWin if it exists, otherwise return the default ScreenWinDisplay.
  template <typename Getter, typename GetterType>
  static ScreenWinDisplay GetScreenWinDisplayVia(Getter getter,
                                                 GetterType value);

  //-----------------------------------------------------------------
  // UwpTextScaleFactor::Observer:

  void OnUwpTextScaleFactorChanged() override;
  void OnUwpTextScaleFactorCleanup(UwpTextScaleFactor* source) override;

  // Tests don't want to use the actual DPI settings of the monitor(s) on
  // the machine running the test.
  // Returns false if running in unit tests, if the ScreenWin constructor was
  // called with initialize set to false.
  bool PerProcessDPIAwarenessDisabledForTesting() const;

  // Helper implementing the DisplayObserver handling.
  DisplayChangeNotifier change_notifier_;

  base::CallbackListSubscription hwnd_subscription_;

  // Current list of ScreenWinDisplays.
  std::vector<ScreenWinDisplay> screen_win_displays_;

  // The Displays corresponding to |screen_win_displays_| for GetAllDisplays().
  // This must be updated anytime |screen_win_displays_| is updated.
  std::vector<Display> displays_;

  // A helper to read color profiles from the filesystem.
  std::unique_ptr<ColorProfileReader> color_profile_reader_ =
      std::make_unique<ColorProfileReader>(this);

  // Callback to use to query when the HDR status may have changed.
  RequestHDRStatusCallback request_hdr_status_callback_;

  // Information gathered from DXGI adapters and outputs.
  gfx::mojom::DXGIInfoPtr dxgi_info_;

  base::ScopedObservation<UwpTextScaleFactor, UwpTextScaleFactor::Observer>
      scale_factor_observation_{this};

  // Used to avoid calling GetSystemMetricsForDpi in unit tests.
  bool per_process_dpi_awareness_disabled_for_testing_ = false;

  // Used to track if primary_monitor_ changes, which is used as a signal that
  // screen_win_displays_ needs to be updated. This should be updated when
  // screen_win_displays_ is updated.
  HMONITOR primary_monitor_ = nullptr;
};

// Returns a ScreenWin instance. If one does not exist, creates a fallback
// ScreenWin instance that may be replaced with the real one later if necessary.
DISPLAY_EXPORT ScreenWin* GetScreenWin();

}  // namespace display::win

#endif  // UI_DISPLAY_WIN_SCREEN_WIN_H_
