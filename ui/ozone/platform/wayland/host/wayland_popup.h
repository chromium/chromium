// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_POPUP_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_POPUP_H_

#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace views::corewm {
enum class TooltipTrigger;
}  // namespace views::corewm

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

  // WaylandWindow overrides:

  // Configure related:
  void HandleSurfaceConfigure(uint32_t serial) override;
  void HandlePopupConfigure(const gfx::Rect& bounds) override;
  bool IsSurfaceConfigured() override;
  void AckConfigure(uint32_t serial) override;
  void UpdateVisualSize(const gfx::Size& size_px) override;
  void ApplyPendingBounds() override;

  void OnCloseRequest() override;
  bool OnInitialize(PlatformWindowInitProperties properties) override;
  WaylandPopup* AsWaylandPopup() override;
  void SetWindowGeometry(gfx::Size size_dip) override;
  void UpdateWindowMask() override;
  void PropagateBufferScale(float new_scale) override;
  void ShowTooltip(const std::u16string& text,
                   const gfx::Point& position,
                   const PlatformWindowTooltipTrigger trigger,
                   const base::TimeDelta show_delay,
                   const base::TimeDelta hide_delay) override;
  void HideTooltip() override;

  // PlatformWindow
  void Show(bool inactive) override;
  void Hide() override;
  bool IsVisible() const override;
  void SetBoundsInDIP(const gfx::Rect& bounds) override;

 private:
  // zaura_surface listeners
  static void DeskChanged(void* data, zaura_surface* surface, int32_t state) {}
  static void TooltipShown(void* data,
                           zaura_surface* surface,
                           const char* text,
                           int32_t x,
                           int32_t y,
                           int32_t width,
                           int32_t height);
  static void TooltipHidden(void* data, zaura_surface* surface);

  // Creates a popup window, which is visible as a menu window.
  bool CreateShellPopup();

  // Decorates the surface, which requires custom extensions based on exo.
  void UpdateDecoration();

  // Returns bounds with origin relative to parent window's origin.
  gfx::Rect AdjustPopupWindowPosition();

  // Wrappers around xdg v5 and xdg v6 objects. WaylandPopup doesn't
  // know anything about the version.
  std::unique_ptr<ShellPopupWrapper> shell_popup_;

  // Set to true if the surface is decorated via aura_popup -- the custom exo
  // extension to xdg_popup.
  bool decorated_via_aura_popup_ = false;

  PlatformWindowShadowType shadow_type_ = PlatformWindowShadowType::kNone;

  // Helps to avoid reposition itself if HandlePopupConfigure was called, which
  // resulted in calling SetBounds.
  bool wayland_sets_bounds_ = false;

  // If WaylandPopup has been moved, schedule redraw as the client of the
  // Ozone/Wayland may not do so. Otherwise, a new state (if bounds has been
  // changed) won't be applied.
  bool schedule_redraw_ = false;

  // The last buffer scale sent to the wayland server.
  absl::optional<float> last_sent_buffer_scale_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_POPUP_H_
