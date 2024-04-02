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
// environment. Unless it is already set in `command_line`, this function
// combines information from ozone-platform-hint command line switch, required
// environment variables, eg: DISPLAY for X11, WAYLAND_DISPLAY for Wayland, etc,
// in order to determine which Ozone platform backend must be selected.
void SetOzonePlatformForLinuxIfNeeded(base::CommandLine& command_line);

// Returns true if Wayland display variable or socket file is available.
bool HasWaylandDisplay(base::Environment& env);

// Returns true if X11 display variable or socket file is available.
bool HasX11Display(base::Environment& env);

}  // namespace ui

#endif  // UI_LINUX_DISPLAY_SERVER_UTILS_H_
