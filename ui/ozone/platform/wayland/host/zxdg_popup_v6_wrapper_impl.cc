// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/zxdg_popup_v6_wrapper_impl.h"

#include <xdg-shell-unstable-v6-client-protocol.h>

#include <memory>

#include "base/environment.h"
#include "base/nix/xdg_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/shell_popup_wrapper.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"
#include "ui/ozone/platform/wayland/host/wayland_popup.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"
#include "ui/ozone/platform/wayland/host/zxdg_surface_v6_wrapper_impl.h"
#include "ui/ozone/platform/wayland/host/zxdg_toplevel_v6_wrapper_impl.h"

namespace ui {

namespace {

uint32_t TranslateAnchor(OwnedWindowAnchorPosition anchor) {
  switch (anchor) {
    case OwnedWindowAnchorPosition::kNone:
      return ZXDG_POSITIONER_V6_ANCHOR_NONE;
    case OwnedWindowAnchorPosition::kTop:
      return ZXDG_POSITIONER_V6_ANCHOR_TOP;
    case OwnedWindowAnchorPosition::kBottom:
      return ZXDG_POSITIONER_V6_ANCHOR_BOTTOM;
    case OwnedWindowAnchorPosition::kLeft:
      return ZXDG_POSITIONER_V6_ANCHOR_LEFT;
    case OwnedWindowAnchorPosition::kRight:
      return ZXDG_POSITIONER_V6_ANCHOR_RIGHT;
    case OwnedWindowAnchorPosition::kTopLeft:
      return ZXDG_POSITIONER_V6_ANCHOR_TOP | ZXDG_POSITIONER_V6_ANCHOR_LEFT;
    case OwnedWindowAnchorPosition::kBottomLeft:
      return ZXDG_POSITIONER_V6_ANCHOR_BOTTOM | ZXDG_POSITIONER_V6_ANCHOR_LEFT;
    case OwnedWindowAnchorPosition::kTopRight:
      return ZXDG_POSITIONER_V6_ANCHOR_TOP | ZXDG_POSITIONER_V6_ANCHOR_RIGHT;
    case OwnedWindowAnchorPosition::kBottomRight:
      return ZXDG_POSITIONER_V6_ANCHOR_BOTTOM | ZXDG_POSITIONER_V6_ANCHOR_RIGHT;
  }
}

uint32_t TranslateGravity(OwnedWindowAnchorGravity gravity) {
  switch (gravity) {
    case OwnedWindowAnchorGravity::kNone:
      return ZXDG_POSITIONER_V6_GRAVITY_NONE;
    case OwnedWindowAnchorGravity::kTop:
      return ZXDG_POSITIONER_V6_GRAVITY_TOP;
    case OwnedWindowAnchorGravity::kBottom:
      return ZXDG_POSITIONER_V6_GRAVITY_BOTTOM;
    case OwnedWindowAnchorGravity::kLeft:
      return ZXDG_POSITIONER_V6_GRAVITY_LEFT;
    case OwnedWindowAnchorGravity::kRight:
      return ZXDG_POSITIONER_V6_GRAVITY_RIGHT;
    case OwnedWindowAnchorGravity::kTopLeft:
      return ZXDG_POSITIONER_V6_GRAVITY_TOP | ZXDG_POSITIONER_V6_GRAVITY_LEFT;
    case OwnedWindowAnchorGravity::kBottomLeft:
      return ZXDG_POSITIONER_V6_GRAVITY_BOTTOM |
             ZXDG_POSITIONER_V6_GRAVITY_LEFT;
    case OwnedWindowAnchorGravity::kTopRight:
      return ZXDG_POSITIONER_V6_GRAVITY_TOP | ZXDG_POSITIONER_V6_GRAVITY_RIGHT;
    case OwnedWindowAnchorGravity::kBottomRight:
      return ZXDG_POSITIONER_V6_GRAVITY_BOTTOM |
             ZXDG_POSITIONER_V6_GRAVITY_RIGHT;
  }
}

uint32_t TranslateConstraintAdjustment(
    OwnedWindowConstraintAdjustment constraint_adjustment) {
  uint32_t res = 0;
  if ((constraint_adjustment &
       OwnedWindowConstraintAdjustment::kAdjustmentSlideX) !=
      OwnedWindowConstraintAdjustment::kAdjustmentNone)
    res |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_SLIDE_X;
  if ((constraint_adjustment &
       OwnedWindowConstraintAdjustment::kAdjustmentSlideY) !=
      OwnedWindowConstraintAdjustment::kAdjustmentNone)
    res |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_SLIDE_Y;
  if ((constraint_adjustment &
       OwnedWindowConstraintAdjustment::kAdjustmentFlipX) !=
      OwnedWindowConstraintAdjustment::kAdjustmentNone)
    res |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_X;
  if ((constraint_adjustment &
       OwnedWindowConstraintAdjustment::kAdjustmentFlipY) !=
      OwnedWindowConstraintAdjustment::kAdjustmentNone)
    res |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_Y;
  if ((constraint_adjustment &
       OwnedWindowConstraintAdjustment::kAdjustmentResizeX) !=
      OwnedWindowConstraintAdjustment::kAdjustmentNone)
    res |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_RESIZE_X;
  if ((constraint_adjustment &
       OwnedWindowConstraintAdjustment::kAdjustmentRezizeY) !=
      OwnedWindowConstraintAdjustment::kAdjustmentNone)
    res |= ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_RESIZE_Y;
  return res;
}

}  // namespace

ZXDGPopupV6WrapperImpl::ZXDGPopupV6WrapperImpl(
    std::unique_ptr<ZXDGSurfaceV6WrapperImpl> surface,
    WaylandWindow* wayland_window,
    WaylandConnection* connection)
    : wayland_window_(wayland_window),
      connection_(connection),
      zxdg_surface_v6_wrapper_(std::move(surface)) {
  DCHECK(zxdg_surface_v6_wrapper_);
  DCHECK(wayland_window_ && wayland_window_->parent_window());
}

ZXDGPopupV6WrapperImpl::~ZXDGPopupV6WrapperImpl() = default;

bool ZXDGPopupV6WrapperImpl::Initialize(const ShellPopupParams& params) {
  if (!connection_->shell_v6()) {
    NOTREACHED() << "Wrong shell protocol";
    return false;
  }

  ZXDGSurfaceV6WrapperImpl* parent_xdg_surface = nullptr;
  // If the parent window is a popup, the surface of that popup must be used as
  // a parent.
  if (auto* parent_popup = wayland_window_->parent_window()->AsWaylandPopup()) {
    ZXDGPopupV6WrapperImpl* popup =
        static_cast<ZXDGPopupV6WrapperImpl*>(parent_popup->shell_popup());
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

  params_ = params;
  // Wayland doesn't allow empty bounds. If a zero or negative size is set, the
  // invalid_input error is raised. Thus, use the least possible one.
  // WaylandPopup will update its bounds upon the following configure event.
  if (params_.bounds.IsEmpty())
    params_.bounds.set_size({1, 1});

  static constexpr zxdg_popup_v6_listener zxdg_popup_v6_listener = {
      &ZXDGPopupV6WrapperImpl::Configure,
      &ZXDGPopupV6WrapperImpl::PopupDone,
  };

  auto positioner = CreatePositioner(wayland_window_->parent_window());
  if (!positioner)
    return false;

  zxdg_popup_v6_.reset(zxdg_surface_v6_get_popup(
      zxdg_surface_v6_wrapper_->zxdg_surface(),
      parent_xdg_surface->zxdg_surface(), positioner.get()));
  if (!zxdg_popup_v6_)
    return false;

  GrabIfPossible(connection_, wayland_window_->parent_window());

  zxdg_popup_v6_add_listener(zxdg_popup_v6_.get(), &zxdg_popup_v6_listener,
                             this);

  wayland_window_->root_surface()->Commit();
  return true;
}

void ZXDGPopupV6WrapperImpl::AckConfigure(uint32_t serial) {
  DCHECK(zxdg_surface_v6_wrapper_);
  zxdg_surface_v6_wrapper_->AckConfigure(serial);
}

bool ZXDGPopupV6WrapperImpl::IsConfigured() {
  DCHECK(zxdg_surface_v6_wrapper_);
  return zxdg_surface_v6_wrapper_->IsConfigured();
}

bool ZXDGPopupV6WrapperImpl::SetBounds(const gfx::Rect& new_bounds) {
  // zxdg_popup_v6 doesn't support repositioning. The client must recreate the
  // objects instead.
  return false;
}

void ZXDGPopupV6WrapperImpl::SetWindowGeometry(const gfx::Rect& bounds) {
  zxdg_surface_v6_set_window_geometry(zxdg_surface_v6_wrapper_->zxdg_surface(),
                                      bounds.x(), bounds.y(), bounds.width(),
                                      bounds.height());
}

void ZXDGPopupV6WrapperImpl::Grab(uint32_t serial) {
  DCHECK(connection_->seat());

  zxdg_popup_v6_grab(zxdg_popup_v6_.get(), connection_->seat()->wl_object(),
                     serial);
}

wl::Object<zxdg_positioner_v6> ZXDGPopupV6WrapperImpl::CreatePositioner(
    WaylandWindow* parent_window) {
  wl::Object<zxdg_positioner_v6> positioner(
      zxdg_shell_v6_create_positioner(connection_->shell_v6()));
  if (!positioner)
    return {};

  gfx::Rect anchor_rect;
  OwnedWindowAnchorPosition anchor_position;
  OwnedWindowAnchorGravity anchor_gravity;
  OwnedWindowConstraintAdjustment constraint_adjustment;
  FillAnchorData(params_, &anchor_rect, &anchor_position, &anchor_gravity,
                 &constraint_adjustment);

  zxdg_positioner_v6_set_anchor_rect(positioner.get(), anchor_rect.x(),
                                     anchor_rect.y(), anchor_rect.width(),
                                     anchor_rect.height());
  zxdg_positioner_v6_set_size(positioner.get(), params_.bounds.width(),
                              params_.bounds.height());
  zxdg_positioner_v6_set_anchor(positioner.get(),
                                TranslateAnchor(anchor_position));
  zxdg_positioner_v6_set_gravity(positioner.get(),
                                 TranslateGravity(anchor_gravity));
  zxdg_positioner_v6_set_constraint_adjustment(
      positioner.get(), TranslateConstraintAdjustment(constraint_adjustment));
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
