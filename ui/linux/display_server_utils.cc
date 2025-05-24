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

#if BUILDFLAG(IS_OZONE_X11)
constexpr char kPlatformX11[] = "x11";
#endif

#if BUILDFLAG(IS_OZONE_WAYLAND)
constexpr char kPlatformWayland[] = "wayland";

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

// Evaluates the environment and returns the effective platform name for the
// given |ozone_platform_hint|.
// For the "auto" value, returns "wayland" if the XDG session type is "wayland",
// "x11" otherwise.
// For the "wayland" value, checks if the Wayland server is available, and
// returns "x11" if it is not. See https://crbug.com/1246928.
std::string MaybeFixPlatformName(const std::string& platform_hint) {
#if BUILDFLAG(IS_OZONE_WAYLAND)
  // Wayland is selected if both conditions below are true:
  // 1. The user selected either 'wayland' or 'auto'.
  // 2. The XDG session type is 'wayland', OR the user has selected 'wayland'
  //    explicitly and a Wayland server is running.
  // Otherwise, fall back to X11.
  if (platform_hint == kPlatformWayland || platform_hint == "auto") {
    auto env = base::Environment::Create();
    std::optional<std::string> xdg_session_type =
        env->GetVar(base::nix::kXdgSessionTypeEnvVar);

    if ((xdg_session_type.has_value() && *xdg_session_type == "wayland") ||
        (platform_hint == kPlatformWayland && HasWaylandDisplay(*env))) {
      return kPlatformWayland;
    }
  }
#endif  // BUILDFLAG(IS_OZONE_WAYLAND)

#if BUILDFLAG(IS_OZONE_X11)
  if (platform_hint == kPlatformX11) {
    return kPlatformX11;
  }
#if BUILDFLAG(IS_OZONE_WAYLAND)
  if (platform_hint == kPlatformWayland || platform_hint == "auto") {
    // We are here if:
    // - The binary has both X11 and Wayland backends.
    // - The user wanted Wayland but that did not work, otherwise it would have
    //   been returned above.
    if (platform_hint == kPlatformWayland) {
      LOG(WARNING) << "No Wayland server is available. Falling back to X11.";
    } else {
      LOG(WARNING) << "This is not a Wayland session. Falling back to X11. "
                      "If you need to run Chrome on Wayland using some "
                      "embedded compositor, e.g. Weston, please specify "
                      "Wayland as your preferred Ozone platform, or use "
                      "--ozone-platform=wayland.";
    }
    return kPlatformX11;
  }
#endif  // BUILDFLAG(IS_OZONE_WAYLAND)
#endif  // BUILDFLAG(IS_OZONE_X11)

  return platform_hint;
}

void MaybeOverrideDefaultAsAuto(base::CommandLine& command_line) {
#if BUILDFLAG(IS_OZONE_WAYLAND)
  const auto ozone_platform_hint =
      command_line.GetSwitchValueASCII(switches::kOzonePlatformHint);
  if (!ozone_platform_hint.empty() ||
      !base::FeatureList::IsEnabled(
          features::kOverrideDefaultOzonePlatformHintToAuto)) {
    return;
  }
  command_line.AppendSwitchASCII(switches::kOzonePlatformHint, "auto");
#endif  // BUILDFLAG(IS_OZONE_WAYLAND)
}

}  // namespace

void SetOzonePlatformForLinuxIfNeeded(base::CommandLine& command_line) {
  // On the desktop, we fix the platform name if necessary.
  // See https://crbug.com/1246928.
  if (!command_line.HasSwitch(switches::kOzonePlatform)) {
    MaybeOverrideDefaultAsAuto(command_line);
    const auto ozone_platform_hint =
        command_line.GetSwitchValueASCII(switches::kOzonePlatformHint);
    if (!ozone_platform_hint.empty()) {
      command_line.AppendSwitchASCII(switches::kOzonePlatform,
                                     MaybeFixPlatformName(ozone_platform_hint));
    }
  }
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
