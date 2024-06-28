// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/linux/display_server_utils.h"

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
#include "third_party/angle/src/gpu_info_util/SystemInfo.h"
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
  std::string wayland_display;
  const bool has_wayland_display =
      env.GetVar("WAYLAND_DISPLAY", &wayland_display) &&
      !wayland_display.empty();
  if (has_wayland_display) {
    return true;
  }

  std::string xdg_runtime_dir;
  const bool has_xdg_runtime_dir =
      env.GetVar("XDG_RUNTIME_DIR", &xdg_runtime_dir) &&
      !xdg_runtime_dir.empty();
  if (has_xdg_runtime_dir) {
    constexpr char kDefaultWaylandSocketName[] = "wayland-0";
    const auto wayland_socket_path =
        base::FilePath(xdg_runtime_dir).Append(kDefaultWaylandSocketName);
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
    std::string xdg_session_type;
    const bool has_xdg_session_type =
        env->GetVar(base::nix::kXdgSessionTypeEnvVar, &xdg_session_type) &&
        !xdg_session_type.empty();

    if ((has_xdg_session_type && xdg_session_type == "wayland") ||
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

  // We do not override users on NVIDIA to --ozone-platform-hint=auto because of
  // flickering due to missing linux-drm-syncobj.
  // https://gitlab.freedesktop.org/wayland/wayland-protocols/merge_requests/90
  angle::SystemInfo system_info;
  bool success = angle::GetSystemInfo(&system_info);
  if (system_info.gpus.empty()) {
    return;
  }
  if (system_info.activeGPUIndex < 0) {
    system_info.activeGPUIndex = 0;
  }

  if (success && system_info.gpus[system_info.activeGPUIndex].vendorId !=
                     angle::kVendorID_NVIDIA) {
    command_line.AppendSwitchASCII(switches::kOzonePlatformHint, "auto");
  }
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
  std::string xdisplay;
  return env.GetVar("DISPLAY", &xdisplay) && !xdisplay.empty();
#endif  // !BUILDFLAG(IS_OZONE_X11)
}

}  // namespace ui
