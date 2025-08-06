// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/linux/display_server_utils.h"

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/logging.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/ozone/public/ozone_switches.h"

#if BUILDFLAG(IS_OZONE_WAYLAND)
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/nix/xdg_util.h"
#include "ui/base/ui_base_features.h"
#endif

namespace ui {

namespace {

#if BUILDFLAG(IS_OZONE_WAYLAND)
bool InspectWaylandDisplay(base::Environment& env) {
  std::optional<std::string> wayland_display = env.GetVar("WAYLAND_DISPLAY");
  if (wayland_display.has_value()) {
    return true;
  }

  std::optional<std::string> xdg_runtime_dir = env.GetVar("XDG_RUNTIME_DIR");
  if (xdg_runtime_dir.has_value()) {
    constexpr char kDefaultWaylandSocketName[] = "wayland-0";
    const auto wayland_socket_path =
        base::FilePath(*xdg_runtime_dir).Append(kDefaultWaylandSocketName);
    if (base::PathExists(wayland_socket_path)) {
      env.SetVar("WAYLAND_DISPLAY", kDefaultWaylandSocketName);
      return true;
    }
  }
  return false;
}
#endif  // BUILDFLAG(IS_OZONE_WAYLAND)

}  // namespace

void SetOzonePlatformForLinuxIfNeeded(base::CommandLine& command_line) {
  // On the desktop, we fix the platform name if necessary.
  // See https://crbug.com/1246928.
  if (command_line.HasSwitch(switches::kOzonePlatform)) {
    return;
  }

#if BUILDFLAG(IS_OZONE_WAYLAND)
  auto env = base::Environment::Create();
  std::optional<std::string> xdg_session_type =
      env->GetVar(base::nix::kXdgSessionTypeEnvVar);
  if (xdg_session_type.has_value() && *xdg_session_type == "wayland") {
    command_line.AppendSwitchASCII(switches::kOzonePlatform, "wayland");
    return;
  }
#endif

#if BUILDFLAG(IS_OZONE_X11)
  command_line.AppendSwitchASCII(switches::kOzonePlatform, "x11");
#endif
}

bool HasWaylandDisplay(base::Environment& env) {
#if !BUILDFLAG(IS_OZONE_WAYLAND)
  return false;
#else
  static bool has_wayland_display = InspectWaylandDisplay(env);
  return has_wayland_display;
#endif  // !BUILDFLAG(IS_OZONE_WAYLAND)
}

bool HasX11Display(base::Environment& env) {
#if !BUILDFLAG(IS_OZONE_X11)
  return false;
#else
  return env.GetVar("DISPLAY").has_value();
#endif  // !BUILDFLAG(IS_OZONE_X11)
}

}  // namespace ui
