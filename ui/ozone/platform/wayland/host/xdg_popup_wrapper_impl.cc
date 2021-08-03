// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/xdg_popup_wrapper_impl.h"

#include <xdg-shell-client-protocol.h>
#include <xdg-shell-unstable-v6-client-protocol.h>

#include <memory>

#include "base/environment.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"
#include "ui/ozone/platform/wayland/host/wayland_popup.h"
#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"
#include "ui/ozone/platform/wayland/host/xdg_surface_wrapper_impl.h"
#include "ui/ozone/platform/wayland/host/xdg_toplevel_wrapper_impl.h"

namespace ui {

namespace {

uint32_t TranslateAnchor(WlAnchor anchor) {
  switch (anchor) {
    case WlAnchor::None:
      return XDG_POSITIONER_ANCHOR_NONE;
    case WlAnchor::Top:
      return XDG_POSITIONER_ANCHOR_TOP;
    case WlAnchor::Bottom:
      return XDG_POSITIONER_ANCHOR_BOTTOM;
    case WlAnchor::Left:
      return XDG_POSITIONER_ANCHOR_LEFT;
    case WlAnchor::Right:
      return XDG_POSITIONER_ANCHOR_RIGHT;
    case WlAnchor::TopLeft:
      return XDG_POSITIONER_ANCHOR_TOP_LEFT;
    case WlAnchor::BottomLeft:
      return XDG_POSITIONER_ANCHOR_BOTTOM_LEFT;
    case WlAnchor::TopRight:
      return XDG_POSITIONER_ANCHOR_TOP_RIGHT;
    case WlAnchor::BottomRight:
      return XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT;
  }
}

uint32_t TranslateGravity(WlGravity gravity) {
  switch (gravity) {
    case WlGravity::None:
      return XDG_POSITIONER_GRAVITY_NONE;
    case WlGravity::Top:
      return XDG_POSITIONER_GRAVITY_TOP;
    case WlGravity::Bottom:
      return XDG_POSITIONER_GRAVITY_BOTTOM;
    case WlGravity::Left:
      return XDG_POSITIONER_GRAVITY_LEFT;
    case WlGravity::Right:
      return XDG_POSITIONER_GRAVITY_RIGHT;
    case WlGravity::TopLeft:
      return XDG_POSITIONER_GRAVITY_TOP_LEFT;
    case WlGravity::BottomLeft:
      return XDG_POSITIONER_GRAVITY_BOTTOM_LEFT;
    case WlGravity::TopRight:
      return XDG_POSITIONER_GRAVITY_TOP_RIGHT;
    case WlGravity::BottomRight:
      return XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT;
  }
}

uint32_t TranslateContraintAdjustment(
    WlConstraintAdjustment constraint_adjustment) {
  uint32_t res = 0;
  if ((constraint_adjustment & WlConstraintAdjustment::SlideX) !=
      WlConstraintAdjustment::None)
    res |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X;
  if ((constraint_adjustment & WlConstraintAdjustment::SlideY) !=
      WlConstraintAdjustment::None)
    res |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y;
  if ((constraint_adjustment & WlConstraintAdjustment::FlipX) !=
      WlConstraintAdjustment::None)
    res |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X;
  if ((constraint_adjustment & WlConstraintAdjustment::FlipY) !=
      WlConstraintAdjustment::None)
    res |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y;
  if ((constraint_adjustment & WlConstraintAdjustment::ResizeX) !=
      WlConstraintAdjustment::None)
    res |= XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_X;
  if ((constraint_adjustment & WlConstraintAdjustment::ResizeY) !=
      WlConstraintAdjustment::None)
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

  struct xdg_positioner* positioner =
      CreatePositioner(wayland_window_->parent_window());
  if (!positioner)
    return false;

  xdg_popup_.reset(xdg_surface_get_popup(xdg_surface_wrapper_->xdg_surface(),
                                         parent_xdg_surface->xdg_surface(),
                                         positioner));
  if (!xdg_popup_)
    return false;

  xdg_positioner_destroy(positioner);

  if (CanGrabPopup(connection_)) {
    xdg_popup_grab(xdg_popup_.get(), connection_->seat(),
                   connection_->serial());
  }
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
  auto* positioner = CreatePositioner(wayland_window_->parent_window());
  DCHECK(positioner);
  // TODO(msisov): figure out how we can make use of the reposition token.
  // The protocol says the token itself is opaque, and has no other special
  // meaning.
  xdg_popup_reposition(xdg_popup_.get(), positioner, ++next_reposition_token_);
  connection_->ScheduleFlush();

  // We can safely destroy the object now.
  xdg_positioner_destroy(positioner);
  return true;
}

struct xdg_positioner* XDGPopupWrapperImpl::CreatePositioner(
    WaylandWindow* parent_window) {
  struct xdg_positioner* positioner;
  positioner = xdg_wm_base_create_positioner(connection_->shell());
  if (!positioner)
    return nullptr;

  // The parent we got must be the topmost in the stack of the same family
  // windows.
  auto* topmost_child = parent_window->GetTopMostChildWindow();
  DCHECK(parent_window == parent_window->GetTopMostChildWindow() ||
         topmost_child == wayland_window_);

  // Place anchor to the end of the possible position.
  gfx::Rect anchor_rect = GetAnchorRect(
      params_.menu_type, params_.bounds,
      gfx::ScaleToRoundedRect(parent_window->GetBounds(),
                              1.0 / parent_window->window_scale()));

  xdg_positioner_set_anchor_rect(positioner, anchor_rect.x(), anchor_rect.y(),
                                 anchor_rect.width(), anchor_rect.height());
  xdg_positioner_set_size(positioner, params_.bounds.width(),
                          params_.bounds.height());
  xdg_positioner_set_anchor(
      positioner,
      TranslateAnchor(GetAnchor(params_.menu_type, params_.bounds)));
  xdg_positioner_set_gravity(
      positioner,
      TranslateGravity(GetGravity(params_.menu_type, params_.bounds)));
  xdg_positioner_set_constraint_adjustment(
      positioner,
      TranslateContraintAdjustment(GetConstraintAdjustment(params_.menu_type)));
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
