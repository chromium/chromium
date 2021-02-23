// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_screensaver_window_finder.h"

#include "base/command_line.h"
#include "base/strings/string_piece_forward.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/switches.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/scoped_ignore_errors.h"
#include "ui/gfx/x/screensaver.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/xproto_util.h"

namespace ui {

ScreensaverWindowFinder::ScreensaverWindowFinder() : exists_(false) {}

// static
bool ScreensaverWindowFinder::ScreensaverWindowExists() {
  // Avoid calling into potentially missing X11 APIs in headless mode.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kHeadless))
    return false;

  auto* connection = x11::Connection::Get();

  // Let the server know the client version before making any requests.
  connection->screensaver().QueryVersion(
      {x11::ScreenSaver::major_version, x11::ScreenSaver::minor_version});

  auto reply =
      connection->screensaver().QueryInfo(connection->default_root()).Sync();
  if (reply && static_cast<x11::ScreenSaver::State>(reply->state) ==
                   x11::ScreenSaver::State::On) {
    return true;
  }

  // Ironically, xscreensaver does not conform to the XScreenSaver protocol, so
  // info.state == ScreenSaverOff or info.state == ScreenSaverDisabled does not
  // necessarily mean that a screensaver is not active, so add a special check
  // for xscreensaver.
  x11::Atom lock_atom = x11::GetAtom("LOCK");
  std::vector<x11::Atom> atom_properties;
  if (GetArrayProperty(GetX11RootWindow(), x11::GetAtom("_SCREENSAVER_STATUS"),
                       &atom_properties) &&
      atom_properties.size() > 0) {
    if (atom_properties[0] == lock_atom)
      return true;
  }

  // Also check the top level windows to see if any of them are screensavers.
  x11::ScopedIgnoreErrors ignore_errors(connection);
  ScreensaverWindowFinder finder;
  ui::EnumerateTopLevelWindows(&finder);
  return finder.exists_;
}

bool ScreensaverWindowFinder::ShouldStopIterating(x11::Window window) {
  if (!ui::IsWindowVisible(window) || !IsScreensaverWindow(window))
    return false;
  exists_ = true;
  return true;
}

bool ScreensaverWindowFinder::IsScreensaverWindow(x11::Window window) const {
  // It should occupy the full screen.
  if (!ui::IsX11WindowFullScreen(window))
    return false;

  // For xscreensaver, the window should have _SCREENSAVER_VERSION property.
  if (ui::PropertyExists(window, x11::GetAtom("_SCREENSAVER_VERSION")))
    return true;

  // For all others, like gnome-screensaver, the window's WM_CLASS property
  // should contain "screensaver".
  std::vector<char> value;
  if (!GetArrayProperty(window, x11::Atom::WM_CLASS, &value))
    return false;

  return base::StringPiece(value.data(), value.size()).find("screensaver") !=
         base::StringPiece::npos;
}

}  // namespace ui
