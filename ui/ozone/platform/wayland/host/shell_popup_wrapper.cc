// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/shell_popup_wrapper.h"

#include <optional>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/notreached.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/owned_window_anchor.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_popup.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"
#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

namespace {

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
bool IsGnomeShell() {
  auto env = base::Environment::Create();
  return base::nix::GetDesktopEnvironment(env.get()) ==
         base::nix::DESKTOP_ENVIRONMENT_GNOME;
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace

ShellPopupParams::ShellPopupParams() = default;
ShellPopupParams::ShellPopupParams(const ShellPopupParams&) = default;
ShellPopupParams& ShellPopupParams::operator=(const ShellPopupParams&) =
    default;
ShellPopupParams::~ShellPopupParams() = default;

void ShellPopupWrapper::FillAnchorData(
    const ShellPopupParams& params,
    gfx::Rect* anchor_rect,
    OwnedWindowAnchorPosition* anchor_position,
    OwnedWindowAnchorGravity* anchor_gravity,
    OwnedWindowConstraintAdjustment* constraints) const {
  DCHECK(anchor_rect && anchor_position && anchor_gravity && constraints);
  if (params.anchor.has_value()) {
    *anchor_rect = params.anchor->anchor_rect;
    *anchor_position = params.anchor->anchor_position;
    *anchor_gravity = params.anchor->anchor_gravity;
    *constraints = params.anchor->constraint_adjustment;
    return;
  }

  // Use default parameters if params.anchor doesn't have any data.
  *anchor_rect = params.bounds;
  anchor_rect->set_size({1, 1});
  *anchor_position = OwnedWindowAnchorPosition::kTopLeft;
  *anchor_gravity = OwnedWindowAnchorGravity::kBottomRight;
  *constraints = OwnedWindowConstraintAdjustment::kAdjustmentFlipY;
}

XDGPopupWrapperImpl* ShellPopupWrapper::AsXDGPopupWrapper() {
  return nullptr;
}

void ShellPopupWrapper::GrabIfPossible(
    WaylandConnection* connection,
    std::optional<bool> parent_shell_popup_has_grab) {
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(switches::kUseWaylandExplicitGrab))
    return;
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

  // When drag process starts, as described the protocol -
  // https://goo.gl/1Mskq3, the client must have an active implicit grab. If
  // we try to create a popup and grab it, it will be immediately dismissed.
  // Thus, do not take explicit grab during drag process.
  if (connection->IsDragInProgress() || !connection->seat())
    return;

  // According to the definition of the xdg protocol, the grab request must be
  // used in response to some sort of user action like a button press, key
  // press, or touch down event.
  auto serial = connection->serial_tracker().GetSerial(
      {wl::SerialType::kTouchPress, wl::SerialType::kMousePress,
       wl::SerialType::kKeyPress});
  if (!serial.has_value())
    return;

  // The parent of a grabbing popup must either be an xdg_toplevel surface or
  // another xdg_popup with an explicit grab. If it is a popup that did not take
  // an explicit grab, an error will be raised, so early out if that's the case.
  if (!parent_shell_popup_has_grab.value_or(true)) {
    return;
  }

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  if (serial->type == wl::SerialType::kTouchPress && IsGnomeShell())
    return;
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

  Grab(serial->value);
  has_grab_ = true;
}

}  // namespace ui
