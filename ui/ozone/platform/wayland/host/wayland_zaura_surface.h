// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_SURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_SURFACE_H_

#include <wayland-util.h>

#include <components/exo/wayland/protocol/aura-shell-client-protocol.h>

#include "base/memory/weak_ptr.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace gfx {
class Point;
}  // namespace gfx

namespace ui {

class WaylandConnection;

PlatformWindowOcclusionState
WaylandOcclusionStateToPlatformWindowOcclusionState(uint32_t mode);

// Wrapper class for an instance of the zaura_surface client-side wayland
// object.
class WaylandZAuraSurface {
 public:
  // Delegates are forwarded wayland server events for a given zaura_surface
  // instance.
  class Delegate {
   public:
    virtual void OcclusionChanged(wl_fixed_t occlusion_fraction,
                                  uint32_t occlusion_reason) {}
    virtual void LockFrame() {}
    virtual void UnlockFrame() {}
    virtual void OcclusionStateChanged(
        PlatformWindowOcclusionState occlusion_state) {}
    virtual void DeskChanged(int state) {}
    virtual void StartThrottle() {}
    virtual void EndThrottle() {}
    virtual void TooltipShown(const char* text,
                              int32_t x,
                              int32_t y,
                              int32_t width,
                              int32_t height) {}
    virtual void TooltipHidden() {}
  };

  WaylandZAuraSurface(zaura_shell* zaura_shell,
                      wl_surface* wl_surface,
                      WaylandConnection* connection);
  WaylandZAuraSurface(const WaylandZAuraSurface&) = delete;
  WaylandZAuraSurface& operator=(const WaylandZAuraSurface&) = delete;
  ~WaylandZAuraSurface();

  // The following methods return true if the bound zaura_surface version
  // supports the request and it was successfully sent to the server.
  bool SetFrame(zaura_surface_frame_type type);
  bool SetOcclusionTracking();
  bool Activate();
  bool SetFullscreenMode(zaura_surface_fullscreen_mode mode);
  bool SetServerStartResize();
  bool IntentToSnap(zaura_surface_snap_direction snap_direction);
  bool SetSnapLeft();
  bool SetSnapRight();
  bool UnsetSnap();
  bool SetCanGoBack();
  bool UnsetCanGoBack();
  bool SetPip();
  bool SetAspectRatio(float width, float height);
  bool MoveToDesk(int index);
  bool SetInitialWorkspace(int workspace);
  bool SetPin(bool trusted);
  bool UnsetPin();
  bool ShowTooltip(const std::u16string& text,
                   const gfx::Point& position,
                   zaura_surface_tooltip_trigger trigger,
                   const base::TimeDelta& show_delay,
                   const base::TimeDelta& hide_delay);
  bool HideTooltip();

  // Helpers to check whether a given request is supported by the currently
  // bound version.
  bool SupportsActivate() const;
  bool SupportsSetServerStartResize() const;
  bool SupportsUnsetSnap() const;

  zaura_surface* wl_object() { return zaura_surface_.get(); }

  void set_delegate(base::WeakPtr<Delegate> delegate) { delegate_ = delegate; }

 private:
  // Returns true if `zaura_surface_` version is equal or newer than `version`.
  bool IsSupported(uint32_t version) const;

  // zaura_surface_listeners callbacks:
  static void OnOcclusionChanged(void* data,
                                 zaura_surface* surface,
                                 wl_fixed_t occlusion_fraction,
                                 uint32_t occlusion_reason);
  static void OnLockFrameNormal(void* data, zaura_surface* surface);
  static void OnUnlockFrameNormal(void* data, zaura_surface* surface);
  static void OnOcclusionStateChanged(void* data,
                                      zaura_surface* surface,
                                      uint32_t mode);
  static void OnDeskChanged(void* data, zaura_surface* surface, int state);
  static void OnStartThrottle(void* data, zaura_surface* surface);
  static void OnEndThrottle(void* data, zaura_surface* surface);
  static void OnTooltipShown(void* data,
                             zaura_surface* surface,
                             const char* text,
                             int32_t x,
                             int32_t y,
                             int32_t width,
                             int32_t height);
  static void OnTooltipHidden(void* data, zaura_surface* surface);

  // Use a weak ptr as lifetime guarantees are not well defined. E.g. this is
  // typically a WaylandWindow subclass that may be destroyed independently of
  // this surface.
  base::WeakPtr<Delegate> delegate_;

  // The client-side resource handle for the zaura_shell object.
  wl::Object<zaura_surface> zaura_surface_;

  const raw_ptr<WaylandConnection> connection_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_ZAURA_SURFACE_H_
