// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TOPLEVEL_WINDOW_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TOPLEVEL_WINDOW_H_

#include "build/chromeos_buildflags.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/platform_window/extensions/desk_extension.h"
#include "ui/platform_window/extensions/pinned_mode_extension.h"
#include "ui/platform_window/extensions/wayland_extension.h"
#include "ui/platform_window/extensions/workspace_extension.h"
#include "ui/platform_window/extensions/workspace_extension_delegate.h"
#include "ui/platform_window/wm/wm_move_loop_handler.h"
#include "ui/platform_window/wm/wm_move_resize_handler.h"

namespace ui {

class GtkSurface1;
class ShellToplevelWrapper;

class WaylandToplevelWindow : public WaylandWindow,
                              public WmMoveResizeHandler,
                              public WmMoveLoopHandler,
                              public WaylandExtension,
                              public WorkspaceExtension,
                              public DeskExtension,
                              public PinnedModeExtension {
 public:
  WaylandToplevelWindow(PlatformWindowDelegate* delegate,
                        WaylandConnection* connection);
  WaylandToplevelWindow(const WaylandToplevelWindow&) = delete;
  WaylandToplevelWindow& operator=(const WaylandToplevelWindow&) = delete;
  ~WaylandToplevelWindow() override;

  ShellToplevelWrapper* shell_toplevel() const { return shell_toplevel_.get(); }

  // Apply the bounds specified in the most recent configure event. This should
  // be called after processing all pending events in the wayland connection.
  void ApplyPendingBounds() override;

  // WmMoveResizeHandler
  void DispatchHostWindowDragMovement(
      int hittest,
      const gfx::Point& pointer_location_in_px) override;

  // PlatformWindow:
  void Show(bool inactive) override;
  void Hide() override;
  bool IsVisible() const override;
  void SetTitle(const std::u16string& title) override;
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
  bool ShouldUpdateWindowShape() const override;
  bool CanSetDecorationInsets() const override;
  void SetOpaqueRegion(const std::vector<gfx::Rect>* region_px) override;
  void SetInputRegion(const gfx::Rect* region_px) override;
  void SetAspectRatio(const gfx::SizeF& aspect_ratio) override;
  void SetBounds(const gfx::Rect& bounds) override;

  // Sets the window's origin.
  void SetOrigin(const gfx::Point& origin);

  // WaylandWindow overrides:
  absl::optional<std::vector<gfx::Rect>> GetWindowShape() const override;

  bool screen_coordinates_enabled() const {
    return screen_coordinates_enabled_;
  }

  // Client-side decorations on Wayland take some portion of the window surface,
  // and when they are turned on or off, the window geometry is changed.  That
  // happens only once at the moment of switching the decoration mode, and has
  // no further impact on the user experience, but the initial geometry of a
  // top-level window is different on Wayland if compared to other platforms,
  // which affects certain tests.
  static void AllowSettingDecorationInsetsForTest(bool allow);

 private:
  // WaylandWindow overrides:
  void UpdateWindowScale(bool update_bounds) override;
  void HandleToplevelConfigure(int32_t width,
                               int32_t height,
                               bool is_maximized,
                               bool is_fullscreen,
                               bool is_activated) override;
  void HandleAuraToplevelConfigure(int32_t x,
                                   int32_t y,
                                   int32_t width,
                                   int32_t height,
                                   bool is_maximized,
                                   bool is_fullscreen,
                                   bool is_activated) override;
  void HandleSurfaceConfigure(uint32_t serial) override;
  void UpdateVisualSize(const gfx::Size& size_px, float scale_factor) override;
  bool OnInitialize(PlatformWindowInitProperties properties) override;
  bool IsActive() const override;
  bool IsSurfaceConfigured() override;
  void SetWindowGeometry(gfx::Rect bounds) override;
  void AckConfigure(uint32_t serial) override;
  void UpdateDecorations() override;

  // PlatformWindow overrides:
  bool IsClientControlledWindowMovementSupported() const override;

  // WmDragHandler overrides:
  bool ShouldReleaseCaptureForDrag(ui::OSExchangeData* data) const override;

  // zaura_surface listeners
  static void OcclusionChanged(void* data,
                               zaura_surface* surface,
                               wl_fixed_t occlusion_fraction,
                               uint32_t occlusion_reason);
  static void LockFrame(void* data, zaura_surface* surface);
  static void UnlockFrame(void* data, zaura_surface* surface);
  static void OcclusionStateChanged(void* data,
                                    zaura_surface* surface,
                                    uint32_t mode);
  static void DeskChanged(void* data, zaura_surface* surface, int state);
  static void StartThrottle(void* data, zaura_surface* surface);
  static void EndThrottle(void* data, zaura_surface* surface);

  // Calls UpdateWindowShape, set_input_region and set_opaque_region
  // for this toplevel window.
  void UpdateWindowMask() override;
  // Update the window shape using the window mask of PlatformWindowDelegate.
  void UpdateWindowShape() override;

  // WmMoveLoopHandler:
  bool RunMoveLoop(const gfx::Vector2d& drag_offset) override;
  void EndMoveLoop() override;

  // WaylandExtension:
  void StartWindowDraggingSessionIfNeeded(bool allow_system_drag) override;
  void SetImmersiveFullscreenStatus(bool status) override;
  void ShowSnapPreview(WaylandWindowSnapDirection snap,
                       bool allow_haptic_feedback) override;
  void CommitSnap(WaylandWindowSnapDirection snap) override;
  void SetCanGoBack(bool value) override;
  void SetPip() override;
  bool SupportsPointerLock() override;
  void LockPointer(bool enabled) override;
  void Lock(WaylandOrientationLockType lock_Type) override;
  void Unlock() override;
  bool GetTabletMode() override;

  // DeskExtension:
  int GetNumberOfDesks() const override;
  int GetActiveDeskIndex() const override;
  std::u16string GetDeskName(int index) const override;
  void SendToDeskAtIndex(int index) override;

  // WorkspaceExtension:
  std::string GetWorkspace() const override;
  void SetVisibleOnAllWorkspaces(bool always_visible) override;
  bool IsVisibleOnAllWorkspaces() const override;
  void SetWorkspaceExtensionDelegate(
      WorkspaceExtensionDelegate* delegate) override;

  // PinnedModeExtension:
  void Pin(bool trusted) const override;
  void Unpin() const override;

  void TriggerStateChanges();
  void SetWindowState(PlatformWindowState state);

  // Creates a surface window, which is visible as a main window.
  bool CreateShellToplevel();

  WmMoveResizeHandler* AsWmMoveResizeHandler();

  // Propagates the |min_size_| and |max_size_| to the ShellToplevel.
  void SetSizeConstraints();

  // If current state is not PlatformWindowState::kNormal, stores the current
  // bounds into restored_bounds_px_ so that they can be restored when the
  // window gets back to normal state.  Otherwise, resets the restored bounds.
  void SetOrResetRestoredBounds();

  // Initializes additional shell integration, if the appropriate interfaces are
  // available.
  void SetUpShellIntegration();

  // Sets decoration mode for a window.
  void OnDecorationModeChanged();

  // Called when frame is locked to normal state or unlocked from
  // previously locked state.
  void OnFrameLockingChanged(bool lock);

  // Called when the occlusion state is updated.
  void OnOcclusionStateChanged(PlatformWindowOcclusionState occlusion_state);

  // Called when a window is moved to another desk or assigned to
  // all desks state.
  void OnDeskChanged(int state);

  // Sets |workspace_| to |aura_surface_|.
  // This must be called in SetUpShellIntegration().
  void SetInitialWorkspace();

  // Wrappers around shell surface.
  std::unique_ptr<ShellToplevelWrapper> shell_toplevel_;

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
  // PlatformWindowInitProperties::wm_class_name. This is used by Wayland
  // compositor to identify the app, unite it's windows into the same stack of
  // windows and find *.desktop file to set various preferences including icons.
  std::string app_id_;
#endif

  // Title of the ShellToplevel.
  std::u16string window_title_;

  // Max and min sizes of the WaylandToplevelWindow window.
  absl::optional<gfx::Size> min_size_;
  absl::optional<gfx::Size> max_size_;

  wl::Object<zaura_surface> aura_surface_;
  // |gtk_surface1_| is the optional GTK surface that provides better
  // integration with the desktop shell.
  std::unique_ptr<GtkSurface1> gtk_surface1_;

  // When use_native_frame is false, client-side decoration is set,
  // e.g. lacros-browser.
  // When use_native_frame is true, server-side decoration is set,
  // e.g. lacros-taskmanager.
  bool use_native_frame_ = false;

  absl::optional<std::vector<gfx::Rect>> window_shape_in_dips_;

  absl::optional<gfx::Rect> input_region_px_;

  // Tracks how many the window show state requests by made by the Browser
  // are currently being processed by the Wayland Compositor. In practice,
  // each individual increment corresponds to an explicit window show state
  // change request, and gets a response by the Compositor.
  //
  // This mechanism allows Ozone/Wayland to filter out notifying the delegate
  // (PlatformWindowDelegate) more than once, for the same window show state
  // change.
  uint32_t requested_window_show_state_count_ = 0;
  // Prevents the window geometry from being changed during transitions of the
  // window state.
  //
  // Due to expectations of the higher levels, when the window changes its
  // state, the DWTH is notified about the state change before the one actually
  // happens, see TriggerStateChanges().  However, one of consequences of the
  // DWTH being notified is that it wants to update the decoration insets, which
  // implies updating the window geometry.  This flag is used to skip updating
  // the geometry until new window bounds are applied.
  //
  // See https://crbug.com/1223005
  bool state_change_in_transit_ = false;
  // Some use cases such as changing the theme need to update the window
  // geometry without changing its configuration.  They should set this flag.
  // It will result in sending the updated geometry in the next frame update.
  //
  // See https://crbug.com/1223005
  bool set_geometry_on_next_frame_ = false;

  // The desk index for the window.
  // If |workspace_| is -1, window is visible on all workspaces.
  absl::optional<int> workspace_ = absl::nullopt;

  // True when screen coordinates is enabled.
  bool screen_coordinates_enabled_;

  WorkspaceExtensionDelegate* workspace_extension_delegate_ = nullptr;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TOPLEVEL_WINDOW_H_
