// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_POPUP_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_POPUP_H_

#include <cstdint>
#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "ui/base/owned_window_anchor.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/xdg_surface.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

class WaylandConnection;
class WaylandWindow;

// Wrapper class for xdg-shell's xdg_popup protocol objects. Used by
// WaylandPopup to do popup-specific stuff, such as anchoring the window and
// grabbing the pointer.
//
// Note that XDG does not allow {0, 0} geometries in its protocol, but this is
// allowed in Chrome. All {0, 0} bounds will be resized to {1, 1}.
class XdgPopup {
 public:
  struct InitParams {
    gfx::Rect bounds;
    // This parameter is temporarily optional. Later, when all the clients
    // start to pass these parameters, std::optional type will be removed.
    std::optional<OwnedWindowAnchor> anchor;
  };

  explicit XdgPopup(std::unique_ptr<XdgSurface> xdg_surface);
  XdgPopup(const XdgPopup&) = delete;
  XdgPopup& operator=(const XdgPopup&) = delete;
  ~XdgPopup();

  bool Initialize(const InitParams& params);
  void AckConfigure(uint32_t serial);
  bool IsConfigured();
  bool SetBounds(const gfx::Rect& new_bounds);
  void SetWindowGeometry(const gfx::Rect& bounds);
  void Grab(uint32_t serial);
  bool has_grab() const { return has_grab_; }

  // Fills anchor data either from params.anchor or with default anchor
  // parameters if params.anchor is empty.
  void FillAnchorData(const InitParams& params,
                      gfx::Rect* anchor_rect,
                      OwnedWindowAnchorPosition* anchor_position,
                      OwnedWindowAnchorGravity* anchor_gravity,
                      OwnedWindowConstraintAdjustment* constraints) const;

  struct xdg_surface* xdg_surface() const { return xdg_surface_->wl_object(); }

 private:
  wl::Object<xdg_positioner> CreatePositioner();

  // Returns the serial value for a popup grab, if there is one available.
  // `parent_shell_popup_has_grab` has value if this popup is dangling off
  // another shell_popup, true if that popup has grab.
  void GrabIfPossible(WaylandConnection* connection,
                      std::optional<bool> parent_shell_popup_has_grab);

  // xdg_popup_listener callbacks:
  static void OnConfigure(void* data,
                          xdg_popup* popup,
                          int32_t x,
                          int32_t y,
                          int32_t width,
                          int32_t height);
  static void OnPopupDone(void* data, xdg_popup* popup);
  static void OnRepositioned(void* data, xdg_popup* popup, uint32_t token);

  WaylandConnection* connection() const { return xdg_surface_->connection_; }
  WaylandWindow* window() const { return xdg_surface_->wayland_window_; }

  // Ground surface for this popup.
  const std::unique_ptr<XdgSurface> xdg_surface_;

  // XDG Shell Stable object.
  wl::Object<xdg_popup> xdg_popup_;

  // Tells if explicit grab was taken for this popup. As per
  // https://wayland.app/protocols/xdg-shell#xdg_popup:request:grab
  bool has_grab_ = false;

  // Parameters that help to configure this popup.
  InitParams params_;

  uint32_t next_reposition_token_ = 1;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_POPUP_H_
