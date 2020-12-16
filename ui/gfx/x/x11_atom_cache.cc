// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/x11_atom_cache.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/future.h"

namespace x11 {

namespace {

struct {
  const char* atom_name;
  Atom atom_value;
} const kPredefinedAtoms[] = {
    // {"PRIMARY", Atom::PRIMARY},
    // {"SECONDARY", Atom::SECONDARY},
    // {"ARC", Atom::ARC},
    {"ATOM", Atom::ATOM},
    // {"BITMAP", Atom::BITMAP},
    {"CARDINAL", Atom::CARDINAL},
    // {"COLORMAP", Atom::COLORMAP},
    // {"CURSOR", Atom::CURSOR},
    // {"CUT_BUFFER0", Atom::CUT_BUFFER0},
    // {"CUT_BUFFER1", Atom::CUT_BUFFER1},
    // {"CUT_BUFFER2", Atom::CUT_BUFFER2},
    // {"CUT_BUFFER3", Atom::CUT_BUFFER3},
    // {"CUT_BUFFER4", Atom::CUT_BUFFER4},
    // {"CUT_BUFFER5", Atom::CUT_BUFFER5},
    // {"CUT_BUFFER6", Atom::CUT_BUFFER6},
    // {"CUT_BUFFER7", Atom::CUT_BUFFER7},
    // {"DRAWABLE", Atom::DRAWABLE},
    // {"FONT", Atom::FONT},
    // {"INTEGER", Atom::INTEGER},
    // {"PIXMAP", Atom::PIXMAP},
    // {"POINT", Atom::POINT},
    // {"RECTANGLE", Atom::RECTANGLE},
    // {"RESOURCE_MANAGER", Atom::RESOURCE_MANAGER},
    // {"RGB_COLOR_MAP", Atom::RGB_COLOR_MAP},
    // {"RGB_BEST_MAP", Atom::RGB_BEST_MAP},
    // {"RGB_BLUE_MAP", Atom::RGB_BLUE_MAP},
    // {"RGB_DEFAULT_MAP", Atom::RGB_DEFAULT_MAP},
    // {"RGB_GRAY_MAP", Atom::RGB_GRAY_MAP},
    // {"RGB_GREEN_MAP", Atom::RGB_GREEN_MAP},
    // {"RGB_RED_MAP", Atom::RGB_RED_MAP},
    {"STRING", Atom::STRING},
    // {"VISUALID", Atom::VISUALID},
    // {"WINDOW", Atom::WINDOW},
    // {"WM_COMMAND", Atom::WM_COMMAND},
    // {"WM_HINTS", Atom::WM_HINTS},
    // {"WM_CLIENT_MACHINE", Atom::WM_CLIENT_MACHINE},
    // {"WM_ICON_NAME", Atom::WM_ICON_NAME},
    // {"WM_ICON_SIZE", Atom::WM_ICON_SIZE},
    // {"WM_NAME", Atom::WM_NAME},
    // {"WM_NORMAL_HINTS", Atom::WM_NORMAL_HINTS},
    // {"WM_SIZE_HINTS", Atom::WM_SIZE_HINTS},
    // {"WM_ZOOM_HINTS", Atom::WM_ZOOM_HINTS},
    // {"MIN_SPACE", Atom::MIN_SPACE},
    // {"NORM_SPACE", Atom::NORM_SPACE},
    // {"MAX_SPACE", Atom::MAX_SPACE},
    // {"END_SPACE", Atom::END_SPACE},
    // {"SUPERSCRIPT_X", Atom::SUPERSCRIPT_X},
    // {"SUPERSCRIPT_Y", Atom::SUPERSCRIPT_Y},
    // {"SUBSCRIPT_X", Atom::SUBSCRIPT_X},
    // {"SUBSCRIPT_Y", Atom::SUBSCRIPT_Y},
    // {"UNDERLINE_POSITION", Atom::UNDERLINE_POSITION},
    // {"UNDERLINE_THICKNESS", Atom::UNDERLINE_THICKNESS},
    // {"STRIKEOUT_ASCENT", Atom::STRIKEOUT_ASCENT},
    // {"STRIKEOUT_DESCENT", Atom::STRIKEOUT_DESCENT},
    // {"ITALIC_ANGLE", Atom::ITALIC_ANGLE},
    // {"X_HEIGHT", Atom::X_HEIGHT},
    // {"QUAD_WIDTH", Atom::QUAD_WIDTH},
    // {"WEIGHT", Atom::WEIGHT},
    // {"POINT_SIZE", Atom::POINT_SIZE},
    // {"RESOLUTION", Atom::RESOLUTION},
    // {"COPYRIGHT", Atom::COPYRIGHT},
    // {"NOTICE", Atom::NOTICE},
    // {"FONT_NAME", Atom::FONT_NAME},
    // {"FAMILY_NAME", Atom::FAMILY_NAME},
    // {"FULL_NAME", Atom::FULL_NAME},
    // {"CAP_HEIGHT", Atom::CAP_HEIGHT},
    {"WM_CLASS", Atom::WM_CLASS},
    // {"WM_TRANSIENT_FOR", Atom::WM_TRANSIENT_FOR},
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
    "image/svg+xml",
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

Atom GetAtom(const std::string& name) {
  return X11AtomCache::GetInstance()->GetAtom(name);
}

X11AtomCache* X11AtomCache::GetInstance() {
  return base::Singleton<X11AtomCache>::get();
}

X11AtomCache::X11AtomCache() : connection_(Connection::Get()) {
  for (const auto& predefined_atom : kPredefinedAtoms)
    cached_atoms_[predefined_atom.atom_name] = predefined_atom.atom_value;

  std::vector<Future<InternAtomReply>> requests;
  requests.reserve(kCacheCount);
  for (const char* name : kAtomsToCache)
    requests.push_back(
        connection_->InternAtom(InternAtomRequest{.name = name}));
  // Flush so all requests are sent before waiting on any replies.
  connection_->Flush();
  for (size_t i = 0; i < kCacheCount; ++i) {
    if (auto response = requests[i].Sync())
      cached_atoms_[kAtomsToCache[i]] = static_cast<Atom>(response->atom);
  }
}

X11AtomCache::~X11AtomCache() = default;

Atom X11AtomCache::GetAtom(const std::string& name) const {
  const auto it = cached_atoms_.find(name);
  if (it != cached_atoms_.end())
    return it->second;

  Atom atom = Atom::None;
  if (auto response =
          connection_->InternAtom(InternAtomRequest{.name = name}).Sync()) {
    atom = static_cast<Atom>(response->atom);
    cached_atoms_.emplace(name, atom);
  } else {
    static int error_count = 0;
    ++error_count;
    // TODO(https://crbug.com/1000919): Evaluate and remove UMA metrics after
    // enough data is gathered.
    base::UmaHistogramCounts100("X11.XInternAtomFailure", error_count);
  }
  return atom;
}

}  // namespace x11
