// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/xdg_popup_wrapper_impl.h"

#include <xdg-shell-client-protocol.h>
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
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"
#include "ui/ozone/platform/wayland/host/wayland_popup.h"
#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"
#include "ui/ozone/platform/wayland/host/xdg_surface_wrapper_impl.h"

namespace ui {

namespace {

uint32_t TranslateAnchorStable(WlAnchor anchor) {
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

uint32_t TranslateGravityStable(WlGravity gravity) {
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

uint32_t TranslateContraintAdjustmentStable(
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

uint32_t TranslateAnchorV6(WlAnchor anchor) {
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

uint32_t TranslateGravityV6(WlGravity gravity) {
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

uint32_t TranslateContraintAdjustmentV6(
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

uint32_t GetAnchor(MenuType menu_type, const gfx::Rect& bounds, bool stable) {
  WlAnchor anchor = WlAnchor::None;
  switch (menu_type) {
    case MenuType::TYPE_RIGHT_CLICK:
      anchor = WlAnchor::TopLeft;
      break;
    case MenuType::TYPE_3DOT_PARENT_MENU:
      anchor = WlAnchor::BottomRight;
      break;
    case MenuType::TYPE_3DOT_CHILD_MENU:
      // Chromium may want to manually position a child menu on the left side of
      // its parent menu. Thus, react accordingly. Positive x means the child is
      // located on the right side of the parent and negative - on the left
      // side.
      if (bounds.x() >= 0)
        anchor = WlAnchor::TopRight;
      else
        anchor = WlAnchor::TopLeft;
      break;
    case MenuType::TYPE_UNKNOWN:
      NOTREACHED() << "Unsupported menu type";
      break;
  }

  if (stable)
    return TranslateAnchorStable(anchor);
  else {
    return TranslateAnchorV6(anchor);
  }
}

uint32_t GetGravity(MenuType menu_type, const gfx::Rect& bounds, bool stable) {
  WlGravity gravity = WlGravity::None;
  switch (menu_type) {
    case MenuType::TYPE_RIGHT_CLICK:
      gravity = WlGravity::BottomRight;
      break;
    case MenuType::TYPE_3DOT_PARENT_MENU:
      gravity = WlGravity::BottomRight;
      break;
    case MenuType::TYPE_3DOT_CHILD_MENU:
      // Chromium may want to manually position a child menu on the left side of
      // its parent menu. Thus, react accordingly. Positive x means the child is
      // located on the right side of the parent and negative - on the left
      // side.
      if (bounds.x() >= 0)
        gravity = WlGravity::BottomRight;
      else
        gravity = WlGravity::BottomLeft;
      break;
    case MenuType::TYPE_UNKNOWN:
      NOTREACHED() << "Unsupported menu type";
      break;
  }

  if (stable)
    return TranslateGravityStable(gravity);
  else {
    return TranslateGravityV6(gravity);
  }
}

uint32_t GetConstraintAdjustment(MenuType menu_type, bool stable) {
  WlConstraintAdjustment constraint = WlConstraintAdjustment::None;

  switch (menu_type) {
    case MenuType::TYPE_RIGHT_CLICK:
      constraint = WlConstraintAdjustment::SlideX |
                   WlConstraintAdjustment::SlideY |
                   WlConstraintAdjustment::FlipY;
      break;
    case MenuType::TYPE_3DOT_PARENT_MENU:
      constraint =
          WlConstraintAdjustment::SlideX | WlConstraintAdjustment::FlipY;
      break;
    case MenuType::TYPE_3DOT_CHILD_MENU:
      constraint =
          WlConstraintAdjustment::SlideY | WlConstraintAdjustment::FlipX;
      break;
    case MenuType::TYPE_UNKNOWN:
      NOTREACHED() << "Unsupported menu type";
      break;
  }
  if (stable)
    return TranslateContraintAdjustmentStable(constraint);
  else {
    return TranslateContraintAdjustmentV6(constraint);
  }
}

}  // namespace

XDGPopupWrapperImpl::XDGPopupWrapperImpl(
    std::unique_ptr<XDGSurfaceWrapperImpl> surface,
    WaylandWindow* wayland_window)
    : wayland_window_(wayland_window), xdg_surface_(std::move(surface)) {
  DCHECK(xdg_surface_);
  DCHECK(wayland_window_ && wayland_window_->parent_window());
}

XDGPopupWrapperImpl::~XDGPopupWrapperImpl() = default;

bool XDGPopupWrapperImpl::Initialize(WaylandConnection* connection,
                                     const gfx::Rect& bounds) {
  if (!connection->shell() && !connection->shell_v6()) {
    NOTREACHED() << "Wrong shell protocol";
    return false;
  }

  XDGSurfaceWrapperImpl* parent_xdg_surface = nullptr;
  // If the parent window is a popup, the surface of that popup must be used as
  // a parent.
  if (wl::IsMenuType(wayland_window_->parent_window()->type())) {
    auto* wayland_popup =
        static_cast<WaylandPopup*>(wayland_window_->parent_window());
    XDGPopupWrapperImpl* popup =
        static_cast<XDGPopupWrapperImpl*>(wayland_popup->shell_popup());
    parent_xdg_surface = popup->xdg_surface();
  } else {
    WaylandToplevelWindow* wayland_surface =
        static_cast<WaylandToplevelWindow*>(wayland_window_->parent_window());
    parent_xdg_surface =
        static_cast<XDGSurfaceWrapperImpl*>(wayland_surface->shell_surface());
  }

  if (!xdg_surface_ || !parent_xdg_surface)
    return false;

  auto new_bounds = bounds;
  // Wayland doesn't allow empty bounds. If a zero or negative size is set, the
  // invalid_input error is raised. Thus, use the least possible one.
  // WaylandPopup will update its bounds upon the following configure event.
  if (new_bounds.IsEmpty())
    new_bounds.set_size({1, 1});

  if (connection->shell())
    return InitializeStable(connection, new_bounds, parent_xdg_surface);
  else if (connection->shell_v6())
    return InitializeV6(connection, new_bounds, parent_xdg_surface);
  return false;
}

bool XDGPopupWrapperImpl::InitializeStable(
    WaylandConnection* connection,
    const gfx::Rect& bounds,
    XDGSurfaceWrapperImpl* parent_xdg_surface) {
  static const struct xdg_popup_listener xdg_popup_listener = {
      &XDGPopupWrapperImpl::ConfigureStable,
      &XDGPopupWrapperImpl::PopupDoneStable,
  };

  struct xdg_positioner* positioner = CreatePositionerStable(
      connection, wayland_window_->parent_window(), bounds);
  if (!positioner)
    return false;

  xdg_popup_.reset(xdg_surface_get_popup(xdg_surface_->xdg_surface(),
                                         parent_xdg_surface->xdg_surface(),
                                         positioner));
  if (!xdg_popup_)
    return false;

  xdg_positioner_destroy(positioner);

  if (CanGrabPopup(connection)) {
    xdg_popup_grab(xdg_popup_.get(), connection->seat(), connection->serial());
  }
  xdg_popup_add_listener(xdg_popup_.get(), &xdg_popup_listener, this);

  wayland_window_->root_surface()->Commit();
  return true;
}

struct xdg_positioner* XDGPopupWrapperImpl::CreatePositionerStable(
    WaylandConnection* connection,
    WaylandWindow* parent_window,
    const gfx::Rect& bounds) {
  struct xdg_positioner* positioner;
  positioner = xdg_wm_base_create_positioner(connection->shell());
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

  xdg_positioner_set_anchor_rect(positioner, anchor_rect.x(), anchor_rect.y(),
                                 anchor_rect.width(), anchor_rect.height());
  xdg_positioner_set_size(positioner, bounds.width(), bounds.height());
  xdg_positioner_set_anchor(positioner, GetAnchor(menu_type, bounds, true));
  xdg_positioner_set_gravity(positioner, GetGravity(menu_type, bounds, true));
  xdg_positioner_set_constraint_adjustment(
      positioner, GetConstraintAdjustment(menu_type, true));
  return positioner;
}

bool XDGPopupWrapperImpl::InitializeV6(
    WaylandConnection* connection,
    const gfx::Rect& bounds,
    XDGSurfaceWrapperImpl* parent_xdg_surface) {
  static const struct zxdg_popup_v6_listener zxdg_popup_v6_listener = {
      &XDGPopupWrapperImpl::ConfigureV6,
      &XDGPopupWrapperImpl::PopupDoneV6,
  };

  zxdg_positioner_v6* positioner =
      CreatePositionerV6(connection, wayland_window_->parent_window(), bounds);
  if (!positioner)
    return false;

  zxdg_popup_v6_.reset(zxdg_surface_v6_get_popup(
      xdg_surface_->zxdg_surface(), parent_xdg_surface->zxdg_surface(),
      positioner));
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

zxdg_positioner_v6* XDGPopupWrapperImpl::CreatePositionerV6(
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
                                GetAnchor(menu_type, bounds, false));
  zxdg_positioner_v6_set_gravity(positioner,
                                 GetGravity(menu_type, bounds, false));
  zxdg_positioner_v6_set_constraint_adjustment(
      positioner, GetConstraintAdjustment(menu_type, false));
  return positioner;
}

MenuType XDGPopupWrapperImpl::GetMenuTypeForPositioner(
    WaylandConnection* connection,
    WaylandWindow* parent_window) const {
  bool is_right_click_menu =
      connection->event_source()->last_pointer_button_pressed() &
      EF_RIGHT_MOUSE_BUTTON;

  // Different types of menu require different anchors, constraint adjustments,
  // gravity and etc.
  if (is_right_click_menu)
    return MenuType::TYPE_RIGHT_CLICK;
  else if (!wl::IsMenuType(parent_window->type()))
    return MenuType::TYPE_3DOT_PARENT_MENU;
  else
    return MenuType::TYPE_3DOT_CHILD_MENU;
}

bool XDGPopupWrapperImpl::CanGrabPopup(WaylandConnection* connection) const {
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

void XDGPopupWrapperImpl::Configure(void* data,
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
void XDGPopupWrapperImpl::PopupDone(void* data) {
  WaylandWindow* window =
      static_cast<XDGPopupWrapperImpl*>(data)->wayland_window_;
  DCHECK(window);
  window->Hide();
  window->OnCloseRequest();
}

// static
void XDGPopupWrapperImpl::ConfigureStable(void* data,
                                          struct xdg_popup* xdg_popup,
                                          int32_t x,
                                          int32_t y,
                                          int32_t width,
                                          int32_t height) {
  Configure(data, x, y, width, height);
}

// static
void XDGPopupWrapperImpl::PopupDoneStable(void* data,
                                          struct xdg_popup* xdg_popup) {
  PopupDone(data);
}

// static
void XDGPopupWrapperImpl::ConfigureV6(void* data,
                                      struct zxdg_popup_v6* zxdg_popup_v6,
                                      int32_t x,
                                      int32_t y,
                                      int32_t width,
                                      int32_t height) {
  Configure(data, x, y, width, height);
}

// static
void XDGPopupWrapperImpl::PopupDoneV6(void* data,
                                      struct zxdg_popup_v6* zxdg_popup_v6) {
  PopupDone(data);
}

XDGSurfaceWrapperImpl* XDGPopupWrapperImpl::xdg_surface() {
  DCHECK(xdg_surface_.get());
  return xdg_surface_.get();
}

}  // namespace ui
