// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/zxdg_popup_v6_wrapper_impl.h"

#include <xdg-shell-unstable-v6-client-protocol.h>

#include <memory>

#include "base/environment.h"
#include "base/nix/xdg_util.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"
#include "ui/ozone/platform/wayland/host/wayland_popup.h"
#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"
#include "ui/ozone/platform/wayland/host/zxdg_surface_v6_wrapper_impl.h"
#include "ui/ozone/platform/wayland/host/zxdg_toplevel_v6_wrapper_impl.h"

namespace ui {

namespace {

uint32_t TranslateAnchor(WlAnchor anchor) {
  switch (anchor) {
    case WlAnchor::None:
      return ZXDG_POSITIONER_V6_ANCHOR_NONE;
    case WlAnchor::Top:
      return ZXDG_POSITIONER_V6_ANCHOR_TOP;
    case WlAnchor::Bottom:
      return ZXDG_POSITIONER_V6_ANCHOR_BOTTOM;
    case WlAnchor::Left:
      return ZXDG_POSITIONER_V6_ANCHOR_LEFT;
    case WlAnchor::Right:
      return ZXDG_POSITIONER_V6_ANCHOR_RIGHT;
    case WlAnchor::TopLeft:
      return ZXDG_POSITIONER_V6_ANCHOR_TOP | ZXDG_POSITIONER_V6_ANCHOR_LEFT;
    case WlAnchor::BottomLeft:
      return ZXDG_POSITIONER_V6_ANCHOR_BOTTOM | ZXDG_POSITIONER_V6_ANCHOR_LEFT;
    case WlAnchor::TopRight:
      return ZXDG_POSITIONER_V6_ANCHOR_TOP | ZXDG_POSITIONER_V6_ANCHOR_RIGHT;
    case WlAnchor::BottomRight:
      return ZXDG_POSITIONER_V6_ANCHOR_BOTTOM | ZXDG_POSITIONER_V6_ANCHOR_RIGHT;
  }
}

uint32_t TranslateGravity(WlGravity gravity) {
  switch (gravity) {
    case WlGravity::None:
      return ZXDG_POSITIONER_V6_GRAVITY_NONE;
    case WlGravity::Top:
      return ZXDG_POSITIONER_V6_GRAVITY_TOP;
    case WlGravity::Bottom:
      return ZXDG_POSITIONER_V6_GRAVITY_BOTTOM;
    case WlGravity::Left:
      return ZXDG_POSITIONER_V6_GRAVITY_LEFT;
    case WlGravity::Right:
      return ZXDG_POSITIONER_V6_GRAVITY_RIGHT;
    case WlGravity::TopLeft:
      return ZXDG_POSITIONER_V6_GRAVITY_TOP | ZXDG_POSITIONER_V6_GRAVITY_LEFT;
    case WlGravity::BottomLeft:
      return ZXDG_POSITIONER_V6_GRAVITY_BOTTOM |
             ZXDG_POSITIONER_V6_GRAVITY_LEFT;
    case WlGravity::TopRight:
      return ZXDG_POSITIONER_V6_GRAVITY_TOP | ZXDG_POSITIONER_V6_GRAVITY_RIGHT;
    case WlGravity::BottomRight:
      return ZXDG_POSITIONER_V6_GRAVITY_BOTTOM |
             ZXDG_POSITIONER_V6_GRAVITY_RIGHT;
  }
}

uint32_t TranslateConstraintAdjustment(
    WlConstraintAdjustment constraint_adjustment) {
  uint32_t res = 0;
  if ((constraint_adjustment & WlConstraintAdjustment::SlideX) !=
      WlConstraintAdjustment::None)
    res |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_SLIDE_X;
  if ((constraint_adjustment & WlConstraintAdjustment::SlideY) !=
      WlConstraintAdjustment::None)
    res |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_SLIDE_Y;
  if ((constraint_adjustment & WlConstraintAdjustment::FlipX) !=
      WlConstraintAdjustment::None)
    res |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_X;
  if ((constraint_adjustment & WlConstraintAdjustment::FlipY) !=
      WlConstraintAdjustment::None)
    res |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_Y;
  if ((constraint_adjustment & WlConstraintAdjustment::ResizeX) !=
      WlConstraintAdjustment::None)
    res |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_RESIZE_X;
  if ((constraint_adjustment & WlConstraintAdjustment::ResizeY) !=
      WlConstraintAdjustment::None)
    res |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_RESIZE_Y;
  return res;
}

}  // namespace

ZXDGPopupV6WrapperImpl::ZXDGPopupV6WrapperImpl(
    std::unique_ptr<ZXDGSurfaceV6WrapperImpl> surface,
    WaylandWindow* wayland_window)
    : wayland_window_(wayland_window),
      zxdg_surface_v6_wrapper_(std::move(surface)) {
  DCHECK(zxdg_surface_v6_wrapper_);
  DCHECK(wayland_window_ && wayland_window_->parent_window());
}

ZXDGPopupV6WrapperImpl::~ZXDGPopupV6WrapperImpl() = default;

bool ZXDGPopupV6WrapperImpl::Initialize(WaylandConnection* connection,
                                        const gfx::Rect& bounds) {
  if (!connection->shell() && !connection->shell_v6()) {
    NOTREACHED() << "Wrong shell protocol";
    return false;
  }

  ZXDGSurfaceV6WrapperImpl* parent_xdg_surface = nullptr;
  // If the parent window is a popup, the surface of that popup must be used as
  // a parent.
  if (wl::IsMenuType(wayland_window_->parent_window()->type())) {
    auto* wayland_popup =
        static_cast<WaylandPopup*>(wayland_window_->parent_window());
    ZXDGPopupV6WrapperImpl* popup =
        static_cast<ZXDGPopupV6WrapperImpl*>(wayland_popup->shell_popup());
    parent_xdg_surface = popup->zxdg_surface_v6_wrapper();
  } else {
    WaylandToplevelWindow* wayland_surface =
        static_cast<WaylandToplevelWindow*>(wayland_window_->parent_window());
    parent_xdg_surface = static_cast<ZXDGToplevelV6WrapperImpl*>(
                             wayland_surface->shell_toplevel())
                             ->zxdg_surface_v6_wrapper();
  }

  if (!zxdg_surface_v6_wrapper_ || !parent_xdg_surface)
    return false;

  auto new_bounds = bounds;
  // Wayland doesn't allow empty bounds. If a zero or negative size is set, the
  // invalid_input error is raised. Thus, use the least possible one.
  // WaylandPopup will update its bounds upon the following configure event.
  if (new_bounds.IsEmpty())
    new_bounds.set_size({1, 1});

  if (connection->shell_v6())
    return InitializeV6(connection, new_bounds, parent_xdg_surface);

  return false;
}

bool ZXDGPopupV6WrapperImpl::InitializeV6(
    WaylandConnection* connection,
    const gfx::Rect& bounds,
    ZXDGSurfaceV6WrapperImpl* parent_xdg_surface) {
  static const struct zxdg_popup_v6_listener zxdg_popup_v6_listener = {
      &ZXDGPopupV6WrapperImpl::Configure,
      &ZXDGPopupV6WrapperImpl::PopupDone,
  };

  zxdg_positioner_v6* positioner =
      CreatePositioner(connection, wayland_window_->parent_window(), bounds);
  if (!positioner)
    return false;

  zxdg_popup_v6_.reset(zxdg_surface_v6_get_popup(
      zxdg_surface_v6_wrapper_->zxdg_surface(),
      parent_xdg_surface->zxdg_surface(), positioner));
  if (!zxdg_popup_v6_)
    return false;

  zxdg_positioner_v6_destroy(positioner);

  if (CanGrabPopup(connection)) {
    zxdg_popup_v6_grab(zxdg_popup_v6_.get(), connection->seat(),
                       connection->serial());
  }
  zxdg_popup_v6_add_listener(zxdg_popup_v6_.get(), &zxdg_popup_v6_listener,
                             this);

  wayland_window_->root_surface()->Commit();
  return true;
}

void ZXDGPopupV6WrapperImpl::AckConfigure(uint32_t serial) {
  DCHECK(zxdg_surface_v6_wrapper_);
  zxdg_surface_v6_wrapper_->AckConfigure(serial);
}

zxdg_positioner_v6* ZXDGPopupV6WrapperImpl::CreatePositioner(
    WaylandConnection* connection,
    WaylandWindow* parent_window,
    const gfx::Rect& bounds) {
  struct zxdg_positioner_v6* positioner;
  positioner = zxdg_shell_v6_create_positioner(connection->shell_v6());
  if (!positioner)
    return nullptr;

  auto menu_type = GetMenuTypeForPositioner(connection, parent_window);

  // The parent we got must be the topmost in the stack of the same family
  // windows.
  DCHECK_EQ(parent_window->GetTopMostChildWindow(), parent_window);

  // Place anchor to the end of the possible position.
  gfx::Rect anchor_rect = GetAnchorRect(
      menu_type, bounds,
      gfx::ScaleToRoundedRect(parent_window->GetBounds(),
                              1.0 / parent_window->buffer_scale()));

  zxdg_positioner_v6_set_anchor_rect(positioner, anchor_rect.x(),
                                     anchor_rect.y(), anchor_rect.width(),
                                     anchor_rect.height());
  zxdg_positioner_v6_set_size(positioner, bounds.width(), bounds.height());
  zxdg_positioner_v6_set_anchor(positioner,
                                TranslateAnchor(GetAnchor(menu_type, bounds)));
  zxdg_positioner_v6_set_gravity(
      positioner, TranslateGravity(GetGravity(menu_type, bounds)));
  zxdg_positioner_v6_set_constraint_adjustment(
      positioner,
      TranslateConstraintAdjustment(GetConstraintAdjustment(menu_type)));
  return positioner;
}

// static
void ZXDGPopupV6WrapperImpl::Configure(void* data,
                                       struct zxdg_popup_v6* zxdg_popup_v6,
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
      static_cast<ZXDGPopupV6WrapperImpl*>(data)->wayland_window_;
  DCHECK(window);
  window->HandlePopupConfigure({x, y, width, height});
}

// static
void ZXDGPopupV6WrapperImpl::PopupDone(void* data,
                                       struct zxdg_popup_v6* zxdg_popup_v6) {
  WaylandWindow* window =
      static_cast<ZXDGPopupV6WrapperImpl*>(data)->wayland_window_;
  DCHECK(window);
  window->Hide();
  window->OnCloseRequest();
}

ZXDGSurfaceV6WrapperImpl* ZXDGPopupV6WrapperImpl::zxdg_surface_v6_wrapper()
    const {
  DCHECK(zxdg_surface_v6_wrapper_.get());
  return zxdg_surface_v6_wrapper_.get();
}

}  // namespace ui
