// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/x11_atom_cache.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"

namespace {

struct {
  const char* atom_name;
  Atom atom_value;
} const kPredefinedAtoms[] = {
    // {"PRIMARY", XA_PRIMARY},
    // {"SECONDARY", XA_SECONDARY},
    // {"ARC", XA_ARC},
    {"ATOM", XA_ATOM},
    // {"BITMAP", XA_BITMAP},
    {"CARDINAL", XA_CARDINAL},
    // {"COLORMAP", XA_COLORMAP},
    // {"CURSOR", XA_CURSOR},
    // {"CUT_BUFFER0", XA_CUT_BUFFER0},
    // {"CUT_BUFFER1", XA_CUT_BUFFER1},
    // {"CUT_BUFFER2", XA_CUT_BUFFER2},
    // {"CUT_BUFFER3", XA_CUT_BUFFER3},
    // {"CUT_BUFFER4", XA_CUT_BUFFER4},
    // {"CUT_BUFFER5", XA_CUT_BUFFER5},
    // {"CUT_BUFFER6", XA_CUT_BUFFER6},
    // {"CUT_BUFFER7", XA_CUT_BUFFER7},
    // {"DRAWABLE", XA_DRAWABLE},
    // {"FONT", XA_FONT},
    // {"INTEGER", XA_INTEGER},
    // {"PIXMAP", XA_PIXMAP},
    // {"POINT", XA_POINT},
    // {"RECTANGLE", XA_RECTANGLE},
    // {"RESOURCE_MANAGER", XA_RESOURCE_MANAGER},
    // {"RGB_COLOR_MAP", XA_RGB_COLOR_MAP},
    // {"RGB_BEST_MAP", XA_RGB_BEST_MAP},
    // {"RGB_BLUE_MAP", XA_RGB_BLUE_MAP},
    // {"RGB_DEFAULT_MAP", XA_RGB_DEFAULT_MAP},
    // {"RGB_GRAY_MAP", XA_RGB_GRAY_MAP},
    // {"RGB_GREEN_MAP", XA_RGB_GREEN_MAP},
    // {"RGB_RED_MAP", XA_RGB_RED_MAP},
    {"STRING", XA_STRING},
    // {"VISUALID", XA_VISUALID},
    // {"WINDOW", XA_WINDOW},
    // {"WM_COMMAND", XA_WM_COMMAND},
    // {"WM_HINTS", XA_WM_HINTS},
    // {"WM_CLIENT_MACHINE", XA_WM_CLIENT_MACHINE},
    // {"WM_ICON_NAME", XA_WM_ICON_NAME},
    // {"WM_ICON_SIZE", XA_WM_ICON_SIZE},
    // {"WM_NAME", XA_WM_NAME},
    // {"WM_NORMAL_HINTS", XA_WM_NORMAL_HINTS},
    // {"WM_SIZE_HINTS", XA_WM_SIZE_HINTS},
    // {"WM_ZOOM_HINTS", XA_WM_ZOOM_HINTS},
    // {"MIN_SPACE", XA_MIN_SPACE},
    // {"NORM_SPACE", XA_NORM_SPACE},
    // {"MAX_SPACE", XA_MAX_SPACE},
    // {"END_SPACE", XA_END_SPACE},
    // {"SUPERSCRIPT_X", XA_SUPERSCRIPT_X},
    // {"SUPERSCRIPT_Y", XA_SUPERSCRIPT_Y},
    // {"SUBSCRIPT_X", XA_SUBSCRIPT_X},
    // {"SUBSCRIPT_Y", XA_SUBSCRIPT_Y},
    // {"UNDERLINE_POSITION", XA_UNDERLINE_POSITION},
    // {"UNDERLINE_THICKNESS", XA_UNDERLINE_THICKNESS},
    // {"STRIKEOUT_ASCENT", XA_STRIKEOUT_ASCENT},
    // {"STRIKEOUT_DESCENT", XA_STRIKEOUT_DESCENT},
    // {"ITALIC_ANGLE", XA_ITALIC_ANGLE},
    // {"X_HEIGHT", XA_X_HEIGHT},
    // {"QUAD_WIDTH", XA_QUAD_WIDTH},
    // {"WEIGHT", XA_WEIGHT},
    // {"POINT_SIZE", XA_POINT_SIZE},
    // {"RESOLUTION", XA_RESOLUTION},
    // {"COPYRIGHT", XA_COPYRIGHT},
    // {"NOTICE", XA_NOTICE},
    // {"FONT_NAME", XA_FONT_NAME},
    // {"FAMILY_NAME", XA_FAMILY_NAME},
    // {"FULL_NAME", XA_FULL_NAME},
    // {"CAP_HEIGHT", XA_CAP_HEIGHT},
    {"WM_CLASS", XA_WM_CLASS},
    // {"WM_TRANSIENT_FOR", XA_WM_TRANSIENT_FOR},
};

constexpr const char* kAtomsToCache[] = {
    "ATOM_PAIR",
    "Abs Dbl End Timestamp",
    "Abs Dbl Fling X Velocity",
    "Abs Dbl Fling Y Velocity",
    "Abs Dbl Metrics Data 1",
    "Abs Dbl Metrics Data 2",
    "Abs Dbl Ordinal X",
    "Abs Dbl Ordinal Y",
    "Abs Dbl Start Timestamp",
    "Abs Finger Count",
    "Abs Fling State",
    "Abs MT Orientation",
    "Abs MT Position X",
    "Abs MT Position Y",
    "Abs MT Pressure",
    "Abs MT Touch Major",
    "Abs MT Touch Minor",
    "Abs MT Tracking ID",
    "Abs Metrics Type",
    "CHECK",
    "CHOME_SELECTION",
    "CHROME_SELECTION",
    "CHROMIUM_COMPOSITE_WINDOW",
    "CHROMIUM_TIMESTAMP",
    "CLIPBOARD",
    "CLIPBOARD_MANAGER",
    "Content Protection",
    "Desired",
    "Device Node",
    "Device Product ID",
    "EDID",
    "Enabled",
    "FAKE_SELECTION",
    "Full aspect",
    "INCR",
    "KEYBOARD",
    "LOCK",
    "MOUSE",
    "MULTIPLE",
    "Rel Horiz Wheel",
    "Rel Vert Wheel",
    "SAVE_TARGETS",
    "SELECTION_STRING",
    "TARGET1",
    "TARGET2",
    "TARGETS",
    "TEXT",
    "TIMESTAMP",
    "TOUCHPAD",
    "TOUCHSCREEN",
    "Tap Paused",
    "Touch Timestamp",
    "UTF8_STRING",
    "Undesired",
    "WM_DELETE_WINDOW",
    "WM_PROTOCOLS",
    "WM_WINDOW_ROLE",
    "XdndActionAsk",
    "XdndActionCopy",
    "XdndActionDirectSave",
    "XdndActionLink",
    "XdndActionList",
    "XdndActionMove",
    "XdndActionPrivate",
    "XdndAware",
    "XdndDirectSave0",
    "XdndDrop",
    "XdndEnter",
    "XdndFinished",
    "XdndLeave",
    "XdndPosition",
    "XdndProxy",
    "XdndSelection",
    "XdndStatus",
    "XdndTypeList",
    "_CHROME_DISPLAY_INTERNAL",
    "_CHROME_DISPLAY_ROTATION",
    "_CHROME_DISPLAY_SCALE_FACTOR",
    "_CHROMIUM_DRAG_RECEIVER",
    "_GTK_HIDE_TITLEBAR_WHEN_MAXIMIZED",
    "_GTK_THEME_VARIANT",
    "_ICC_PROFILE",
    "_MOTIF_WM_HINTS",
    "_NETSCAPE_URL",
    "_NET_ACTIVE_WINDOW",
    "_NET_CLIENT_LIST_STACKING",
    "_NET_CURRENT_DESKTOP",
    "_NET_FRAME_EXTENTS",
    "_NET_SUPPORTED",
    "_NET_SUPPORTING_WM_CHECK",
    "_NET_SYSTEM_TRAY_OPCODE",
    "_NET_SYSTEM_TRAY_S0",
    "_NET_SYSTEM_TRAY_VISUAL",
    "_NET_WM_BYPASS_COMPOSITOR",
    "_NET_WM_CM_S0",
    "_NET_WM_DESKTOP",
    "_NET_WM_ICON",
    "_NET_WM_MOVERESIZE",
    "_NET_WM_NAME",
    "_NET_WM_PID",
    "_NET_WM_PING",
    "_NET_WM_STATE",
    "_NET_WM_STATE_ABOVE",
    "_NET_WM_STATE_FOCUSED",
    "_NET_WM_STATE_FULLSCREEN",
    "_NET_WM_STATE_HIDDEN",
    "_NET_WM_STATE_MAXIMIZED_HORZ",
    "_NET_WM_STATE_MAXIMIZED_VERT",
    "_NET_WM_STATE_SKIP_TASKBAR",
    "_NET_WM_STATE_STICKY",
    "_NET_WM_SYNC_REQUEST",
    "_NET_WM_SYNC_REQUEST_COUNTER",
    "_NET_WM_USER_TIME",
    "_NET_WM_WINDOW_OPACITY",
    "_NET_WM_WINDOW_TYPE",
    "_NET_WM_WINDOW_TYPE_DND",
    "_NET_WM_WINDOW_TYPE_MENU",
    "_NET_WM_WINDOW_TYPE_NORMAL",
    "_NET_WM_WINDOW_TYPE_NOTIFICATION",
    "_NET_WM_WINDOW_TYPE_TOOLTIP",
    "_NET_WORKAREA",
    "_SCREENSAVER_STATUS",
    "_SCREENSAVER_VERSION",
    "_XEMBED_INFO",
    "application/octet-stream",
    "application/vnd.chromium.test",
    "chromium/filename",
    "chromium/x-bookmark-entries",
    "chromium/x-browser-actions",
    "chromium/x-file-system-files",
    "chromium/x-pepper-custom-data",
    "chromium/x-renderer-taint",
    "chromium/x-web-custom-data",
    "chromium/x-webkit-paste",
    "image/png",
    "marker_event",
    "scaling mode",
    "text/html",
    "text/plain",
    "text/plain;charset=utf-8",
    "text/rtf",
    "text/uri-list",
    "text/x-moz-url",
};

constexpr int kCacheCount = base::size(kAtomsToCache);

}  // namespace

namespace gfx {

XAtom GetAtom(const char* name) {
  return X11AtomCache::GetInstance()->GetAtom(name);
}

X11AtomCache* X11AtomCache::GetInstance() {
  return base::Singleton<X11AtomCache>::get();
}

X11AtomCache::X11AtomCache() : xdisplay_(gfx::GetXDisplay()) {
  for (const auto& predefined_atom : kPredefinedAtoms)
    cached_atoms_[predefined_atom.atom_name] = predefined_atom.atom_value;

  // Grab all the atoms we need now to minimize roundtrips to the X11 server.
  std::vector<XAtom> cached_atoms(kCacheCount);
  XInternAtoms(xdisplay_, const_cast<char**>(kAtomsToCache), kCacheCount, False,
               cached_atoms.data());

  for (int i = 0; i < kCacheCount; ++i)
    cached_atoms_[kAtomsToCache[i]] = cached_atoms[i];
}

X11AtomCache::~X11AtomCache() {}

XAtom X11AtomCache::GetAtom(const char* name) const {
  const auto it = cached_atoms_.find(name);
  if (it != cached_atoms_.end())
    return it->second;

  // XInternAtom returns None on failure. Source:
  // https://www.x.org/releases/X11R7.5/doc/man/man3/XInternAtom.3.html
  XAtom atom = XInternAtom(xdisplay_, name, False);
  if (atom == None) {
    static int error_count = 0;
    ++error_count;
    // TODO(https://crbug.com/1000919): Evaluate and remove UMA metrics after
    // enough data is gathered.
    base::UmaHistogramCounts100("X11.XInternAtomFailure", error_count);
  }
  cached_atoms_.emplace(name, atom);
  return atom;
}

}  // namespace gfx
