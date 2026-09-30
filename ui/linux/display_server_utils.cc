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
#include "ui/gfx/switches.h"
#include "ui/ozone/public/ozone_switches.h"

#if BUILDFLAG(SUPPORTS_OZONE_WAYLAND)
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/nix/xdg_util.h"
#include "ui/base/ui_base_features.h"
#endif

namespace ui {

namespace {

#if BUILDFLAG(SUPPORTS_OZONE_WAYLAND)
constexpr char kDefaultWaylandSocketName[] = "wayland-0";

// Looks where wl_display_connect(nullptr) does. A variable that is set but
// empty hides the sources after it, as it does there.
bool CanConnectToWayland(base::Environment& env) {
  if (std::optional<std::string> socket = env.GetVar("WAYLAND_SOCKET")) {
    return !socket->empty();
  }
  if (std::optional<std::string> display = env.GetVar("WAYLAND_DISPLAY")) {
    return !display->empty();
  }
  std::optional<std::string> xdg_runtime_dir = env.GetVar("XDG_RUNTIME_DIR");
  return xdg_runtime_dir.has_value() &&
         base::PathExists(base::FilePath(*xdg_runtime_dir)
                              .Append(kDefaultWaylandSocketName));
}

bool InspectWaylandDisplay(base::Environment& env) {
  const bool uses_default_socket =
      !env.HasVar("WAYLAND_SOCKET") && !env.HasVar("WAYLAND_DISPLAY");
  if (!CanConnectToWayland(env)) {
    return false;
  }
  if (uses_default_socket) {
    env.SetVar("WAYLAND_DISPLAY", kDefaultWaylandSocketName);
  }
  return true;
}
#endif  // BUILDFLAG(SUPPORTS_OZONE_WAYLAND)

}  // namespace

void SetOzonePlatformForLinuxIfNeeded(base::CommandLine& command_line) {
  // On the desktop, we fix the platform name if necessary.
  // See https://crbug.com/1246928.
  if (command_line.HasSwitch(switches::kOzonePlatform)) {
    return;
  }

#if BUILDFLAG(SUPPORTS_OZONE_WAYLAND)
  auto env = base::Environment::Create();
  bool use_wayland = env->GetVar(base::nix::kXdgSessionTypeEnvVar) == "wayland";
#if BUILDFLAG(SUPPORTS_OZONE_X11)
  // The session type may be unset (no logind) or describe another session
  // (ssh -X, containers), so it only breaks ties.
  const bool has_wayland = CanConnectToWayland(*env);
  const bool has_x11 =
      HasX11Display(*env) || command_line.HasSwitch(switches::kX11Display);
  if (has_wayland != has_x11) {
    use_wayland = has_wayland;
  }
#endif
  if (use_wayland) {
    command_line.AppendSwitchASCII(switches::kOzonePlatform, "wayland");
    return;
  }
#endif

#if BUILDFLAG(SUPPORTS_OZONE_X11)
  command_line.AppendSwitchASCII(switches::kOzonePlatform, "x11");
#endif
}

bool HasWaylandDisplay(base::Environment& env) {
#if !BUILDFLAG(SUPPORTS_OZONE_WAYLAND)
  return false;
#else
  static bool has_wayland_display = InspectWaylandDisplay(env);
  return has_wayland_display;
#endif  // !BUILDFLAG(SUPPORTS_OZONE_WAYLAND)
}

bool HasX11Display(base::Environment& env) {
#if !BUILDFLAG(SUPPORTS_OZONE_X11)
  return false;
#else
  return !env.GetVar("DISPLAY").value_or(std::string()).empty();
#endif  // !BUILDFLAG(SUPPORTS_OZONE_X11)
}

}  // namespace ui
