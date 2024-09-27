// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TOPLEVEL_WINDOW_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TOPLEVEL_WINDOW_H_

#include <memory>
#include <optional>
#include <ostream>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
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

namespace gfx {
class RoundedCornersF;
}  // namespace gfx

namespace ui {

class GtkSurface1;
class ShellToplevelWrapper;

class WaylandToplevelWindow : public WaylandWindow,
                              public WmMoveResizeHandler,
                              public WmMoveLoopHandler,
                              public WaylandToplevelExtension,
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
  void LockFrame() override;
  void UnlockFrame() override;
  void OcclusionStateChanged(
      PlatformWindowOcclusionState occlusion_state) override;
  void DeskChanged(int state) override;
  void StartThrottle() override;
  void EndThrottle() override;
  void TooltipShown(const char* text,
                    int32_t x,
                    int32_t y,
                    int32_t width,
                    int32_t height) override;
  void TooltipHidden() override;
  WaylandToplevelWindow* AsWaylandToplevelWindow() override;

  // Configure related:
  void HandleToplevelConfigure(int32_t width,
                               int32_t height,
                               const WindowStates& window_states) override;
  void HandleToplevelConfigureWithOrigin(
      int32_t x,
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
  void SetWindowGeometry(const PlatformWindowDelegate::State& state) override;
  bool IsScreenCoordinatesEnabled() const override;
  bool SupportsConfigureMinimizedState() const override;
  bool SupportsConfigurePinnedState() const override;
  void ShowTooltip(const std::u16string& text,
                   const gfx::Point& position,
                   const PlatformWindowTooltipTrigger trigger,
                   const base::TimeDelta show_delay,
                   const base::TimeDelta hide_delay) override;
  void HideTooltip() override;
  void PropagateBufferScale(float new_scale) override;
  base::WeakPtr<WaylandWindow> AsWeakPtr() override;
  void OnRotateFocus(uint32_t serial, uint32_t direction, bool restart);
  void OnOverviewChange(uint32_t in_overview_as_int);

  // WmDragHandler:
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
  void Activate() override;
  void Deactivate() override;
  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon) override;
  void SizeConstraintsChanged() override;
  // `SetZOrderLevel()` must be called on `z_order_` in
  // `SetUpShellIntegration()`.
  void SetZOrderLevel(ZOrderLevel order) override;
  ZOrderLevel GetZOrderLevel() const override;
  void SetShape(std::unique_ptr<ShapeRects> native_shape,
                const gfx::Transform& transform) override;
  std::string GetWindowUniqueId() const override;
  // SetUseNativeFrame and ShouldUseNativeFrame decide on
  // xdg-decoration mode for a window.
  void SetUseNativeFrame(bool use_native_frame) override;
  bool ShouldUseNativeFrame() const override;
  bool ShouldUpdateWindowShape() const override;
  bool CanSetDecorationInsets() const override;
  void SetOpaqueRegion(
      std::optional<std::vector<gfx::Rect>> region_px) override;
  void SetInputRegion(std::optional<std::vector<gfx::Rect>> region_px) override;
  bool IsClientControlledWindowMovementSupported() const override;
  void NotifyStartupComplete(const std::string& startup_id) override;
  void SetAspectRatio(const gfx::SizeF& aspect_ratio) override;
  void SetBoundsInPixels(const gfx::Rect& bounds) override;
  void SetBoundsInDIP(const gfx::Rect& bounds) override;

  // WmMoveLoopHandler:
  bool RunMoveLoop(const gfx::Vector2d& drag_offset) override;
  void EndMoveLoop() override;

  // WaylandToplevelExtension:
  void StartWindowDraggingSessionIfNeeded(
      ui::mojom::DragEventSource event_source,
      bool allow_system_drag) override;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void SetImmersiveFullscreenStatus(bool status) override;
  void SetTopInset(int height) override;
  gfx::RoundedCornersF GetWindowCornersRadii() override;
  void SetShadowCornersRadii(const gfx::RoundedCornersF& radii) override;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
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
  void SetFloatToLocation(
      WaylandFloatStartLocation float_start_location) override;
  void UnSetFloat() override;

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

  void DumpState(std::ostream& out) const override;

 private:
  // WaylandWindow protected overrides:
  // Calls UpdateWindowShape, set_input_region and set_opaque_region for this
  // toplevel window.
  void UpdateWindowMask() override;

  void UpdateSystemModal();

  void TriggerStateChanges(PlatformWindowState window_state);

  // Sets the new window `state` to the window. `target_display_id` gets ignored
  // unless the state is `PlatformWindowState::kFullscreen`.
  void SetWindowState(PlatformWindowState state, int64_t target_display_id);

  bool ShouldTriggerStateChange(PlatformWindowState state,
                                int64_t target_display_id) const;

  // Activates the surface using XDG activation given an activation token.
  void ActivateWithToken(std::string token);

  WaylandOutput* GetWaylandOutputForDisplayId(int64_t display_id);

  // Creates a surface window, which is visible as a main window.
  bool CreateShellToplevel();

  WmMoveResizeHandler* AsWmMoveResizeHandler();

  // Propagates the minimum size and maximum size to the ShellToplevel.
  void SetSizeConstraints();

  // Initializes additional shell integration, if the appropriate interfaces are
  // available.
  void SetUpShellIntegration();

  // Sets decoration mode for a window.
  void OnDecorationModeChanged();

  // Called when frame is locked to normal state or unlocked from
  // previously locked state.
  void OnFrameLockingChanged(bool lock);

  // Called when a window is moved to another desk or assigned to
  // all desks state.
  void OnDeskChanged(int state);

  // Sets `workspace_` to `aura_surface_`.
  // This must be called in SetUpShellIntegration().
  void SetInitialWorkspace();

  // Wrappers around shell surface.
  std::unique_ptr<ShellToplevelWrapper> shell_toplevel_;

  // True if it's maximized before requesting the window state change from the
  // client.
  // TODO(b/328109805): Move this logic to server side on Lacros.
  bool previously_maximized_ = false;

  // The display ID to switch to in case the state is `kFullscreen`.
  int64_t fullscreen_display_id_ = display::kInvalidDisplayId;

#if BUILDFLAG(IS_LINUX)
  // Contains the current state of the tiled edges.
  WindowTiledEdges tiled_state_;
#endif

  bool is_active_ = false;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // The flag that indicates the last requested immersive fullscreen status from
  // SetImmersiveFullscreenStatue to detect the immersive status changes. Set to
  // null if it had never been called.
  std::optional<bool> last_requested_immersive_status_ = std::nullopt;

  // Unique ID for this window. May be shared over non-Wayland IPC transports
  // (e.g. mojo) to identify the window.
  std::string window_unique_id_;

  int64_t initial_display_id_ = display::kInvalidDisplayId;
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

  std::optional<std::vector<gfx::Rect>> opaque_region_px_;
  std::optional<std::vector<gfx::Rect>> input_region_px_;

  // Information used by the compositor to restore the window state upon
  // creation.
  int32_t restore_session_id_ = 0;
  std::optional<int32_t> restore_window_id_ = 0;
  std::optional<std::string> restore_window_id_source_;

  // Information pertaining to a window's persistability.
  bool persistable_ = true;

  // Current modal status.
  bool system_modal_ = false;

  // The desk index for the window.
  // If |workspace_| is -1, window is visible on all workspaces.
  std::optional<int> workspace_ = std::nullopt;

  // The z order for the window.
  ZOrderLevel z_order_ = ZOrderLevel::kNormal;

  // True when screen coordinates is enabled.
  bool screen_coordinates_enabled_;

  // The last buffer scale sent to the wayland server.
  std::optional<float> last_sent_buffer_scale_;

  raw_ptr<WorkspaceExtensionDelegate> workspace_extension_delegate_ = nullptr;

  base::WeakPtrFactory<WaylandToplevelWindow> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TOPLEVEL_WINDOW_H_
