// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUBBLE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUBBLE_H_

#include "base/memory/weak_ptr.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

// A WaylandWindow implementation to show kBubble and kPopup widgets.
// Implemented using a wl_subsurface object.
class WaylandBubble final : public WaylandWindow {
 public:
  WaylandBubble(PlatformWindowDelegate* delegate,
                WaylandConnection* connection,
                WaylandWindow* parent);
  WaylandBubble(const WaylandBubble&) = delete;
  WaylandBubble& operator=(const WaylandBubble&) = delete;
  ~WaylandBubble() override;

  bool activatable() { return activatable_; }

  // PlatformWindow overrides:
  void Show(bool inactive) override;
  void Hide() override;
  bool IsVisible() const override;
  void SetBoundsInDIP(const gfx::Rect& bounds) override;
  void SetInputRegion(std::optional<std::vector<gfx::Rect>> region_px) override;
  void Activate() override;
  void Deactivate() override;
  void ShowTooltip(const std::u16string& text,
                   const gfx::Point& position,
                   const PlatformWindowTooltipTrigger trigger,
                   const base::TimeDelta show_delay,
                   const base::TimeDelta hide_delay) override;
  void HideTooltip() override;

  // WaylandWindow overrides:
  void UpdateWindowScale(bool update_bounds) override;
  void PropagateBufferScale(float new_scale) override {}
  void OnSequencePoint(int64_t seq) override;
  // TODO(crbug.com/329145822): this needs to apply the offset that is requested
  // by SetBoundsInDIP.
  void AckConfigure(uint32_t serial) override {}
  base::WeakPtr<WaylandWindow> AsWeakPtr() override;
  bool IsScreenCoordinatesEnabled() const override;
  bool IsActive() const override;
  WaylandBubble* AsWaylandBubble() override;

 private:
  // WaylandWindow overrides:
  bool OnInitialize(PlatformWindowInitProperties properties,
                    PlatformWindowDelegate::State* state) override;
  bool IsSurfaceConfigured() override;
  void UpdateWindowMask() override;

  // Creates (if necessary) and shows a subsurface window in the parent.
  void AddToParentAsSubsurface();

  void SetSubsurfacePosition();

  wl::Object<wl_subsurface> subsurface_;

  // Copied from Widget::InitParams::activatable, indicates whether this bubble
  // take activation from the parent window.
  bool activatable_ = false;
  // Copied from Widget::InitParams::accept_events, indicates whether this
  // bubble traps inputs.
  bool accept_events_ = true;

  base::WeakPtrFactory<WaylandBubble> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUBBLE_H_
