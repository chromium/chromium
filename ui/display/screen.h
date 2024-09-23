// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_SCREEN_H_
#define UI_DISPLAY_SCREEN_H_

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "ui/display/display.h"
#include "ui/display/display_export.h"
#include "ui/display/screen_infos.h"
#include "ui/gfx/gpu_extra_info.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace gfx {
class Point;
class Rect;
}  // namespace gfx

namespace display {
class DisplayObserver;
enum class TabletState;

// A utility class for getting various info about screen size, displays,
// cursor position, etc.
//
// Also, can notify DisplayObservers about global workspace changes. The
// availability of that functionality depends on a platform.
//
// Note that this class does not represent an individual display connected to a
// computer -- see the Display class for that. A single Screen object exists
// regardless of the number of connected displays.
class DISPLAY_EXPORT Screen {
 public:
  Screen();

  Screen(const Screen&) = delete;
  Screen& operator=(const Screen&) = delete;

  virtual ~Screen();

  // Retrieves the single Screen object; this may be null if it's not already
  // created, except for IOS where it creates a native screen instance
  // automatically. On ChromeOS ash the return value is only null on startup.

  static Screen* GetScreen();

  // Returns whether a Screen singleton exists or not.
  static bool HasScreen();

  // [Deprecated] as a public method. Do not use this.
  // Sets the global screen. Returns the previously installed screen, if any.
  // NOTE: this does not take ownership of |screen|. Tests must be sure to reset
  // any state they install.
  static Screen* SetScreenInstance(Screen* instance,
                                   const base::Location& location = FROM_HERE);

  // Returns the current absolute position of the mouse pointer.
  virtual gfx::Point GetCursorScreenPoint() = 0;

  // Allows tests to override the cursor point location on the screen.
  virtual void SetCursorScreenPointForTesting(const gfx::Point& point);

  // Returns true if the cursor is directly over |window|.
  virtual bool IsWindowUnderCursor(gfx::NativeWindow window) = 0;

  // Returns the window at the given screen coordinate |point|.
  virtual gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) = 0;

  // Finds the topmost visible chrome window at |screen_point|. This should
  // return nullptr if |screen_point| is in another program's window which
  // occludes the topmost chrome window. Ignores the windows in |ignore|, which
  // contain windows such as the tab being dragged right now.
  virtual gfx::NativeWindow GetLocalProcessWindowAtPoint(
      const gfx::Point& point,
      const std::set<gfx::NativeWindow>& ignore) = 0;

  // Returns the number of displays.  Mirrored displays are excluded; this
  // method is intended to return the number of distinct, usable displays.
  // The value returned must be at least 1, as GetAllDisplays returns a fake
  // display if there are no displays in the system.
  virtual int GetNumDisplays() const = 0;

  // Returns the list of displays that are currently available.
  // Screen subclasses must return at least one Display, even if it is fake.
  virtual const std::vector<Display>& GetAllDisplays() const = 0;

  // Returns the display nearest the specified window.
  // If the window is NULL or the window is not rooted to a display this will
  // return the primary display.
  //
  // Warning: When determining which scale factor to use for a given native
  // window, use `GetPreferredScaleFactorForWindow` instead, as it properly
  // supports system-controlled per-window scaling, such as Wayland.
  virtual Display GetDisplayNearestWindow(gfx::NativeWindow window) const = 0;

  // Returns the display nearest the specified view. It may still use the window
  // that contains the view (i.e. if a window is spread over two displays,
  // the location of the view within that window won't influence the result).
  //
  // Warning: When determining which scale factor to use for a given native
  // view, use `GetPreferredScaleFactorForView` instead, as it properly
  // supports system-controlled per-window scaling, such as Wayland.
  virtual Display GetDisplayNearestView(gfx::NativeView view) const;

  // Returns the display nearest the specified DIP |point|.
  virtual Display GetDisplayNearestPoint(const gfx::Point& point) const = 0;

  // Returns the display that most closely intersects the DIP rect |match_rect|.
  virtual Display GetDisplayMatching(const gfx::Rect& match_rect) const = 0;

  // Returns the primary display. It is guaranteed that this will return a
  // display with a valid display ID even if there is no display connected.
  // A real display will be reported via DisplayObserver when it is connected.
  virtual Display GetPrimaryDisplay() const = 0;

  // Returns a suggested display to use when creating a new window. On most
  // platforms just returns the primary display.
  Display GetDisplayForNewWindows() const;

  // Sets the suggested display to use when creating a new window.
  virtual void SetDisplayForNewWindows(int64_t display_id);

  // Returns ScreenInfos, attempting to set the current ScreenInfo to the
  // display corresponding to `nearest_id`.  The returned result is guaranteed
  // to be non-empty.  This function also performs fallback to ensure the result
  // also has a valid current ScreenInfo and exactly one primary ScreenInfo
  // (both of which may or may not be `nearest_id`).
  display::ScreenInfos GetScreenInfosNearestDisplay(int64_t nearest_id) const;

#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
  // Object which suspends the platform-specific screensaver for the duration of
  // its existence.
  class ScreenSaverSuspender {
   public:
    ScreenSaverSuspender() = default;

    ScreenSaverSuspender(const ScreenSaverSuspender&) = delete;
    ScreenSaverSuspender& operator=(const ScreenSaverSuspender&) = delete;

    // Causes the platform-specific screensaver to be un-suspended iff this is
    // the last remaining instance.
    virtual ~ScreenSaverSuspender() = 0;
  };

  // Suspends the platform-specific screensaver until the returned
  // |ScreenSaverSuspender| is destructed, or returns nullptr if suspension
  // failed. This method allows stacking multiple overlapping calls, such that
  // the platform-specific screensaver will not be un-suspended until all
  // returned |ScreenSaverSuspender| instances have been destructed.
  virtual std::unique_ptr<ScreenSaverSuspender> SuspendScreenSaver();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)

  // Returns whether the screensaver is currently running.
  virtual bool IsScreenSaverActive() const;

  // Calculates idle time.
  virtual base::TimeDelta CalculateIdleTime() const;

  // Adds/Removes display observers.
  virtual void AddObserver(DisplayObserver* observer) = 0;
  virtual void RemoveObserver(DisplayObserver* observer) = 0;

  // Converts |screen_rect| to DIP coordinates in the context of |window|
  // clamping to the enclosing rect if the coordinates do not fall on pixel
  // boundaries. If |window| is null, the primary display is used as the
  // context.
  virtual gfx::Rect ScreenToDIPRectInWindow(gfx::NativeWindow window,
                                            const gfx::Rect& screen_rect) const;

  // Converts |dip_rect| to screen coordinates in the context of |window|
  // clamping to the enclosing rect if the coordinates do not fall on pixel
  // boundaries. If |window| is null, the primary display is used as the
  // context.
  virtual gfx::Rect DIPToScreenRectInWindow(gfx::NativeWindow window,
                                            const gfx::Rect& dip_rect) const;

  // Returns true if the display with |display_id| is found and returns that
  // display in |display|. Otherwise returns false and |display| remains
  // untouched.
  bool GetDisplayWithDisplayId(int64_t display_id, Display* display) const;

  virtual void SetPanelRotationForTesting(int64_t display_id,
                                          Display::Rotation rotation);

  // Depending on a platform, a client can listen to global workspace changes
  // by implementing and setting self as a DisplayObserver. It is also possible
  // to get current workspace through the GetCurrentWorkspace method.
  virtual std::string GetCurrentWorkspace();

  // Returns human readable description of the window manager, desktop, and
  // other system properties related to the compositing.
  virtual base::Value::List GetGpuExtraInfo(
      const gfx::GpuExtraInfo& gpu_extra_info);

  // Returns the preferred scale factor for |window|, if the underlying platform
  // supports per-window scaling, otherwise returns the scale factor of display
  // nearst to |window|, using GetDisplayNearest[Window|View].
  virtual std::optional<float> GetPreferredScaleFactorForWindow(
      gfx::NativeWindow window) const;
  virtual std::optional<float> GetPreferredScaleFactorForView(
      gfx::NativeView view) const;

#if BUILDFLAG(IS_CHROMEOS)
  // Returns tablet state.
  virtual TabletState GetTabletState() const;

  // Returns true if the system is in tablet mode.
  bool InTabletMode() const;

  // Overrides tablet state stored in screen and notifies observers only on
  // Lacros side.
  // Not that this method may make tablet state out-of-sync with Ash side.
  virtual void OverrideTabletStateForTesting(TabletState tablet_state) {}
#endif  // BUILDFLAG(IS_CHROMEOS)

 protected:
  void set_shutdown(bool shutdown) { shutdown_ = shutdown; }
  int64_t display_id_for_new_windows() const {
    return display_id_for_new_windows_;
  }

 private:
  friend class ScopedDisplayForNewWindows;

  // Used to temporarily override the value from SetDisplayForNewWindows() by
  // creating an instance of ScopedDisplayForNewWindows. Call with
  // |kInvalidDisplayId| to unset.
  void SetScopedDisplayForNewWindows(int64_t display_id);

  static gfx::NativeWindow GetWindowForView(gfx::NativeView view);

  // A flag indicates that the instance is a special one used during shutdown.
  bool shutdown_ = false;

  int64_t display_id_for_new_windows_;
  int64_t scoped_display_id_for_new_windows_ = display::kInvalidDisplayId;

#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
  uint32_t screen_saver_suspension_count_ = 0;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
};

#if BUILDFLAG(IS_APPLE)

// TODO(oshima): move this to separate apple specific file.

// TODO(crbug.com/40222482): Make this static private member of
// ScopedNativeScreen.
DISPLAY_EXPORT Screen* CreateNativeScreen();

// ScopedNativeScreen creates a native screen if there is no screen created yet
// (e.g. by a unit test).
class DISPLAY_EXPORT ScopedNativeScreen final {
 public:
  explicit ScopedNativeScreen(const base::Location& location = FROM_HERE);
  ScopedNativeScreen(const ScopedNativeScreen&) = delete;
  ScopedNativeScreen& operator=(const ScopedNativeScreen&) = delete;
  ~ScopedNativeScreen();

 private:
  std::unique_ptr<Screen> screen_;
};

#endif

}  // namespace display

#endif  // UI_DISPLAY_SCREEN_H_
