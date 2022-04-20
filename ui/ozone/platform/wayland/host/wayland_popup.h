// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_POPUP_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_POPUP_H_

#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

class WaylandConnection;
class ShellPopupWrapper;

class WaylandPopup : public WaylandWindow {
 public:
  WaylandPopup(PlatformWindowDelegate* delegate,
               WaylandConnection* connection,
               WaylandWindow* parent);

  WaylandPopup(const WaylandPopup&) = delete;
  WaylandPopup& operator=(const WaylandPopup&) = delete;

  ~WaylandPopup() override;

  ShellPopupWrapper* shell_popup() const { return shell_popup_.get(); }

  // PlatformWindow
  void Show(bool inactive) override;
  void Hide() override;
  bool IsVisible() const override;
  void SetBounds(const gfx::Rect& bounds) override;

 private:
  // WaylandWindow overrides:
  void HandlePopupConfigure(const gfx::Rect& bounds) override;
  void HandleSurfaceConfigure(uint32_t serial) override;
  void OnCloseRequest() override;
  bool OnInitialize(PlatformWindowInitProperties properties) override;
  WaylandPopup* AsWaylandPopup() override;
  bool IsSurfaceConfigured() override;
  void SetWindowGeometry(gfx::Rect bounds) override;
  void AckConfigure(uint32_t serial) override;
  void UpdateVisualSize(const gfx::Size& size_px, float scale_factor) override;
  void ApplyPendingBounds() override;
  void UpdateWindowMask() override;

  // Creates a popup window, which is visible as a menu window.
  bool CreateShellPopup();

  // Initializes the aura-shell surface, in the case aura-shell EXO extension
  // is available.
  void InitializeAuraShellSurface();

  // Returns bounds with origin relative to parent window's origin.
  gfx::Rect AdjustPopupWindowPosition();

  // Wrappers around xdg v5 and xdg v6 objects. WaylandPopup doesn't
  // know anything about the version.
  std::unique_ptr<ShellPopupWrapper> shell_popup_;

  wl::Object<zaura_surface> aura_surface_;

  PlatformWindowShadowType shadow_type_ = PlatformWindowShadowType::kNone;

  // Helps to avoid reposition itself if HandlePopupConfigure was called, which
  // resulted in calling SetBounds.
  bool wayland_sets_bounds_ = false;

  // If WaylandPopup has been moved, schedule redraw as the client of the
  // Ozone/Wayland may not do so. Otherwise, a new state (if bounds has been
  // changed) won't be applied.
  bool schedule_redraw_ = false;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_POPUP_H_
