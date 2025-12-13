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
#include "base/scoped_observation.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/xdg_session.h"
#include "ui/ozone/public/platform_session_manager.h"
#include "ui/platform_window/extensions/system_modal_extension.h"
#include "ui/platform_window/extensions/wayland_extension.h"
#include "ui/platform_window/extensions/workspace_extension.h"
#include "ui/platform_window/extensions/workspace_extension_delegate.h"
#include "ui/platform_window/wm/wm_move_loop_handler.h"
#include "ui/platform_window/wm/wm_move_resize_handler.h"

namespace views::corewm {
enum class TooltipTrigger;
}  // namespace views::corewm

namespace ui {

class OrgKdeKwinAppmenu;
class XdgToplevel;

class WaylandToplevelWindow : public WaylandWindow,
                              public WmMoveResizeHandler,
                              public WmMoveLoopHandler,
                              public WaylandToplevelExtension,
                              public WorkspaceExtension,
                              public SystemModalExtension,
                              public XdgSession::Observer {
 public:
  WaylandToplevelWindow(PlatformWindowDelegate* delegate,
                        WaylandConnection* connection);
  WaylandToplevelWindow(const WaylandToplevelWindow&) = delete;
  WaylandToplevelWindow& operator=(const WaylandToplevelWindow&) = delete;
  ~WaylandToplevelWindow() override;

  XdgToplevel* xdg_toplevel() const { return xdg_toplevel_.get(); }

  // Sets the window's origin.
  void SetOrigin(const gfx::Point& origin);

  // Notify that this window's active state may change.
  void UpdateActivationState();

  // WaylandWindow overrides:
  void UpdateWindowScale(bool update_bounds) override;
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
  bool IsSuspended() const override;
  void SetWindowGeometry(const PlatformWindowDelegate::State& state) override;
  base::WeakPtr<WaylandWindow> AsWeakPtr() override;

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
  void ShowWindowControlsMenu(const gfx::Point& point) override;
  void Activate() override;
  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon) override;
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
  void SetOpaqueRegion(
      std::optional<std::vector<gfx::Rect>> region_px) override;
  void SetInputRegion(std::optional<std::vector<gfx::Rect>> region_px) override;
  bool IsClientControlledWindowMovementSupported() const override;

  // WmMoveLoopHandler:
  bool RunMoveLoop(const gfx::Vector2d& drag_offset) override;
  void EndMoveLoop() override;

  // WaylandToplevelExtension:
  void StartWindowDraggingSessionIfNeeded(
      ui::mojom::DragEventSource event_source,
      bool allow_system_drag) override;
  bool SupportsPointerLock() override;
  void LockPointer(bool enabled) override;
  void SetAppmenu(const std::string& service_name,
                  const std::string& object_path) override;
  void UnsetAppmenu() override;

  // WorkspaceExtension:
  std::string GetWorkspace() const override;
  void SetVisibleOnAllWorkspaces(bool always_visible) override;
  bool IsVisibleOnAllWorkspaces() const override;
  void SetWorkspaceExtensionDelegate(
      WorkspaceExtensionDelegate* delegate) override;

  // SystemModalExtension:
  void SetSystemModal(bool modal) override;

  void DumpState(std::ostream& out) const override;

  // XdgSession::Observer:
  void OnSessionDestroying() override;

 private:
  // XdgToplevelSession instances are owned by toplevel windows, given their
  // close lifecycle, though ownership might be transferred to the associated
  // XdgSession, during removals, for example. To make this relationship more
  // explicit and keep the public API clean and concise, friendship is used
  // here. See XdgSession::RemoveToplevel for further context.
  friend class XdgSession;
  std::unique_ptr<XdgToplevelSession> TakeToplevelSession() {
    return std::move(toplevel_session_);
  }
  std::string session_id() const {
    return session_data_ ? session_data_->session_id : "";
  }
  int32_t session_toplevel_id() const {
    return session_data_ ? session_data_->window_id : 0;
  }

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

  // Creates and initializes the underlying xdg_toplevel surface, which
  // is the protocol object the makes it possible for this map this as a
  // toplevel window.
  bool CreateXdgToplevel();

  WmMoveResizeHandler* AsWmMoveResizeHandler();

  // Propagates the minimum size and maximum size to the ShellToplevel.
  void SetSizeConstraints();

  // Initializes additional shell integration, if the appropriate interfaces are
  // available.
  void SetUpShellIntegration();

  // Sets decoration mode for a window.
  void OnDecorationModeChanged();

  // Issues session management requests, if needed, at mapping- and
  // configure-time stages of the toplevel window initialization.
  void UpdateSessionStateIfNeeded();

  // Try to announce the appmenu associated with this toplevel, if there's any.
  void TryAnnounceAppmenu();

  std::unique_ptr<XdgToplevel> xdg_toplevel_;

  // True if it's maximized before requesting the window state change from the
  // client.
  bool previously_maximized_ = false;

  // The display ID to switch to in case the state is `kFullscreen`.
  int64_t fullscreen_display_id_ = display::kInvalidDisplayId;

  bool is_active_ = false;
  bool is_xdg_active_ = false;
  bool is_suspended_ = false;

  // Id of the chromium app passed through
  // PlatformWindowInitProperties::wm_class_name. This is used by Wayland
  // compositor to identify the app, unite it's windows into the same stack of
  // windows and find *.desktop file to set various preferences including icons.
  std::string app_id_;

  // Title of the ShellToplevel.
  std::u16string window_title_;

  // When use_native_frame is false, client-side decoration is set.
  // When use_native_frame is true, server-side decoration is set.
  bool use_native_frame_ = false;

  std::optional<std::vector<gfx::Rect>> opaque_region_px_;
  std::optional<std::vector<gfx::Rect>> input_region_px_;

  // Current modal status.
  bool system_modal_ = false;

  // The desk index for the window.
  // If |workspace_| is -1, window is visible on all workspaces.
  std::optional<int> workspace_ = std::nullopt;

  // The z order for the window.
  ZOrderLevel z_order_ = ZOrderLevel::kNormal;

  // Activation will only be done if the surface is configured.
  // If it is requested before, it is stored and executed in ack configure.
  std::optional<std::string> pending_configure_activation_token_ = std::nullopt;

  raw_ptr<WorkspaceExtensionDelegate> workspace_extension_delegate_ = nullptr;

  gfx::ImageSkia initial_icon_;

  std::optional<PlatformSessionWindowData> session_data_;
  raw_ptr<XdgSession> session_;
  base::ScopedObservation<XdgSession, XdgSession::Observer> session_observer_{
      this};
  std::unique_ptr<XdgToplevelSession> toplevel_session_;

  // Global application menu integration.
  std::unique_ptr<OrgKdeKwinAppmenu> appmenu_;
  std::string appmenu_service_name_;
  std::string appmenu_object_path_;

  base::WeakPtrFactory<WaylandToplevelWindow> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TOPLEVEL_WINDOW_H_
