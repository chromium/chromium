// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/xdg_popup.h"

#include <xdg-shell-client-protocol.h>

#include <memory>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_popup.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"
#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/xdg_toplevel.h"
#include "ui/ozone/public/ozone_switches.h"

namespace ui {

namespace {

uint32_t TranslateAnchor(OwnedWindowAnchorPosition anchor) {
  switch (anchor) {
    case OwnedWindowAnchorPosition::kNone:
      return XDG_POSITIONER_ANCHOR_NONE;
    case OwnedWindowAnchorPosition::kTop:
      return XDG_POSITIONER_ANCHOR_TOP;
    case OwnedWindowAnchorPosition::kBottom:
      return XDG_POSITIONER_ANCHOR_BOTTOM;
    case OwnedWindowAnchorPosition::kLeft:
      return XDG_POSITIONER_ANCHOR_LEFT;
    case OwnedWindowAnchorPosition::kRight:
      return XDG_POSITIONER_ANCHOR_RIGHT;
    case OwnedWindowAnchorPosition::kTopLeft:
      return XDG_POSITIONER_ANCHOR_TOP_LEFT;
    case OwnedWindowAnchorPosition::kBottomLeft:
      return XDG_POSITIONER_ANCHOR_BOTTOM_LEFT;
    case OwnedWindowAnchorPosition::kTopRight:
      return XDG_POSITIONER_ANCHOR_TOP_RIGHT;
    case OwnedWindowAnchorPosition::kBottomRight:
      return XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT;
  }
}

uint32_t TranslateGravity(OwnedWindowAnchorGravity gravity) {
  switch (gravity) {
    case OwnedWindowAnchorGravity::kNone:
      return XDG_POSITIONER_GRAVITY_NONE;
    case OwnedWindowAnchorGravity::kTop:
      return XDG_POSITIONER_GRAVITY_TOP;
    case OwnedWindowAnchorGravity::kBottom:
      return XDG_POSITIONER_GRAVITY_BOTTOM;
    case OwnedWindowAnchorGravity::kLeft:
      return XDG_POSITIONER_GRAVITY_LEFT;
    case OwnedWindowAnchorGravity::kRight:
      return XDG_POSITIONER_GRAVITY_RIGHT;
    case OwnedWindowAnchorGravity::kTopLeft:
      return XDG_POSITIONER_GRAVITY_TOP_LEFT;
    case OwnedWindowAnchorGravity::kBottomLeft:
      return XDG_POSITIONER_GRAVITY_BOTTOM_LEFT;
    case OwnedWindowAnchorGravity::kTopRight:
      return XDG_POSITIONER_GRAVITY_TOP_RIGHT;
    case OwnedWindowAnchorGravity::kBottomRight:
      return XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT;
  }
}

uint32_t TranslateConstraintAdjustment(
    OwnedWindowConstraintAdjustment constraint_adjustment) {
  uint32_t res = 0;
  if ((constraint_adjustment &
       OwnedWindowConstraintAdjustment::kAdjustmentSlideX) !=
      OwnedWindowConstraintAdjustment::kAdjustmentNone) {
    res |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X;
  }
  if ((constraint_adjustment &
       OwnedWindowConstraintAdjustment::kAdjustmentSlideY) !=
      OwnedWindowConstraintAdjustment::kAdjustmentNone) {
    res |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y;
  }
  if ((constraint_adjustment &
       OwnedWindowConstraintAdjustment::kAdjustmentFlipX) !=
      OwnedWindowConstraintAdjustment::kAdjustmentNone) {
    res |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X;
  }
  if ((constraint_adjustment &
       OwnedWindowConstraintAdjustment::kAdjustmentFlipY) !=
      OwnedWindowConstraintAdjustment::kAdjustmentNone) {
    res |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y;
  }
  if ((constraint_adjustment &
       OwnedWindowConstraintAdjustment::kAdjustmentResizeX) !=
      OwnedWindowConstraintAdjustment::kAdjustmentNone) {
    res |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_X;
  }
  if ((constraint_adjustment &
       OwnedWindowConstraintAdjustment::kAdjustmentRezizeY) !=
      OwnedWindowConstraintAdjustment::kAdjustmentNone) {
    res |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_Y;
  }
  return res;
}

bool IsGnomeShell() {
  auto env = base::Environment::Create();
  return base::nix::GetDesktopEnvironment(env.get()) ==
         base::nix::DESKTOP_ENVIRONMENT_GNOME;
}

}  // namespace

XdgPopup::XdgPopup(std::unique_ptr<XdgSurface> xdg_surface)
    : xdg_surface_(std::move(xdg_surface)) {
  CHECK(xdg_surface_);
  CHECK(window() && window()->parent_window());
}

XdgPopup::~XdgPopup() = default;

bool XdgPopup::Initialize(const InitParams& params) {
  auto* xdg_parent = window()->AsWaylandPopup()->GetXdgParentWindow();
  if (!xdg_parent) {
    NOTREACHED() << "xdg_popup does not have a valid parent xdg_surface";
  }

  struct xdg_surface* parent_xdg_surface = nullptr;
  // If the xdg_parent window is a popup, the surface of that popup must be used
  // as a parent to create this xdg_popup.
  if (auto* parent_popup = xdg_parent->AsWaylandPopup()) {
    parent_xdg_surface = parent_popup->xdg_popup()->xdg_surface();
  } else if (auto* parent_toplevel = xdg_parent->AsWaylandToplevelWindow()) {
    parent_xdg_surface = parent_toplevel->xdg_toplevel()->xdg_surface();
  }

  CHECK(xdg_surface_ && parent_xdg_surface);

  if (!xdg_surface_ || !parent_xdg_surface) {
    return false;
  }

  params_ = params;
  // Wayland doesn't allow empty bounds. If a zero or negative size is set, the
  // invalid_input error is raised. Thus, use the least possible one.
  // WaylandPopup will update its bounds upon the following configure event.
  if (params_.bounds.IsEmpty()) {
    params_.bounds.set_size({1, 1});
  }

  auto positioner = CreatePositioner();
  if (!positioner) {
    return false;
  }

  xdg_popup_.reset(xdg_surface_get_popup(xdg_surface(), parent_xdg_surface,
                                         positioner.get()));
  if (!xdg_popup_) {
    return false;
  }
  connection()->window_manager()->NotifyWindowRoleAssigned(window());

  std::optional<bool> parent_xdg_popup_has_grab;
  if (auto* parent_popup = xdg_parent->AsWaylandPopup()) {
    parent_xdg_popup_has_grab.emplace(parent_popup->xdg_popup()->has_grab());
  }
  GrabIfPossible(connection(), parent_xdg_popup_has_grab);

  static constexpr xdg_popup_listener kXdgPopupListener = {
      .configure = &OnConfigure,
      .popup_done = &OnPopupDone,
      .repositioned = &OnRepositioned,
  };
  xdg_popup_add_listener(xdg_popup_.get(), &kXdgPopupListener, this);

  window()->root_surface()->Commit();
  return true;
}

void XdgPopup::AckConfigure(uint32_t serial) {
  DCHECK(xdg_surface_);
  xdg_surface_->AckConfigure(serial);
}

bool XdgPopup::IsConfigured() {
  DCHECK(xdg_surface_);
  return xdg_surface_->IsConfigured();
}

bool XdgPopup::SetBounds(const gfx::Rect& new_bounds) {
  if (xdg_popup_get_version(xdg_popup_.get()) <
      XDG_POPUP_REPOSITIONED_SINCE_VERSION) {
    return false;
  }

  params_.bounds = new_bounds;

  // Create a new positioner with new bounds.
  auto positioner = CreatePositioner();
  if (!positioner) {
    return false;
  }

  // TODO(msisov): figure out how we can make use of the reposition token.
  // The protocol says the token itself is opaque, and has no other special
  // meaning.
  xdg_popup_reposition(xdg_popup_.get(), positioner.get(),
                       ++next_reposition_token_);

  connection()->Flush();
  return true;
}

void XdgPopup::SetWindowGeometry(const gfx::Rect& bounds) {
  xdg_surface_set_window_geometry(xdg_surface(), bounds.x(), bounds.y(),
                                  bounds.width(), bounds.height());
}

void XdgPopup::Grab(uint32_t serial) {
  xdg_popup_grab(xdg_popup_.get(), connection()->seat()->wl_object(), serial);
}

void XdgPopup::FillAnchorData(
    const InitParams& params,
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

wl::Object<xdg_positioner> XdgPopup::CreatePositioner() {
  wl::Object<xdg_positioner> positioner(
      xdg_wm_base_create_positioner(connection()->shell()));
  if (!positioner) {
    return {};
  }

  gfx::Rect anchor_rect;
  OwnedWindowAnchorPosition anchor_position;
  OwnedWindowAnchorGravity anchor_gravity;
  OwnedWindowConstraintAdjustment constraint_adjustment;
  FillAnchorData(params_, &anchor_rect, &anchor_position, &anchor_gravity,
                 &constraint_adjustment);

  // XDG protocol does not allow empty geometries, but Chrome does. Set a dummy
  // {1, 1} size to prevent protocol error.
  if (anchor_rect.IsEmpty()) {
    anchor_rect.set_size({1, 1});
  }
  xdg_positioner_set_anchor_rect(positioner.get(), anchor_rect.x(),
                                 anchor_rect.y(), anchor_rect.width(),
                                 anchor_rect.height());
  // XDG protocol does not allow empty geometries, but Chrome does. Set a dummy
  // {1, 1} size to prevent protocol error.
  if (params_.bounds.IsEmpty()) {
    params_.bounds.set_size({1, 1});
  }
  xdg_positioner_set_size(positioner.get(), params_.bounds.width(),
                          params_.bounds.height());
  xdg_positioner_set_anchor(positioner.get(), TranslateAnchor(anchor_position));
  xdg_positioner_set_gravity(positioner.get(),
                             TranslateGravity(anchor_gravity));
  xdg_positioner_set_constraint_adjustment(
      positioner.get(), TranslateConstraintAdjustment(constraint_adjustment));
  return positioner;
}

void XdgPopup::GrabIfPossible(WaylandConnection* connection,
                              std::optional<bool> parent_shell_popup_has_grab) {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(switches::kUseWaylandExplicitGrab)) {
    return;
  }

  // When drag process starts, as described the protocol -
  // https://goo.gl/1Mskq3, the client must have an active implicit grab. If
  // we try to create a popup and grab it, it will be immediately dismissed.
  // Thus, do not take explicit grab during drag process.
  if (connection->IsDragInProgress() || !connection->seat()) {
    return;
  }

  // According to the definition of the xdg protocol, the grab request must be
  // used in response to some sort of user action like a button press, key
  // press, or touch down event.
  auto serial = connection->serial_tracker().GetSerial(
      {wl::SerialType::kTouchPress, wl::SerialType::kMousePress,
       wl::SerialType::kKeyPress});
  if (!serial.has_value()) {
    return;
  }

  // The parent of a grabbing popup must either be an xdg_toplevel surface or
  // another xdg_popup with an explicit grab. If it is a popup that did not take
  // an explicit grab, an error will be raised, so early out if that's the case.
  if (!parent_shell_popup_has_grab.value_or(true)) {
    return;
  }

  if (serial->type == wl::SerialType::kTouchPress && IsGnomeShell()) {
    return;
  }

  Grab(serial->value);
  has_grab_ = true;
}

// static
void XdgPopup::OnConfigure(void* data,
                           xdg_popup* popup,
                           int32_t x,
                           int32_t y,
                           int32_t width,
                           int32_t height) {
  // As long as the Wayland compositor repositions/requires to position windows
  // relative to their parents, do not propagate final bounds information to
  // Chromium. The browser places windows in respect to screen origin, but
  // Wayland requires doing so in respect to parent window's origin. To properly
  // place windows, the bounds are translated and adjusted according to the
  // Wayland compositor needs during WaylandWindow::CreateXdgPopup call.
  auto* self = static_cast<XdgPopup*>(data);
  WaylandWindow* window = self->window();
  DCHECK(window);
  window->HandlePopupConfigure({x, y, width, height});
}

// static
void XdgPopup::OnPopupDone(void* data, xdg_popup* popup) {
  auto* self = static_cast<XdgPopup*>(data);
  WaylandWindow* window = self->window();
  DCHECK(window);
  window->Hide();
  window->OnCloseRequest();
}

// static
void XdgPopup::OnRepositioned(void* data, xdg_popup* popup, uint32_t token) {
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace ui
