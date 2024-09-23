// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_POPUP_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_POPUP_H_

#include "base/memory/weak_ptr.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace views::corewm {
enum class TooltipTrigger;
}  // namespace views::corewm

namespace ui {

class WaylandConnection;
class ShellPopupWrapper;

class WaylandPopup final : public WaylandWindow {
 public:
  WaylandPopup(PlatformWindowDelegate* delegate,
               WaylandConnection* connection,
               WaylandWindow* parent);

  WaylandPopup(const WaylandPopup&) = delete;
  WaylandPopup& operator=(const WaylandPopup&) = delete;

  ~WaylandPopup() override;

  ShellPopupWrapper* shell_popup() { return shell_popup_.get(); }

  // WaylandWindow overrides:
  void TooltipShown(const char* text,
                    int32_t x,
                    int32_t y,
                    int32_t width,
                    int32_t height) override;
  void TooltipHidden() override;

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
  void PropagateBufferScale(float new_scale) override;
  base::WeakPtr<WaylandWindow> AsWeakPtr() override;
  void ShowTooltip(const std::u16string& text,
                   const gfx::Point& position,
                   const PlatformWindowTooltipTrigger trigger,
                   const base::TimeDelta show_delay,
                   const base::TimeDelta hide_delay) override;
  void HideTooltip() override;
  bool IsScreenCoordinatesEnabled() const override;

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

  // Wrappers around xdg v5 and xdg v6 objects. WaylandPopup doesn't
  // know anything about the version.
  std::unique_ptr<ShellPopupWrapper> shell_popup_;

  // Set to true if the surface is decorated via aura_popup -- the custom exo
  // extension to xdg_popup.
  bool decorated_via_aura_popup_ = false;

  PlatformWindowShadowType shadow_type_ = PlatformWindowShadowType::kNone;

  // If WaylandPopup has been moved, schedule redraw as the client of the
  // Ozone/Wayland may not do so. Otherwise, a new state (if bounds has been
  // changed) won't be applied.
  bool schedule_redraw_ = false;

  // The last buffer scale sent to the wayland server.
  std::optional<float> last_sent_buffer_scale_;

  base::WeakPtrFactory<WaylandPopup> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_POPUP_H_
