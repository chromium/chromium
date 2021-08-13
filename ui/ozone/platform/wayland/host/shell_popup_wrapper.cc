// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/shell_popup_wrapper.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

ShellPopupParams::ShellPopupParams() = default;
ShellPopupParams::ShellPopupParams(const ShellPopupParams&) = default;
ShellPopupParams& ShellPopupParams::operator=(const ShellPopupParams&) =
    default;
ShellPopupParams::~ShellPopupParams() = default;

bool ShellPopupWrapper::CanGrabPopup(WaylandConnection* connection) const {
  // When drag process starts, as described the protocol -
  // https://goo.gl/1Mskq3, the client must have an active implicit grab. If
  // we try to create a popup and grab it, it will be immediately dismissed.
  // Thus, do not take explicit grab during drag process.
  if (connection->IsDragInProgress() || !connection->seat())
    return false;

  // According to the definition of the xdg protocol, the grab request must be
  // used in response to some sort of user action like a button press, key
  // press, or touch down event.
  EventType last_event_type = connection->event_serial().event_type;
  return last_event_type == ET_TOUCH_PRESSED ||
         last_event_type == ET_KEY_PRESSED ||
         last_event_type == ET_MOUSE_PRESSED;
}

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

}  // namespace ui
