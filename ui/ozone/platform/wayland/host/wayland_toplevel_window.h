// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TOPLEVEL_WINDOW_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TOPLEVEL_WINDOW_H_

#include "base/containers/circular_deque.h"
#include "build/chromeos_buildflags.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/platform_window/extensions/wayland_extension.h"
#include "ui/platform_window/wm/wm_move_loop_handler.h"
#include "ui/platform_window/wm/wm_move_resize_handler.h"

namespace ui {

class ShellToplevelWrapper;

class WaylandToplevelWindow : public WaylandWindow,
                              public WmMoveResizeHandler,
                              public WmMoveLoopHandler,
                              public WaylandExtension {
 public:
  WaylandToplevelWindow(PlatformWindowDelegate* delegate,
                        WaylandConnection* connection);
  WaylandToplevelWindow(const WaylandToplevelWindow&) = delete;
  WaylandToplevelWindow& operator=(const WaylandToplevelWindow&) = delete;
  ~WaylandToplevelWindow() override;

  ShellToplevelWrapper* shell_toplevel() const { return shell_toplevel_.get(); }

  // Apply the bounds specified in the most recent configure event. This should
  // be called after processing all pending events in the wayland connection.
  void ApplyPendingBounds();

  // WmMoveResizeHandler
  void DispatchHostWindowDragMovement(
      int hittest,
      const gfx::Point& pointer_location_in_px) override;

  // PlatformWindow:
  void Show(bool inactive) override;
  void Hide() override;
  bool IsVisible() const override;
  void SetTitle(const base::string16& title) override;
  void ToggleFullscreen() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  PlatformWindowState GetPlatformWindowState() const override;
  void Activate() override;
  void SizeConstraintsChanged() override;
  std::string GetWindowUniqueId() const override;
  // SetUseNativeFrame and ShouldUseNativeFrame decide on
  // xdg-decoration mode for a window.
  void SetUseNativeFrame(bool use_native_frame) override;
  bool ShouldUseNativeFrame() const override;

  // WaylandWindow overrides:
  base::Optional<std::vector<gfx::Rect>> GetWindowShape() const override;

 private:
  // WaylandWindow overrides:
  void HandleToplevelConfigure(int32_t width,
                               int32_t height,
                               bool is_maximized,
                               bool is_fullscreen,
                               bool is_activated) override;
  void HandleSurfaceConfigure(uint32_t serial) override;
  void UpdateVisualSize(const gfx::Size& size_px) override;
  bool OnInitialize(PlatformWindowInitProperties properties) override;
  bool IsActive() const override;
  // Calls UpdateWindowShape, set_input_region and set_opaque_region
  // for this toplevel window.
  void UpdateWindowMask() override;
  // Update the window shape using the window mask of PlatformWindowDelegate.
  void UpdateWindowShape() override;

  // WmMoveLoopHandler:
  bool RunMoveLoop(const gfx::Vector2d& drag_offset) override;
  void EndMoveLoop() override;

  // WaylandExtension:
  void StartWindowDraggingSessionIfNeeded() override;
  void SetImmersiveFullscreenStatus(bool status) override;
  void ShowSnapPreview(WaylandWindowSnapDirection snap) override;
  void CommitSnap(WaylandWindowSnapDirection snap) override;

  void TriggerStateChanges();
  void SetWindowState(PlatformWindowState state);

  // Creates a surface window, which is visible as a main window.
  bool CreateShellToplevel();

  WmMoveResizeHandler* AsWmMoveResizeHandler();

  // Propagates the |min_size_| and |max_size_| to the ShellToplevel.
  void SetSizeConstraints();

  void SetOrResetRestoredBounds();

  // Initializes the aura-shell surface, in the case aura-shell EXO extension
  // is available.
  void InitializeAuraShellSurface();

  // Sets decoration mode for a window.
  void OnDecorationModeChanged();

  // Wrappers around shell surface.
  std::unique_ptr<ShellToplevelWrapper> shell_toplevel_;

  // These bounds attributes below have suffices that indicate units used.
  // Wayland operates in DIP but the platform operates in physical pixels so
  // our WaylandToplevelWindow is the link that has to translate the units.  See
  // also comments in the implementation.
  //
  // Bounds that will be applied when the window state is finalized.  The window
  // may get several configuration events that update the pending bounds, and
  // only upon finalizing the state is the latest value stored as the current
  // bounds via |ApplyPendingBounds|.  Measured in DIP because updated in the
  // handler that receives DIP from Wayland.
  gfx::Rect pending_bounds_dip_;

  // Contains the current state of the window.
  PlatformWindowState state_;
  // Contains the previous state of the window.
  PlatformWindowState previous_state_;

  bool is_active_ = false;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Unique ID for this window. May be shared over non-Wayland IPC transports
  // (e.g. mojo) to identify the window.
  std::string window_unique_id_;
#else
  // Id of the chromium app passed through
  // PlatformWindowInitProperties::wm_class_class. This is used by Wayland
  // compositor to identify the app, unite it's windows into the same stack of
  // windows and find *.desktop file to set various preferences including icons.
  std::string wm_class_class_;
#endif

  // Title of the ShellToplevel.
  base::string16 window_title_;

  // Max and min sizes of the WaylandToplevelWindow window.
  base::Optional<gfx::Size> min_size_;
  base::Optional<gfx::Size> max_size_;

  wl::Object<zaura_surface> aura_surface_;

  // When use_native_frame is false, client-side decoration is set,
  // e.g. lacros-browser.
  // When use_native_frame is true, server-side decoration is set,
  // e.g. lacros-taskmanager.
  bool use_native_frame_ = false;

  base::Optional<std::vector<gfx::Rect>> window_shape_in_dips_;

  // Pending xdg-shell configures, once this window is drawn to |size_dip|,
  // ack_configure with |serial| will be sent to the Wayland compositor.
  struct PendingConfigure {
    gfx::Size size_dip;
    uint32_t serial;
  };
  base::circular_deque<PendingConfigure> pending_configures_;

  // Tracks how many the window show state requests by made by the Browser
  // are currently being processed by the Wayland Compositor. In practice,
  // each individual increment corresponds to an explicit window show state
  // change request, and gets a response by the Compositor.
  //
  // This mechanism allows Ozone/Wayland to filter out notifying the delegate
  // (PlatformWindowDelegate) more than once, for the same window show state
  // change.
  uint32_t requested_window_show_state_count_ = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TOPLEVEL_WINDOW_H_
