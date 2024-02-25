// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_UTIL_H_
#define UI_BASE_X_X11_UTIL_H_

// This file declares utility functions for X11 (Linux only).

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "ui/base/x/x11_cursor.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/xproto.h"

class SkPixmap;

namespace gfx {
class Point;
}  // namespace gfx

namespace ui {

COMPONENT_EXPORT(UI_BASE_X)
size_t RowBytesForVisualWidth(const x11::Connection::VisualInfo& visual_info,
                              int width);

// Draws an SkPixmap on |drawable| using the given |gc|, converting to the
// server side visual as needed.
COMPONENT_EXPORT(UI_BASE_X)
void DrawPixmap(x11::Connection* connection,
                x11::VisualId visual,
                x11::Drawable drawable,
                x11::GraphicsContext gc,
                const SkPixmap& skia_pixmap,
                int src_x,
                int src_y,
                int dst_x,
                int dst_y,
                int width,
                int height);

// These functions cache their results ---------------------------------

// Returns true if the system supports XINPUT2.
COMPONENT_EXPORT(UI_BASE_X) bool IsXInput2Available();

// Return true iff the display supports MIT-SHM.
COMPONENT_EXPORT(UI_BASE_X) bool QueryShmSupport();

// Coalesce all pending motion events (touch or mouse) that are at the top of
// the queue, and return the number eliminated, storing the last one in
// |last_event|.
COMPONENT_EXPORT(UI_BASE_X)
int CoalescePendingMotionEvents(const x11::Event& xev, x11::Event* last_event);

// Sets whether |window| should use the OS window frame.
COMPONENT_EXPORT(UI_BASE_X)
void SetUseOSWindowFrame(x11::Window window, bool use_os_window_frame);

// These functions do not cache their results --------------------------

// Returns true if the shape extension is supported.
COMPONENT_EXPORT(UI_BASE_X) bool IsShapeExtensionAvailable();

// Get the X window id for the default root window
COMPONENT_EXPORT(UI_BASE_X) x11::Window GetX11RootWindow();

// Returns the user's current desktop.
COMPONENT_EXPORT(UI_BASE_X) bool GetCurrentDesktop(int32_t* desktop);

enum HideTitlebarWhenMaximized : uint32_t {
  SHOW_TITLEBAR_WHEN_MAXIMIZED = 0,
  HIDE_TITLEBAR_WHEN_MAXIMIZED = 1,
};
// Sets _GTK_HIDE_TITLEBAR_WHEN_MAXIMIZED on |window|.
COMPONENT_EXPORT(UI_BASE_X)
void SetHideTitlebarWhenMaximizedProperty(x11::Window window,
                                          HideTitlebarWhenMaximized property);

// Returns the raw bytes from a property with minimal
// interpretation. |out_data| should be freed by XFree() after use.
COMPONENT_EXPORT(UI_BASE_X)
bool GetRawBytesOfProperty(x11::Window window,
                           x11::Atom property,
                           scoped_refptr<base::RefCountedMemory>* out_data,
                           x11::Atom* out_type);

// Sets the WM_CLASS attribute for a given X11 window.
COMPONENT_EXPORT(UI_BASE_X)
void SetWindowClassHint(x11::Connection* connection,
                        x11::Window window,
                        const std::string& res_name,
                        const std::string& res_class);

// Sets the WM_WINDOW_ROLE attribute for a given X11 window.
COMPONENT_EXPORT(UI_BASE_X)
void SetWindowRole(x11::Window window, const std::string& role);

// Sends a message to the x11 window manager, enabling or disabling the
// states |state1| and |state2|.
COMPONENT_EXPORT(UI_BASE_X)
void SetWMSpecState(x11::Window window,
                    bool enabled,
                    x11::Atom state1,
                    x11::Atom state2);

// Sends a NET_WM_MOVERESIZE message to the x11 window manager, enabling the
// move/resize mode.  As per NET_WM_MOVERESIZE spec, |location| is the position
// in pixels (relative to the root window) of mouse button press, and
// |direction| indicates whether this is a move or resize event, and if it is a
// resize event, which edges of the window the size grip applies to.
COMPONENT_EXPORT(UI_BASE_X)
void DoWMMoveResize(x11::Connection* connection,
                    x11::Window root_window,
                    x11::Window window,
                    const gfx::Point& location_px,
                    int direction);

// Checks if the window manager has set a specific state.
COMPONENT_EXPORT(UI_BASE_X)
bool HasWMSpecProperty(const base::flat_set<x11::Atom>& properties,
                       x11::Atom atom);

// Determine whether we should default to native decorations or the custom
// frame based on the currently-running window manager.
COMPONENT_EXPORT(UI_BASE_X) bool GetCustomFramePrefDefault();

static const int32_t kAllDesktops = -1;
// Queries the desktop |window| is on, kAllDesktops if sticky. Returns false if
// property not found.
COMPONENT_EXPORT(UI_BASE_X)
bool GetWindowDesktop(x11::Window window, int32_t* desktop);

enum WindowManagerName {
  WM_OTHER,    // We were able to obtain the WM's name, but there is
               // no corresponding entry in this enum.
  WM_UNNAMED,  // Either there is no WM or there is no way to obtain
               // the WM name.

  WM_AWESOME,
  WM_BLACKBOX,
  WM_COMPIZ,
  WM_ENLIGHTENMENT,
  WM_FLUXBOX,
  WM_I3,
  WM_ICE_WM,
  WM_ION3,
  WM_KWIN,
  WM_MATCHBOX,
  WM_METACITY,
  WM_MUFFIN,
  WM_MUTTER,
  WM_NOTION,
  WM_OPENBOX,
  WM_QTILE,
  WM_RATPOISON,
  WM_STUMPWM,
  WM_WMII,
  WM_XFWM4,
  WM_XMONAD,
};
// Attempts to guess the window maager. Returns WM_OTHER or WM_UNNAMED
// if we can't determine it for one reason or another.
COMPONENT_EXPORT(UI_BASE_X) WindowManagerName GuessWindowManager();

// The same as GuessWindowManager(), but returns the raw string.  If we
// can't determine it, return "Unknown".
COMPONENT_EXPORT(UI_BASE_X) std::string GuessWindowManagerName();

// These values are persisted to logs.  Entries should not be renumbered and
// numeric values should never be reused.
//
// Append new window managers before kMaxValue and update LinuxWindowManagerName
// in tools/metrics/histograms/enums.xml accordingly.
//
// See also tools/metrics/histograms/README.md#enum-histograms
enum class UMALinuxWindowManager {
  kOther = 0,
  kBlackbox = 1,
  kChromeOS = 2,  // Deprecated.
  kCompiz = 3,
  kEnlightenment = 4,
  kIceWM = 5,
  kKWin = 6,
  kMetacity = 7,
  kMuffin = 8,
  kMutter = 9,
  kOpenbox = 10,
  kXfwm4 = 11,
  kAwesome = 12,
  kI3 = 13,
  kIon3 = 14,
  kMatchbox = 15,
  kNotion = 16,
  kQtile = 17,
  kRatpoison = 18,
  kStumpWM = 19,
  kWmii = 20,
  kFluxbox = 21,
  kXmonad = 22,
  kUnnamed = 23,
  kMaxValue = kUnnamed
};
COMPONENT_EXPORT(UI_BASE_X) UMALinuxWindowManager GetWindowManagerUMA();

// Returns a buest-effort guess as to whether |window_manager| is tiling (true)
// or stacking (false).
COMPONENT_EXPORT(UI_BASE_X) bool IsWmTiling(WindowManagerName window_manager);

// Returns true if a given window is in full-screen mode.
COMPONENT_EXPORT(UI_BASE_X) bool IsX11WindowFullScreen(x11::Window window);

// Suspends or resumes the X screen saver, and returns whether the operation was
// successful.  Must be called on the UI thread. If called multiple times with
// |suspend| set to true, the screen saver is not un-suspended until this method
// is called an equal number of times with |suspend| set to false.
COMPONENT_EXPORT(UI_BASE_X) bool SuspendX11ScreenSaver(bool suspend);

// Returns the preferred Skia colortype for an X11 visual.  Returns
// kUnknown_SkColorType if there isn't a suitable colortype.
COMPONENT_EXPORT(UI_BASE_X)
SkColorType ColorTypeForVisual(x11::VisualId visual_id);

COMPONENT_EXPORT(UI_BASE_X)
x11::Future<void> SendClientMessage(
    x11::Window window,
    x11::Window target,
    x11::Atom type,
    const std::array<uint32_t, 5> data,
    x11::EventMask event_mask = x11::EventMask::SubstructureNotify |
                                x11::EventMask::SubstructureRedirect);

// Return true if VulkanSurface is supported.
COMPONENT_EXPORT(UI_BASE_X) bool IsVulkanSurfaceSupported();

// Returns an icon for a native window referred by |target_window_id|. Can be
// any window on screen.
COMPONENT_EXPORT(UI_BASE_X)
gfx::ImageSkia GetNativeWindowIcon(intptr_t target_window_id);

}  // namespace ui

#endif  // UI_BASE_X_X11_UTIL_H_
