// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/xdg_popup_wrapper_impl.h"

#include <aura-shell-client-protocol.h>
#include <xdg-shell-client-protocol.h>

#include <memory>

#include "base/environment.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/common/features.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"
#include "ui/ozone/platform/wayland/host/wayland_popup.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"
#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"
#include "ui/ozone/platform/wayland/host/wayland_zaura_shell.h"
#include "ui/ozone/platform/wayland/host/xdg_surface_wrapper_impl.h"
#include "ui/ozone/platform/wayland/host/xdg_toplevel_wrapper_impl.h"

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
      OwnedWindowConstraintAdjustment::kAdjustmentNone)
    res |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X;
  if ((constraint_adjustment &
       OwnedWindowConstraintAdjustment::kAdjustmentSlideY) !=
      OwnedWindowConstraintAdjustment::kAdjustmentNone)
    res |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y;
  if ((constraint_adjustment &
       OwnedWindowConstraintAdjustment::kAdjustmentFlipX) !=
      OwnedWindowConstraintAdjustment::kAdjustmentNone)
    res |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X;
  if ((constraint_adjustment &
       OwnedWindowConstraintAdjustment::kAdjustmentFlipY) !=
      OwnedWindowConstraintAdjustment::kAdjustmentNone)
    res |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y;
  if ((constraint_adjustment &
       OwnedWindowConstraintAdjustment::kAdjustmentResizeX) !=
      OwnedWindowConstraintAdjustment::kAdjustmentNone)
    res |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_X;
  if ((constraint_adjustment &
       OwnedWindowConstraintAdjustment::kAdjustmentRezizeY) !=
      OwnedWindowConstraintAdjustment::kAdjustmentNone)
    res |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_Y;
  return res;
}

}  // namespace

XDGPopupWrapperImpl::XDGPopupWrapperImpl(
    std::unique_ptr<XDGSurfaceWrapperImpl> surface,
    WaylandWindow* wayland_window,
    WaylandConnection* connection)
    : wayland_window_(wayland_window),
      connection_(connection),
      xdg_surface_wrapper_(std::move(surface)) {
  DCHECK(xdg_surface_wrapper_);
  DCHECK(wayland_window_ && wayland_window_->parent_window());
}

XDGPopupWrapperImpl::~XDGPopupWrapperImpl() = default;

bool XDGPopupWrapperImpl::Initialize(const ShellPopupParams& params) {
  if (!connection_->shell()) {
    NOTREACHED() << "Wrong shell protocol";
    return false;
  }

  XDGSurfaceWrapperImpl* parent_xdg_surface = nullptr;
  // If the parent window is a popup, the surface of that popup must be used as
  // a parent.
  if (auto* parent_popup = wayland_window_->parent_window()->AsWaylandPopup()) {
    XDGPopupWrapperImpl* popup =
        static_cast<XDGPopupWrapperImpl*>(parent_popup->shell_popup());
    parent_xdg_surface = popup->xdg_surface_wrapper();
  } else {
    WaylandToplevelWindow* wayland_surface =
        static_cast<WaylandToplevelWindow*>(wayland_window_->parent_window());
    parent_xdg_surface =
        static_cast<XDGToplevelWrapperImpl*>(wayland_surface->shell_toplevel())
            ->xdg_surface_wrapper();
  }

  if (!xdg_surface_wrapper_ || !parent_xdg_surface)
    return false;

  params_ = params;
  // Wayland doesn't allow empty bounds. If a zero or negative size is set, the
  // invalid_input error is raised. Thus, use the least possible one.
  // WaylandPopup will update its bounds upon the following configure event.
  if (params_.bounds.IsEmpty())
    params_.bounds.set_size({1, 1});

  static constexpr struct xdg_popup_listener xdg_popup_listener = {
      &XDGPopupWrapperImpl::Configure,
      &XDGPopupWrapperImpl::PopupDone,
      &XDGPopupWrapperImpl::Repositioned,
  };

  auto positioner = CreatePositioner();
  if (!positioner)
    return false;

  xdg_popup_.reset(xdg_surface_get_popup(xdg_surface_wrapper_->xdg_surface(),
                                         parent_xdg_surface->xdg_surface(),
                                         positioner.get()));
  if (!xdg_popup_)
    return false;
  connection_->window_manager()->NotifyWindowRoleAssigned(wayland_window_);

  if (connection_->zaura_shell()) {
    uint32_t version =
        zaura_shell_get_version(connection_->zaura_shell()->wl_object());
    if (version >= ZAURA_SHELL_GET_AURA_POPUP_FOR_XDG_POPUP_SINCE_VERSION) {
      aura_popup_.reset(zaura_shell_get_aura_popup_for_xdg_popup(
          connection_->zaura_shell()->wl_object(), xdg_popup_.get()));
      if (IsWaylandSurfaceSubmissionInPixelCoordinatesEnabled() &&
          version >=
              ZAURA_POPUP_SURFACE_SUBMISSION_IN_PIXEL_COORDINATES_SINCE_VERSION) {
        zaura_popup_surface_submission_in_pixel_coordinates(aura_popup_.get());
      }
      if (version >= ZAURA_POPUP_SET_MENU_SINCE_VERSION &&
          wayland_window_->type() == PlatformWindowType::kMenu) {
        zaura_popup_set_menu(aura_popup_.get());
      }
    }
  }

  GrabIfPossible(connection_, wayland_window_->parent_window());

  xdg_popup_add_listener(xdg_popup_.get(), &xdg_popup_listener, this);

  wayland_window_->root_surface()->Commit();
  return true;
}

void XDGPopupWrapperImpl::AckConfigure(uint32_t serial) {
  DCHECK(xdg_surface_wrapper_);
  xdg_surface_wrapper_->AckConfigure(serial);
}

bool XDGPopupWrapperImpl::IsConfigured() {
  DCHECK(xdg_surface_wrapper_);
  return xdg_surface_wrapper_->IsConfigured();
}

bool XDGPopupWrapperImpl::SetBounds(const gfx::Rect& new_bounds) {
  if (xdg_popup_get_version(xdg_popup_.get()) <
      XDG_POPUP_REPOSITIONED_SINCE_VERSION) {
    return false;
  }

  params_.bounds = new_bounds;

  // Create a new positioner with new bounds.
  auto positioner = CreatePositioner();
  if (!positioner)
    return false;

  // TODO(msisov): figure out how we can make use of the reposition token.
  // The protocol says the token itself is opaque, and has no other special
  // meaning.
  xdg_popup_reposition(xdg_popup_.get(), positioner.get(),
                       ++next_reposition_token_);

  connection_->Flush();
  return true;
}

void XDGPopupWrapperImpl::SetWindowGeometry(const gfx::Rect& bounds) {
  xdg_surface_set_window_geometry(xdg_surface_wrapper_->xdg_surface(),
                                  bounds.x(), bounds.y(), bounds.width(),
                                  bounds.height());
}

void XDGPopupWrapperImpl::Grab(uint32_t serial) {
  xdg_popup_grab(xdg_popup_.get(), connection_->seat()->wl_object(), serial);
}

bool XDGPopupWrapperImpl::SupportsDecoration() {
  if (!aura_popup_)
    return false;
  uint32_t version = zaura_popup_get_version(aura_popup_.get());
  return version >= ZAURA_POPUP_SET_DECORATION_SINCE_VERSION;
}

void XDGPopupWrapperImpl::Decorate() {
  zaura_popup_set_decoration(aura_popup_.get(),
                             ZAURA_POPUP_DECORATION_TYPE_SHADOW);
}

void XDGPopupWrapperImpl::SetScaleFactor(float scale_factor) {
  if (aura_popup_ && zaura_popup_get_version(aura_popup_.get()) >=
                         ZAURA_POPUP_SET_SCALE_FACTOR_SINCE_VERSION) {
    uint32_t value = *reinterpret_cast<uint32_t*>(&scale_factor);
    zaura_popup_set_scale_factor(aura_popup_.get(), value);
  }
}

wl::Object<xdg_positioner> XDGPopupWrapperImpl::CreatePositioner() {
  wl::Object<xdg_positioner> positioner(
      xdg_wm_base_create_positioner(connection_->shell()));
  if (!positioner)
    return {};

  gfx::Rect anchor_rect;
  OwnedWindowAnchorPosition anchor_position;
  OwnedWindowAnchorGravity anchor_gravity;
  OwnedWindowConstraintAdjustment constraint_adjustment;
  FillAnchorData(params_, &anchor_rect, &anchor_position, &anchor_gravity,
                 &constraint_adjustment);

  xdg_positioner_set_anchor_rect(positioner.get(), anchor_rect.x(),
                                 anchor_rect.y(), anchor_rect.width(),
                                 anchor_rect.height());
  xdg_positioner_set_size(positioner.get(), params_.bounds.width(),
                          params_.bounds.height());
  xdg_positioner_set_anchor(positioner.get(), TranslateAnchor(anchor_position));
  xdg_positioner_set_gravity(positioner.get(),
                             TranslateGravity(anchor_gravity));
  xdg_positioner_set_constraint_adjustment(
      positioner.get(), TranslateConstraintAdjustment(constraint_adjustment));
  return positioner;
}

// static
void XDGPopupWrapperImpl::Configure(void* data,
                                    struct xdg_popup* xdg_popup,
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
  WaylandWindow* window =
      static_cast<XDGPopupWrapperImpl*>(data)->wayland_window_;
  DCHECK(window);
  window->HandlePopupConfigure({x, y, width, height});
}

// static
void XDGPopupWrapperImpl::PopupDone(void* data, struct xdg_popup* xdg_popup) {
  WaylandWindow* window =
      static_cast<XDGPopupWrapperImpl*>(data)->wayland_window_;
  DCHECK(window);
  window->Hide();
  window->OnCloseRequest();
}

// static
void XDGPopupWrapperImpl::Repositioned(void* data,
                                       struct xdg_popup* xdg_popup,
                                       uint32_t token) {
  NOTIMPLEMENTED_LOG_ONCE();
}

XDGSurfaceWrapperImpl* XDGPopupWrapperImpl::xdg_surface_wrapper() const {
  DCHECK(xdg_surface_wrapper_.get());
  return xdg_surface_wrapper_.get();
}

}  // namespace ui
