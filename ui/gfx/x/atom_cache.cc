// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/atom_cache.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/future.h"

namespace x11 {

namespace {

constexpr auto kAtomsToCache = std::to_array<const char* const>({
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
    "Abs Pressure",
    "Abs Tilt X",
    "Abs Tilt Y",
    "Abs X",
    "Abs Y",
    "CHECK",
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
    "STRING",
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
    "_GTK_FRAME_EXTENTS",
    "_GTK_HIDE_TITLEBAR_WHEN_MAXIMIZED",
    "_GTK_THEME_VARIANT",
    "_ICC_PROFILE",
    "_MOTIF_WM_HINTS",
    "_NETSCAPE_URL",
    "_NET_ACTIVE_WINDOW",
    "_NET_CURRENT_DESKTOP",
    "_NET_FRAME_EXTENTS",
    "_NET_STARTUP_INFO",
    "_NET_STARTUP_INFO_BEGIN",
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
    "_NET_WM_OPAQUE_REGION",
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
    "_NET_WM_WINDOW_TYPE_DIALOG",
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
    "chromium/x-source-url",
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
    "xwayland-keyboard",
    "xwayland-pointer",
    "xwayland-touch",
});

}  // namespace

Atom GetAtom(const char* name) {
  return Connection::Get()->GetAtom(name);
}

AtomCache::AtomCache(Connection* connection) : connection_(connection) {
  std::vector<Future<InternAtomReply>> requests;
  requests.reserve(kAtomsToCache.size());
  for (const char* name : kAtomsToCache) {
    requests.push_back(
        connection_->InternAtom(InternAtomRequest{.name = name}));
  }
  // Flush so all requests are sent before waiting on any replies.
  connection_->Flush();

  std::vector<std::pair<const char*, Atom>> atoms;
  // Reserve a little extra space in case unexpected atoms are cached,
  // to prevent reallocating the buffer.
  atoms.reserve(kAtomsToCache.size() + 3);
  for (size_t i = 0; i < kAtomsToCache.size(); ++i) {
    if (auto response = requests[i].Sync()) {
      atoms.emplace_back(kAtomsToCache[i], static_cast<Atom>(response->atom));
    }
  }
  cached_atoms_ = base::flat_map<const char*, Atom, Compare>(std::move(atoms));
}

AtomCache::~AtomCache() = default;

Atom AtomCache::GetAtom(const char* name) {
  const auto it = cached_atoms_.find(name);
  if (it != cached_atoms_.end()) {
    return it->second;
  }

  Atom atom = Atom::None;
  if (auto response =
          connection_->InternAtom(InternAtomRequest{.name = name}).Sync()) {
    atom = response->atom;
    CHECK_GT(atom, x11::Atom::kLastPredefinedAtom)
        << " Use x11::Atom::" << name << " instead of x11::GetAtom(\"" << name
        << "\")";

    auto owned_name = std::make_unique<std::string>(name);

    // Don't log an error if the atom is dynamically named (ends with a digit).
    if (owned_name->empty() || !absl::ascii_isdigit(owned_name->back())) {
      LOG(ERROR) << "Add " << name << " to kAtomsToCache";
    }

    cached_atoms_.emplace(
        owned_strings_.emplace_back(std::move(owned_name))->c_str(), atom);
  }
  return atom;
}

}  // namespace x11
