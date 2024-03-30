// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_SHELL_POPUP_WRAPPER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_SHELL_POPUP_WRAPPER_H_

#include <cstdint>
#include <optional>

#include "ui/base/owned_window_anchor.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

class WaylandConnection;
class XDGPopupWrapperImpl;

struct ShellPopupParams {
  ShellPopupParams();
  ShellPopupParams(const ShellPopupParams&);
  ShellPopupParams& operator=(const ShellPopupParams&);
  ~ShellPopupParams();

  gfx::Rect bounds;
  // This parameter is temporarily optional. Later, when all the clients
  // start to pass these parameters, std::optional type will be removed.
  std::optional<OwnedWindowAnchor> anchor;
};

// Wrapper interface for shell popups.
//
// This is one of three wrapper classes: Shell{Surface,Toplevel,Popup}Wrapper.
// It has the only sub-class in Chromium, but should not be removed because it
// eases downstream implementations.
// See https://crbug.com/1402672
//
// Allows WaylandPopup to do stuff specific to popups, such as anchoring the
// window and grabbing the pointer.
class ShellPopupWrapper {
 public:
  virtual ~ShellPopupWrapper() = default;

  // Initializes the popup surface.
  virtual bool Initialize(const ShellPopupParams& params) = 0;

  // Sends acknowledge configure event back to wayland.
  virtual void AckConfigure(uint32_t serial) = 0;

  // Tells if the surface has been AckConfigured at least once.
  virtual bool IsConfigured() = 0;

  // Changes bounds of the popup window. If changing bounds is not supported,
  // false is returned and the client should recreate the shell popup instead
  // if it still wants to reposition the popup.
  virtual bool SetBounds(const gfx::Rect& new_bounds) = 0;

  // Sets and gets the window geometry.
  virtual void SetWindowGeometry(const gfx::Rect& bounds) = 0;
  // Fills anchor data either from params.anchor or with default anchor
  // parameters if params.anchor is empty.
  void FillAnchorData(const ShellPopupParams& params,
                      gfx::Rect* anchor_rect,
                      OwnedWindowAnchorPosition* anchor_position,
                      OwnedWindowAnchorGravity* anchor_gravity,
                      OwnedWindowConstraintAdjustment* constraints) const;

  // Whether the protocol supports surface decoration.
  virtual bool SupportsDecoration() = 0;

  // Must only be called if SupportsDecoration() returns true.
  // Decorates the surface with a drop shadow.
  virtual void Decorate(ui::PlatformWindowShadowType shadow_type) = 0;

  // Sets the scale factor for the next commit. Scale factor persists until a
  // new one is set.
  virtual void SetScaleFactor(float scale_factor) = 0;

  // Casts `this` to XDGPopupWrapperImpl, if it is of that type.
  virtual XDGPopupWrapperImpl* AsXDGPopupWrapper();

  bool has_grab() const { return has_grab_; }

 protected:
  // Asks the compositor to take explicit-grab for this popup.
  virtual void Grab(uint32_t serial) = 0;

  // Returns the serial value for a popup grab, if there is one available.
  // `parent_shell_popup_has_grab` has value if this popup is dangling off
  // another shell_popup, true if that popup has grab.
  void GrabIfPossible(WaylandConnection* connection,
                      std::optional<bool> parent_shell_popup_has_grab);

 private:
  // Tells if explicit grab was taken for this popup. As per
  // https://wayland.app/protocols/xdg-shell#xdg_popup:request:grab
  bool has_grab_ = false;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_SHELL_POPUP_WRAPPER_H_
