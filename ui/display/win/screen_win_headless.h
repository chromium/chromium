// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_WIN_SCREEN_WIN_HEADLESS_H_
#define UI_DISPLAY_WIN_SCREEN_WIN_HEADLESS_H_

#include <windows.h>

#include <optional>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace headless {
struct HeadlessScreenInfo;
}

namespace display::win {

class DISPLAY_EXPORT ScreenWinHeadless : public ScreenWin {
 public:
  explicit ScreenWinHeadless(
      const std::vector<headless::HeadlessScreenInfo>& screen_infos);

  ScreenWinHeadless(const ScreenWinHeadless&) = delete;
  ScreenWinHeadless& operator=(const ScreenWinHeadless&) = delete;

  ~ScreenWinHeadless() override;

  // Screen:
  gfx::Point GetCursorScreenPoint() override;
  void SetCursorScreenPointForTesting(const gfx::Point& point) override;
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
  bool IsHeadless() const override;

  // ScreenWin:
  std::optional<MONITORINFOEX> MonitorInfoFromScreenPoint(
      const gfx::Point& screen_point) const override;
  std::optional<MONITORINFOEX> MonitorInfoFromScreenRect(
      const gfx::Rect& screen_rect) const override;
  std::optional<MONITORINFOEX> MonitorInfoFromWindow(
      HWND hwnd,
      DWORD default_options) const override;
  int64_t GetDisplayIdFromMonitorInfo(
      const MONITORINFOEX& monitor_info) const override;
  HWND GetRootWindow(HWND hwnd) const override;
  void UpdateAllDisplaysAndNotify() override;
  void UpdateAllDisplaysIfPrimaryMonitorChanged() override;

  ScreenWinDisplay GetScreenWinDisplayNearestHWND(HWND hwnd) const override;
  ScreenWinDisplay GetPrimaryScreenWinDisplay() const override;
  ScreenWinDisplay GetScreenWinDisplay(
      std::optional<MONITORINFOEX> monitor_info) const override;

  // ColorProfileReader::Client:
  void OnColorProfilesChanged() override;

  std::optional<MONITORINFOEX> GetMONITORINFOFromDisplayIdForTest(
      int64_t id) const;

 protected:
  // These are exposed for \\ui\views.
  virtual std::vector<gfx::NativeWindow> GetNativeWindowsAtScreenPoint(
      const gfx::Point& point) const;
  virtual gfx::Rect GetNativeWindowBoundsInScreen(
      gfx::NativeWindow window) const;
  virtual gfx::Rect GetHeadlessWindowBounds(
      gfx::AcceleratedWidget window) const;

 private:
  std::vector<internal::DisplayInfo> DisplayInfosFromScreenInfo(
      const std::vector<headless::HeadlessScreenInfo>& screen_infos);

  Display GetDisplayFromScreenPoint(const gfx::Point& point) const;
  Display GetDisplayFromScreenRect(const gfx::Rect& rect) const;

  std::optional<MONITORINFOEX> GetMONITORINFOFromDisplayId(int64_t id) const;

  // Maps display id to a fake Windows monitor info that correlates to
  // a headless display.
  base::flat_map<int64_t, MONITORINFOEX> headless_monitor_info_;

  gfx::Point cursor_screen_point_;
};

namespace internal {
// Exposed for internal::DisplayInfo::ctor check only!
bool VerifyHeadlessDisplayDeviceName(int64_t id,
                                     const MONITORINFOEX& monitor_info);
}  // namespace internal

}  // namespace display::win

#endif  // UI_DISPLAY_WIN_SCREEN_WIN_HEADLESS_H_
