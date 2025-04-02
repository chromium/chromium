// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_POPUP_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_POPUP_H_

#include "base/memory/weak_ptr.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

class WaylandConnection;
class XdgPopup;

class WaylandPopup final : public WaylandWindow {
 public:
  WaylandPopup(PlatformWindowDelegate* delegate,
               WaylandConnection* connection,
               WaylandWindow* parent);

  WaylandPopup(const WaylandPopup&) = delete;
  WaylandPopup& operator=(const WaylandPopup&) = delete;

  ~WaylandPopup() override;

  XdgPopup* xdg_popup() { return xdg_popup_.get(); }

  // Configure related:
  void HandleSurfaceConfigure(uint32_t serial) override;
  void HandlePopupConfigure(const gfx::Rect& bounds) override;
  void OnSequencePoint(int64_t seq) override;
  bool IsSurfaceConfigured() override;
  void AckConfigure(uint32_t serial) override;

  void OnCloseRequest() override;
  bool OnInitialize(PlatformWindowInitProperties properties,
                    PlatformWindowDelegate::State* state) override;
  WaylandPopup* AsWaylandPopup() override;
  void SetWindowGeometry(const PlatformWindowDelegate::State& state) override;
  void UpdateWindowMask() override;
  base::WeakPtr<WaylandWindow> AsWeakPtr() override;

  // PlatformWindow
  void Show(bool inactive) override;
  void Hide() override;
  bool IsVisible() const override;
  void SetBoundsInDIP(const gfx::Rect& bounds) override;

 private:
  // Creates a popup window, which is visible as a menu window.
  bool CreateShellPopup();

  // Decorates the surface, which requires custom extensions based on exo.
  void UpdateDecoration();

  // Returns bounds with origin relative to parent window's origin.
  gfx::Rect AdjustPopupWindowPosition();

  std::unique_ptr<XdgPopup> xdg_popup_;

  PlatformWindowShadowType shadow_type_ = PlatformWindowShadowType::kNone;

  // If WaylandPopup has been moved, schedule redraw as the client of the
  // Ozone/Wayland may not do so. Otherwise, a new state (if bounds has been
  // changed) won't be applied.
  bool schedule_redraw_ = false;

  base::WeakPtrFactory<WaylandPopup> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_POPUP_H_
