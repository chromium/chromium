// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TOPLEVEL_WINDOW_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TOPLEVEL_WINDOW_H_

#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/platform_window/extensions/desk_extension.h"
#include "ui/platform_window/extensions/pinned_mode_extension.h"
#include "ui/platform_window/extensions/system_modal_extension.h"
#include "ui/platform_window/extensions/wayland_extension.h"
#include "ui/platform_window/extensions/workspace_extension.h"
#include "ui/platform_window/extensions/workspace_extension_delegate.h"
#include "ui/platform_window/wm/wm_move_loop_handler.h"
#include "ui/platform_window/wm/wm_move_resize_handler.h"

namespace views::corewm {
enum class TooltipTrigger;
}  // namespace views::corewm

namespace wl {

// Client-side decorations on Wayland take some portion of the window surface,
// and when they are turned on or off, the window geometry is changed.  That
// happens only once at the moment of switching the decoration mode, and has
// no further impact on the user experience, but the initial geometry of a
// top-level window is different on Wayland if compared to other platforms,
// which affects certain tests.
void AllowClientSideDecorationsForTesting(bool allow);

}  // namespace wl

namespace ui {

class GtkSurface1;
class ShellToplevelWrapper;

class WaylandToplevelWindow : public WaylandWindow,
                              public WmMoveResizeHandler,
                              public WmMoveLoopHandler,
                              public WaylandExtension,
                              public WorkspaceExtension,
                              public DeskExtension,
                              public PinnedModeExtension,
                              public SystemModalExtension {
 public:
  WaylandToplevelWindow(PlatformWindowDelegate* delegate,
                        WaylandConnection* connection);
  WaylandToplevelWindow(const WaylandToplevelWindow&) = delete;
  WaylandToplevelWindow& operator=(const WaylandToplevelWindow&) = delete;
  ~WaylandToplevelWindow() override;

  ShellToplevelWrapper* shell_toplevel() const { return shell_toplevel_.get(); }

  // Sets the window's origin.
  void SetOrigin(const gfx::Point& origin);

  // WaylandWindow overrides:
  void UpdateWindowScale(bool update_bounds) override;

  // Configure related:
  void HandleToplevelConfigure(int32_t width,
                               int32_t height,
                               const WindowStates& window_states) override;
  void HandleAuraToplevelConfigure(int32_t x,
                                   int32_t y,
                                   int32_t width,
                                   int32_t height,
                                   const WindowStates& window_states) override;
  void HandleSurfaceConfigure(uint32_t serial) override;
  void OnSequencePoint(int64_t seq) override;
  bool IsSurfaceConfigured() override;
  void AckConfigure(uint32_t serial) override;

  bool OnInitialize(PlatformWindowInitProperties properties,
                    PlatformWindowDelegate::State* state) override;
  bool IsActive() const override;
  void SetWindowGeometry(gfx::Size size_dip) override;
  bool IsScreenCoordinatesEnabled() const override;
  void ShowTooltip(const std::u16string& text,
                   const gfx::Point& position,
                   const PlatformWindowTooltipTrigger trigger,
                   const base::TimeDelta show_delay,
                   const base::TimeDelta hide_delay) override;
  void HideTooltip() override;
  void PropagateBufferScale(float new_scale) override;

  // WmDragHandler overrides:
  bool ShouldReleaseCaptureForDrag(ui::OSExchangeData* data) const override;

  // WmMoveResizeHandler
  void DispatchHostWindowDragMovement(
      int hittest,
      const gfx::Point& pointer_location_in_px) override;

  // PlatformWindow:
  void Show(bool inactive) override;
  void Hide() override;
  bool IsVisible() const override;
  void SetTitle(const std::u16string& title) override;
  void SetFullscreen(bool fullscreen, int64_t target_display_id) override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  PlatformWindowState GetPlatformWindowState() const override;
  void Activate() override;
  void Deactivate() override;
  void SizeConstraintsChanged() override;
  // `SetZOrderLevel()` must be called on `z_order_` in
  // `SetUpShellIntegration()`.
  void SetZOrderLevel(ZOrderLevel order) override;
  ZOrderLevel GetZOrderLevel() const override;
  std::string GetWindowUniqueId() const override;
  // SetUseNativeFrame and ShouldUseNativeFrame decide on
  // xdg-decoration mode for a window.
  void SetUseNativeFrame(bool use_native_frame) override;
  bool ShouldUseNativeFrame() const override;
  bool ShouldUpdateWindowShape() const override;
  bool CanSetDecorationInsets() const override;
  void SetOpaqueRegion(const std::vector<gfx::Rect>* region_px) override;
  void SetInputRegion(const gfx::Rect* region_px) override;
  bool IsClientControlledWindowMovementSupported() const override;
  void NotifyStartupComplete(const std::string& startup_id) override;
  void SetAspectRatio(const gfx::SizeF& aspect_ratio) override;
  void SetBoundsInPixels(const gfx::Rect& bounds) override;
  void SetBoundsInDIP(const gfx::Rect& bounds) override;

  // WmMoveLoopHandler:
  bool RunMoveLoop(const gfx::Vector2d& drag_offset) override;
  void EndMoveLoop() override;

  // WaylandExtension:
  void StartWindowDraggingSessionIfNeeded(
      ui::mojom::DragEventSource event_source,
      bool allow_system_drag) override;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void SetImmersiveFullscreenStatus(bool status) override;
#endif
  void ShowSnapPreview(WaylandWindowSnapDirection snap,
                       bool allow_haptic_feedback) override;
  void CommitSnap(WaylandWindowSnapDirection snap, float snap_ratio) override;
  void SetCanGoBack(bool value) override;
  void SetPip() override;
  bool SupportsPointerLock() override;
  void LockPointer(bool enabled) override;
  void Lock(WaylandOrientationLockType lock_Type) override;
  void Unlock() override;
  bool GetTabletMode() override;
  void SetFloat(bool value) override;

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
  void Pin(bool trusted) override;
  void Unpin() override;

  // SystemModalExtension:
  void SetSystemModal(bool modal) override;

 private:
  // WaylandWindow protected overrides:
  // Calls UpdateWindowShape, set_input_region and set_opaque_region for this
  // toplevel window.
  void UpdateWindowMask() override;

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
  static void TooltipShown(void* data,
                           zaura_surface* surface,
                           const char* text,
                           int32_t x,
                           int32_t y,
                           int32_t width,
                           int32_t height);
  static void TooltipHidden(void* data, zaura_surface* surface);

  void UpdateSystemModal();

  void TriggerStateChanges();
  void SetWindowState(PlatformWindowState state);

  // Creates a surface window, which is visible as a main window.
  bool CreateShellToplevel();

  WmMoveResizeHandler* AsWmMoveResizeHandler();

  // Propagates the minimum size and maximum size to the ShellToplevel.
  void SetSizeConstraints();

  // If current state is not PlatformWindowState::kNormal, stores the current
  // size into restored_bounds_dip_ so that they can be restored when the
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
  PlatformWindowState state_ = PlatformWindowState::kUnknown;
  // Contains the previous state of the window.
  PlatformWindowState previous_state_ = PlatformWindowState::kUnknown;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // Contains the current state of the tiled edges.
  WindowTiledEdges tiled_state_;
#endif

  bool is_active_ = false;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  bool is_immersive_fullscreen_ = false;

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

  // |gtk_surface1_| is the optional GTK surface that provides better
  // integration with the desktop shell.
  std::unique_ptr<GtkSurface1> gtk_surface1_;

  // When use_native_frame is false, client-side decoration is set,
  // e.g. lacros-browser.
  // When use_native_frame is true, server-side decoration is set,
  // e.g. lacros-taskmanager.
  bool use_native_frame_ = false;

  absl::optional<std::vector<gfx::Rect>> opaque_region_px_;
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

  // Information used by the compositor to restore the window state upon
  // creation.
  int32_t restore_session_id_ = 0;
  absl::optional<int32_t> restore_window_id_ = 0;
  absl::optional<std::string> restore_window_id_source_;

  // Current modal status.
  bool system_modal_ = false;

  // The desk index for the window.
  // If |workspace_| is -1, window is visible on all workspaces.
  absl::optional<int> workspace_ = absl::nullopt;

  // The z order for the window.
  ZOrderLevel z_order_ = ZOrderLevel::kNormal;

  // True when screen coordinates is enabled.
  bool screen_coordinates_enabled_;

  // The last buffer scale sent to the wayland server.
  absl::optional<float> last_sent_buffer_scale_;

  raw_ptr<WorkspaceExtensionDelegate> workspace_extension_delegate_ = nullptr;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TOPLEVEL_WINDOW_H_
