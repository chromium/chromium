// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LINUX_DISPLAY_SERVER_UTILS_H_
#define UI_LINUX_DISPLAY_SERVER_UTILS_H_

namespace base {
class CommandLine;
class Environment;
}  // namespace base

namespace ui {

// Ensures Ozone platform command line option is properly set for Linux Desktop
// environment. Unless it is already set in `command_line`, this function picks
// the display server that can be reached, going by DISPLAY or --display for
// X11 and WAYLAND_SOCKET, WAYLAND_DISPLAY or the default socket for Wayland.
// If both or neither can, XDG_SESSION_TYPE decides.
void SetOzonePlatformForLinuxIfNeeded(base::CommandLine& command_line);

// Returns true if Wayland display variable or socket file is available.
bool HasWaylandDisplay(base::Environment& env);

// Returns true if X11 display variable or socket file is available.
bool HasX11Display(base::Environment& env);

}  // namespace ui

#endif  // UI_LINUX_DISPLAY_SERVER_UTILS_H_
