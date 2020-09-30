// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TOPLEVEL_WINDOW_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TOPLEVEL_WINDOW_H_

#include "build/chromeos_buildflags.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/platform_window/extensions/wayland_extension.h"
#include "ui/platform_window/wm/wm_drag_handler.h"
#include "ui/platform_window/wm/wm_move_loop_handler.h"
#include "ui/platform_window/wm/wm_move_resize_handler.h"

namespace ui {

class ShellSurfaceWrapper;

class WaylandToplevelWindow : public WaylandWindow,
                              public WmMoveResizeHandler,
                              public WmDragHandler,
                              public WmMoveLoopHandler,
                              public WaylandExtension {
 public:
  WaylandToplevelWindow(PlatformWindowDelegate* delegate,
                        WaylandConnection* connection);
  WaylandToplevelWindow(const WaylandToplevelWindow&) = delete;
  WaylandToplevelWindow& operator=(const WaylandToplevelWindow&) = delete;
  ~WaylandToplevelWindow() override;

  ShellSurfaceWrapper* shell_surface() const { return shell_surface_.get(); }

  // Apply the bounds specified in the most recent configure event. This should
  // be called after processing all pending events in the wayland connection.
  void ApplyPendingBounds();

  // WmMoveResizeHandler
  void DispatchHostWindowDragMovement(
      int hittest,
      const gfx::Point& pointer_location_in_px) override;

  // WmDragHandler
  bool StartDrag(const ui::OSExchangeData& data,
                 int operation,
                 gfx::NativeCursor cursor,
                 bool can_grab_pointer,
                 WmDragHandler::Delegate* delegate) override;
  void CancelDrag() override;

  // PlatformWindow
  void Show(bool inactive) override;
  void Hide() override;
  bool IsVisible() const override;
  void SetTitle(const base::string16& title) override;
  void ToggleFullscreen() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  PlatformWindowState GetPlatformWindowState() const override;
  void SizeConstraintsChanged() override;
  std::string GetWindowUniqueId() const override;

 private:
  // WaylandWindow overrides:
  void HandleSurfaceConfigure(int32_t widht,
                              int32_t height,
                              bool is_maximized,
                              bool is_fullscreen,
                              bool is_activated) override;
  void OnDragEnter(const gfx::PointF& point,
                   std::unique_ptr<OSExchangeData> data,
                   int operation) override;
  int OnDragMotion(const gfx::PointF& point, int operation) override;
  void OnDragDrop(std::unique_ptr<OSExchangeData> data) override;
  void OnDragLeave() override;
  void OnDragSessionClose(uint32_t dnd_action) override;
  bool OnInitialize(PlatformWindowInitProperties properties) override;
  bool IsActive() const override;

  // WmMoveLoopHandler:
  bool RunMoveLoop(const gfx::Vector2d& drag_offset) override;
  void EndMoveLoop() override;

  // WaylandExtension:
  void StartWindowDraggingSessionIfNeeded() override;

  void TriggerStateChanges();
  void SetWindowState(PlatformWindowState state);

  // Creates a surface window, which is visible as a main window.
  bool CreateShellSurface();

  WmMoveResizeHandler* AsWmMoveResizeHandler();

  // Propagates the |min_size_| and |max_size_| to the ShellSurface.
  void SetSizeConstraints();

  void SetOrResetRestoredBounds();

  // Initializes the aura-shell EXO extension, if available.
  void InitializeAuraShell();

  // Wrappers around shell surface.
  std::unique_ptr<ShellSurfaceWrapper> shell_surface_;

  WmDragHandler::Delegate* drag_handler_delegate_ = nullptr;

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

#if BUILDFLAG(IS_LACROS)
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

  // Title of the ShellSurface.
  base::string16 window_title_;

  // Max and min sizes of the WaylandToplevelWindow window.
  base::Optional<gfx::Size> min_size_;
  base::Optional<gfx::Size> max_size_;

  base::OnceClosure drag_loop_quit_closure_;

  wl::Object<zaura_surface> aura_surface_;

  base::WeakPtrFactory<WaylandToplevelWindow> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TOPLEVEL_WINDOW_H_
