// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"

#include <string>

#include "base/nix/xdg_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/host/dump_util.h"
#include "ui/ozone/platform/wayland/host/gtk_shell1.h"
#include "ui/ozone/platform/wayland/host/gtk_surface1.h"
#include "ui/ozone/platform/wayland/host/shell_object_factory.h"
#include "ui/ozone/platform/wayland/host/shell_toplevel_wrapper.h"
#include "ui/ozone/platform/wayland/host/wayland_bubble.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_frame_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_popup.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window_drag_controller.h"
#include "ui/ozone/platform/wayland/host/wayland_zwp_pointer_constraints.h"
#include "ui/ozone/platform/wayland/host/xdg_activation.h"
#include "ui/platform_window/common/platform_window_defaults.h"
#include "ui/platform_window/extensions/wayland_extension.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

namespace {

bool ShouldSetBounds(PlatformWindowState state) {
  return state == PlatformWindowState::kNormal;
}

}  // namespace

constexpr int kVisibleOnAllWorkspaces = -1;

WaylandToplevelWindow::WaylandToplevelWindow(PlatformWindowDelegate* delegate,
                                             WaylandConnection* connection)
    : WaylandWindow(delegate, connection) {
  // Set a class property key, which allows |this| to be used for interactive
  // events, e.g. move or resize.
  SetWmMoveResizeHandler(this, AsWmMoveResizeHandler());
}

WaylandToplevelWindow::~WaylandToplevelWindow() = default;

bool WaylandToplevelWindow::CreateShellToplevel() {
  // Certain Wayland compositors (E.g. Mutter) expects wl_surface to have no
  // buffer attached when xdg-surface role is created.
  wl_surface_attach(root_surface()->surface(), nullptr, 0, 0);
  root_surface()->Commit(false);

  ShellObjectFactory factory;
  shell_toplevel_ = factory.CreateShellToplevelWrapper(connection(), this);
  if (!shell_toplevel_) {
    LOG(ERROR) << "Failed to create a ShellToplevel.";
    return false;
  }

  shell_toplevel_->SetAppId(app_id_);
  shell_toplevel_->SetTitle(window_title_);
  SetSizeConstraints();
  TriggerStateChanges(GetPlatformWindowState());
  SetUpShellIntegration();
  OnDecorationModeChanged();

  // This could be the proper time to update window mask using
  // NonClientView::GetWindowMask, since |non_client_view| is not created yet
  // during the call to WaylandWindow::Initialize().
  UpdateWindowMask();
  root_surface()->Commit(true);
  return true;
}

void WaylandToplevelWindow::DispatchHostWindowDragMovement(
    int hittest,
    const gfx::Point& pointer_location_in_px) {
  DCHECK(shell_toplevel_);

  if (hittest == HTCAPTION)
    shell_toplevel_->SurfaceMove(connection());
  else
    shell_toplevel_->SurfaceResize(connection(), hittest);

  connection()->Flush();
  // TODO(crbug.com/40917147): Revisit to resolve the correct impl.
  connection()->event_source()->ReleasePressedPointerButtons(this,
                                                             EventTimeForNow());
}

void WaylandToplevelWindow::Show(bool inactive) {
  if (shell_toplevel_)
    return;

  if (!CreateShellToplevel()) {
    Close();
    return;
  }

  UpdateWindowScale(false);

  if (inactive)
    Deactivate();

  WaylandWindow::Show(inactive);
}

void WaylandToplevelWindow::Hide() {
  if (!shell_toplevel_)
    return;

  if (child_popup()) {
    child_popup()->Hide();
    set_child_popup(nullptr);
  }
  for (auto bubble : child_bubbles()) {
    bubble->Hide();
  }
  WaylandWindow::Hide();

  // When running under Weston, if we don't do this immediately, the window
  // will be unable to receive mouse events after making it visible again.
  // See https://gitlab.freedesktop.org/wayland/weston/-/issues/950.
  if (root_surface() && root_surface()->buffer_id() != 0) {
    root_surface()->AttachBuffer(nullptr);
    root_surface()->ApplyPendingState();
    root_surface()->Commit(false);
  }

  if (gtk_surface1_)
    gtk_surface1_.reset();

  shell_toplevel_.reset();
  connection()->Flush();
}

bool WaylandToplevelWindow::IsVisible() const {
  // X and Windows return true if the window is minimized. For consistency, do
  // the same.
  return !!shell_toplevel_ ||
         GetPlatformWindowState() == PlatformWindowState::kMinimized;
}

void WaylandToplevelWindow::SetTitle(const std::u16string& title) {
  if (window_title_ == title)
    return;

  window_title_ = title;

  if (shell_toplevel_) {
    shell_toplevel_->SetTitle(title);
    connection()->Flush();
  }
}

void WaylandToplevelWindow::SetFullscreen(bool fullscreen,
                                          int64_t target_display_id) {
  // TODO(msisov, tonikitoo): add multiscreen support. As the documentation says
  // if xdg_toplevel_set_fullscreen() is not provided with wl_output, it's up
  // to the compositor to choose which display will be used to map this surface.

  // The `target_display_id` must be invalid if not going into fullscreen.
  DCHECK(fullscreen || target_display_id == display::kInvalidDisplayId);

  if (base::FeatureList::IsEnabled(features::kAsyncFullscreenWindowState)) {
    if (fullscreen) {
      shell_toplevel_->SetFullscreen(
          GetWaylandOutputForDisplayId(target_display_id));
    } else {
      shell_toplevel_->UnSetFullscreen();
    }
    return;
  }

  // We must track the previous state to correctly say our state as long as it
  // can be the maximized instead of normal one.
  PlatformWindowState new_state = PlatformWindowState::kUnknown;
  int64_t display_id = display::kInvalidDisplayId;

  if (fullscreen) {
    new_state = PlatformWindowState::kFullScreen;
    display_id = target_display_id;
  } else if (previously_maximized_) {
    new_state = PlatformWindowState::kMaximized;
  } else {
    new_state = PlatformWindowState::kNormal;
  }

  SetWindowState(new_state, display_id);
}

void WaylandToplevelWindow::Maximize() {
  SetWindowState(PlatformWindowState::kMaximized, display::kInvalidDisplayId);
}

void WaylandToplevelWindow::Minimize() {
  if (!shell_toplevel_) {
    // TODO(crbug.com/40276379): Store `PlatformWindowState::kMinimized` to a
    // pending state.
    return;
  }

  fullscreen_display_id_ = display::kInvalidDisplayId;
  shell_toplevel_->SetMinimized();

  if (IsSurfaceConfigured()) {
    // Wayland standard does not have API to notify client apps about
    // window minimized. In this case we update the window state here
    // synchronously,
    //
    // We also need to check if the surface is already configured in case of a
    // synchronous minimize because a minimized window cannot ack configure.
    // This can happen if a minimized window is restored by a session restore.
    //
    // TODO(crbug.com/40058672): Verify that the claim about a window
    // initialized as a minimized window cannot ack configure. If not
    // `IsSurfaceConfigured()` condition can be removed.
    //
    // TODO(crbug.com/40276379): Use `GetLatestRequestedState().window_state`
    // instead once the window state becomes async.
    auto previous_state = applied_state().window_state;
    previously_maximized_ = previous_state == PlatformWindowState::kMaximized;
    ForceApplyWindowStateDoNotUse(PlatformWindowState::kMinimized);
    delegate()->OnWindowStateChanged(previous_state,
                                     PlatformWindowState::kMinimized);
  }
}

void WaylandToplevelWindow::Restore() {
  DCHECK(shell_toplevel_);

  // Differently from other platforms, under Wayland, unmaximizing the dragged
  // window before starting the drag loop is not needed as it is assumed to be
  // handled at compositor side, just like in xdg_toplevel_surface::move. So
  // skip it if there's a window drag session running.
  auto* drag_controller = connection()->window_drag_controller();
  if (drag_controller && drag_controller->IsDragInProgress()) {
    return;
  }

  SetWindowState(PlatformWindowState::kNormal, display::kInvalidDisplayId);
}

void WaylandToplevelWindow::ActivateWithToken(std::string token) {
  DCHECK(connection()->xdg_activation());
  // xdg-activation implementation doesn't seem to interact well with dnd in
  // some compositors. Eg: Mutter crashes were observed in tab drag sessions.
  // See https://gitlab.gnome.org/GNOME/mutter/-/issues/3822.
  //
  // TODO(crbug.com/40866970): Remove once the compositor bug gets fixed.
  if (connection()->IsDragInProgress()) {
    return;
  }
  connection()->xdg_activation()->Activate(root_surface()->surface(), token);
}

void WaylandToplevelWindow::Activate() {
  // Activation is supported through optional protocol extensions and hence may
  // or may not work depending on the compositor.  The details depend on the
  // compositor as well; for example, Mutter doesn't bring the window to the top
  // when it requests focus, but instead shows a system popup notification to
  // user.
  if (connection()->xdg_activation()) {
    if (auto token = base::nix::TakeXdgActivationToken()) {
      ActivateWithToken(token.value());
    } else {
      connection()->xdg_activation()->RequestNewToken(
          base::BindOnce(&WaylandToplevelWindow::ActivateWithToken,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  } else if (gtk_surface1_) {
    gtk_surface1_->RequestFocus();
  }

  // This is required as the high level activation might not get a flush for
  // a while.
  connection()->Flush();

  WaylandWindow::Activate();
}

void WaylandToplevelWindow::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                           const gfx::ImageSkia& app_icon) {
  if (!shell_toplevel_) {
    return;
  }
  // Let the app icon take precedence over the window icon.
  if (!app_icon.isNull()) {
    shell_toplevel_->SetIcon(app_icon);
  } else {
    shell_toplevel_->SetIcon(window_icon);
  }
  root_surface()->Commit(/*flush=*/true);
}

void WaylandToplevelWindow::SizeConstraintsChanged() {
  // Size constraints only make sense for normal windows.
  if (!shell_toplevel_)
    return;

  SetSizeConstraints();
}

void WaylandToplevelWindow::SetZOrderLevel(ZOrderLevel order) {
  // TODO(crbug.com/374244479): Linux/Wayland doesn't support zorder level.
  // Consider complete removal of that.
  z_order_ = order;
}

ZOrderLevel WaylandToplevelWindow::GetZOrderLevel() const {
  return z_order_;
}

std::string WaylandToplevelWindow::GetWindowUniqueId() const {
  return app_id_;
}

void WaylandToplevelWindow::SetUseNativeFrame(bool use_native_frame) {
  if (use_native_frame_ == use_native_frame)
    return;
  use_native_frame_ = use_native_frame;
  if (shell_toplevel_)
    OnDecorationModeChanged();

  UpdateWindowMask();
}

bool WaylandToplevelWindow::ShouldUseNativeFrame() const {
  // This depends on availability of xdg-decoration protocol extension.
  // Returns false if there is no xdg-decoration protocol extension provided
  // even if use_native_frame_ is true.
  return use_native_frame_ && connection()->xdg_decoration_manager_v1();
}

bool WaylandToplevelWindow::ShouldUpdateWindowShape() const {
  return true;
}

bool WaylandToplevelWindow::CanSetDecorationInsets() const {
  return connection()->SupportsSetWindowGeometry();
}

void WaylandToplevelWindow::SetOpaqueRegion(
    std::optional<std::vector<gfx::Rect>> region_px) {
  opaque_region_px_ = region_px;
  root_surface()->set_opaque_region(region_px);
}

void WaylandToplevelWindow::SetInputRegion(
    std::optional<std::vector<gfx::Rect>> region_px) {
  input_region_px_ = region_px;
  root_surface()->set_input_region(region_px);
}

void WaylandToplevelWindow::NotifyStartupComplete(
    const std::string& startup_id) {
  if (auto* gtk_shell = connection()->gtk_shell1())
    gtk_shell->SetStartupId(startup_id);
}

void WaylandToplevelWindow::UpdateWindowScale(bool update_bounds) {
  auto old_scale = applied_state().window_scale;
  WaylandWindow::UpdateWindowScale(update_bounds);
  if (old_scale == applied_state().window_scale) {
    return;
  }

  // Update min/max size in DIP if buffer scale is updated.
  SizeConstraintsChanged();
}

WaylandToplevelWindow* WaylandToplevelWindow::AsWaylandToplevelWindow() {
  return this;
}

void WaylandToplevelWindow::HandleToplevelConfigure(
    int32_t width_dip,
    int32_t height_dip,
    const WindowStates& window_states) {
  HandleToplevelConfigureWithOrigin(0, 0, width_dip, height_dip, window_states);
}

void WaylandToplevelWindow::HandleToplevelConfigureWithOrigin(
    int32_t x,
    int32_t y,
    int32_t width_dip,
    int32_t height_dip,
    const WindowStates& window_states) {
  // TODO(crbug.com/369952980): Remove once arrays get logged by libwayland.
  VLOG(3) << __func__ << " states=[ " << window_states.ToString() << "]";

  PlatformWindowState window_state = PlatformWindowState::kUnknown;
  if ((GetLatestRequestedState().window_state ==
           PlatformWindowState::kMinimized &&
       !window_states.is_activated) ||
      window_states.is_minimized) {
    window_state = PlatformWindowState::kMinimized;
  } else if (window_states.is_fullscreen) {
    window_state = PlatformWindowState::kFullScreen;
  } else if (window_states.is_maximized) {
    window_state = PlatformWindowState::kMaximized;
  } else {
    window_state = PlatformWindowState::kNormal;
  }

  // No matter what mode we have, the display id doesn't matter at this time
  // anymore.
  fullscreen_display_id_ = display::kInvalidDisplayId;

  // Update state before notifying delegate.
  const bool did_active_change = is_active_ != window_states.is_activated;
  is_active_ = window_states.is_activated;
  bool prev_suspended = is_suspended_;
  is_suspended_ = window_states.is_suspended;

  // The tiled state affects the window geometry, so apply it here.
  if (window_states.tiled_edges != tiled_state_) {
    // This configure changes the decoration insets.  We should adjust the
    // bounds appropriately.
    tiled_state_ = window_states.tiled_edges;
    delegate()->OnWindowTiledStateChanged(window_states.tiled_edges);
  }

  pending_configure_state_.window_state = window_state;

  // Width or height set to 0 means that we should decide on width and height by
  // ourselves, but we don't want to set them to anything else. Use restored
  // bounds size or the current bounds iff the current state is normal (neither
  // maximized nor fullscreen).
  //
  // Note: if the browser was started with --start-fullscreen and a user exits
  // the fullscreen mode, wayland may set the width and height to be 1. Instead,
  // explicitly set the bounds to the current desired ones or the previous
  // bounds.
  gfx::Rect bounds_dip(
      pending_configure_state_.bounds_dip.value_or(gfx::Rect()));
  if (width_dip > 1 && height_dip > 1) {
    bounds_dip.SetRect(x, y, width_dip, height_dip);
    const auto& insets = delegate()->CalculateInsetsInDIP(window_state);
    if (ShouldSetBounds(window_state) && !insets.IsEmpty()) {
      bounds_dip.Inset(-insets);
      bounds_dip.set_origin({x, y});
    }
    // UI Scale must be applied only for size coming from the server. Restored
    // and current dip bounds (used below) are already ui-scale'd.
    bounds_dip = gfx::ScaleToEnclosingRectIgnoringError(
        bounds_dip, 1.f / applied_state().ui_scale);
  } else if (ShouldSetBounds(window_state)) {
    bounds_dip = !restored_bounds_dip().IsEmpty() ? restored_bounds_dip()
                                                  : GetBoundsInDIP();
  }

  bounds_dip = AdjustBoundsToConstraintsDIP(bounds_dip);
  pending_configure_state_.bounds_dip = bounds_dip;
  pending_configure_state_.size_px =
      delegate()->ConvertRectToPixels(bounds_dip).size();

  // Update `restored_bounds_dip_` which is used when the window gets back to
  // normal state after it went maximized or fullscreen. It can be client or
  // compositor side change, so we must store previous bounds to restore later.
  // We reset `restored_bounds_dip_` if the window is normal state, or update it
  // to the applied bounds if we don't have any meaningful value stored.
  if (ShouldSetBounds(window_state)) {
    SetRestoredBoundsInDIP({});
  } else if (GetRestoredBoundsInDIP().IsEmpty()) {
    SetRestoredBoundsInDIP(GetBoundsInDIP());
  }

  if (did_active_change) {
    frame_manager()->OnWindowActivationChanged();
    if (active_bubble()) {
      ActivateBubble(is_active_ ? active_bubble() : nullptr);
    } else {
      delegate()->OnActivationChanged(is_active_);
    }
  }
  if (prev_suspended != is_suspended_) {
    frame_manager()->OnWindowSuspensionChanged();
  }
}

void WaylandToplevelWindow::SetOrigin(const gfx::Point& origin) {
  gfx::Rect new_bounds(origin, GetBoundsInDIP().size());
  WaylandWindow::SetBoundsInDIP(new_bounds);
}

void WaylandToplevelWindow::HandleSurfaceConfigure(uint32_t serial) {
  ProcessPendingConfigureState(serial);
}

void WaylandToplevelWindow::OnSequencePoint(int64_t seq) {
  if (!shell_toplevel_)
    return;

  ProcessSequencePoint(seq);
  MaybeApplyLatestStateRequest(/*force=*/false);
}

bool WaylandToplevelWindow::OnInitialize(
    PlatformWindowInitProperties properties,
    PlatformWindowDelegate::State* state) {
  state->window_state = PlatformWindowState::kNormal;

  app_id_ = properties.wayland_app_id;
  SetWaylandToplevelExtension(this, this);
  SetWmMoveLoopHandler(this, static_cast<WmMoveLoopHandler*>(this));
  SetWorkspaceExtension(this, static_cast<WorkspaceExtension*>(this));
  SetWorkspaceExtensionDelegate(properties.workspace_extension_delegate);

  SetZOrderLevel(properties.z_order);

  if (!properties.workspace.empty()) {
    int workspace;
    base::StringToInt(properties.workspace, &workspace);
    workspace_ = workspace;
  } else if (properties.visible_on_all_workspaces) {
    workspace_ = kVisibleOnAllWorkspaces;
  }
  SetSystemModalExtension(this, static_cast<SystemModalExtension*>(this));
  return true;
}

bool WaylandToplevelWindow::IsActive() const {
  return is_active_;
}

bool WaylandToplevelWindow::IsSuspended() const {
  return is_suspended_;
}

bool WaylandToplevelWindow::IsSurfaceConfigured() {
  return shell_toplevel() ? shell_toplevel()->IsConfigured() : false;
}

void WaylandToplevelWindow::SetWindowGeometry(
    const PlatformWindowDelegate::State& state) {
  DCHECK(connection()->SupportsSetWindowGeometry());

  if (!shell_toplevel_)
    return;

  gfx::Rect geometry_dip = gfx::ScaleToEnclosingRectIgnoringError(
      gfx::Rect(state.bounds_dip.size()), state.ui_scale);

  auto insets_dip = delegate()->CalculateInsetsInDIP(state.window_state);
  if (!insets_dip.IsEmpty()) {
    geometry_dip.Inset(insets_dip);

    // Shrinking the bounds by the decoration insets might result in empty
    // bounds. For the reasons already explained in WaylandWindow::Initialize(),
    // we mustn't request an empty window geometry.
    if (geometry_dip.width() == 0) {
      geometry_dip.set_width(1);
    }
    if (geometry_dip.height() == 0) {
      geometry_dip.set_height(1);
    }
  }
  shell_toplevel_->SetWindowGeometry(geometry_dip);
}

void WaylandToplevelWindow::AckConfigure(uint32_t serial) {
  // We cannot assume the top level wrapper is non-NULL because of a corner case
  // in drag n' drop. There could be times when the tab strip change is detected
  // while processing a configure event received from the compositor and hence
  // destroy the top level wrapper before an ACK is sent.
  // See crbug.com/1512046 for details.
  if (shell_toplevel()) {
    shell_toplevel()->AckConfigure(serial);
  }
}

base::WeakPtr<WaylandWindow> WaylandToplevelWindow::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool WaylandToplevelWindow::IsClientControlledWindowMovementSupported() const {
  auto* window_drag_controller = connection()->window_drag_controller();
  DCHECK(window_drag_controller);
  return window_drag_controller->IsWindowDragProtocolAvailable();
}

bool WaylandToplevelWindow::ShouldReleaseCaptureForDrag(
    ui::OSExchangeData* data) const {
  auto* data_drag_controller = connection()->data_drag_controller();
  DCHECK(data_drag_controller);
  return data_drag_controller->ShouldReleaseCaptureForDrag(data);
}

bool WaylandToplevelWindow::RunMoveLoop(const gfx::Vector2d& drag_offset) {
  DCHECK(connection()->window_drag_controller());
  return connection()->window_drag_controller()->Drag(this, drag_offset);
}

void WaylandToplevelWindow::EndMoveLoop() {
  DCHECK(connection()->window_drag_controller());
  connection()->window_drag_controller()->StopDragging();
}

void WaylandToplevelWindow::StartWindowDraggingSessionIfNeeded(
    ui::mojom::DragEventSource event_source,
    bool allow_system_drag) {
  DCHECK(connection()->window_drag_controller());
  // If extended-drag and xdg-toplevel-drag are not available and
  // |allow_system_drag| is set, this is no-op and WaylandDataDragController is
  // assumed to be used instead. i.e: Fallback to a simpler window drag UX based
  // on regular system drag-and-drop.
  if (!IsClientControlledWindowMovementSupported() && allow_system_drag) {
    return;
  }
  connection()->window_drag_controller()->StartDragSession(this, event_source);
}

bool WaylandToplevelWindow::SupportsPointerLock() {
  return !!connection()->zwp_pointer_constraints() &&
         !!connection()->zwp_relative_pointer_manager();
}
void WaylandToplevelWindow::LockPointer(bool enabled) {
  auto* pointer_constraints = connection()->zwp_pointer_constraints();
  if (enabled)
    pointer_constraints->LockPointer(root_surface());
  else
    pointer_constraints->UnlockPointer();
}

void WaylandToplevelWindow::SetSystemModal(bool modal) {
  system_modal_ = modal;
  if (shell_toplevel_)
    shell_toplevel_->SetSystemModal(modal);
}

void WaylandToplevelWindow::DumpState(std::ostream& out) const {
  WaylandWindow::DumpState(out);
  out << ", title=" << window_title_
      << ", is_active=" << ToBoolString(is_active_)
      << ", system_modal=" << ToBoolString(system_modal_);
}

void WaylandToplevelWindow::UpdateSystemModal() {
  if (shell_toplevel_)
    shell_toplevel_->SetSystemModal(system_modal_);
}

std::string WaylandToplevelWindow::GetWorkspace() const {
  return workspace_.has_value() ? base::NumberToString(workspace_.value())
                                : std::string();
}

void WaylandToplevelWindow::SetVisibleOnAllWorkspaces(bool always_visible) {
  // TODO(crbug.com/374244479): remove this.
}

bool WaylandToplevelWindow::IsVisibleOnAllWorkspaces() const {
  return workspace_ == kVisibleOnAllWorkspaces;
}

void WaylandToplevelWindow::SetWorkspaceExtensionDelegate(
    WorkspaceExtensionDelegate* delegate) {
  workspace_extension_delegate_ = delegate;
}

void WaylandToplevelWindow::TriggerStateChanges(
    PlatformWindowState window_state) {
  if (shell_toplevel_) {
    // Call UnSetMaximized only if current state is normal. Otherwise, if the
    // current state is fullscreen and the previous is maximized, calling
    // UnSetMaximized may result in wrong restored window position that clients
    // are not allowed to know about.
    if (window_state == PlatformWindowState::kMinimized) {
      LOG(FATAL) << "Should not be called with kMinimized state";
    } else if (window_state == PlatformWindowState::kFullScreen) {
      shell_toplevel_->SetFullscreen(
          GetWaylandOutputForDisplayId(fullscreen_display_id_));
    } else if (GetLatestRequestedState().window_state ==
               PlatformWindowState::kFullScreen) {
      shell_toplevel_->UnSetFullscreen();
    } else if (window_state == PlatformWindowState::kMaximized) {
      shell_toplevel_->SetMaximized();
    } else if (window_state == PlatformWindowState::kNormal) {
      shell_toplevel_->UnSetMaximized();
    }
  }

  // Update the window state of the applied state before calling
  // OnWindowStateChanged so it can be used to pick up the new window state. We
  // cannot request state here because the bounds is not yet synchronized with
  // window state. Requesting the state will trigger SetWindowGeometry with the
  // current bounds + insets, so it has a risk to set geometry aligning with the
  // client side window state while the server side has not yet configured it.
  // This behavior is not necessarily a problem, but it causes the failure on
  // weston.
  // TODO(crbug.com/40276379): Remove this once this is async.
  auto previous_state = applied_state().window_state;
  ForceApplyWindowStateDoNotUse(window_state);
  delegate()->OnWindowStateChanged(previous_state, window_state);
  connection()->Flush();
}

void WaylandToplevelWindow::SetWindowState(PlatformWindowState window_state,
                                           int64_t target_display_id) {
  CHECK_NE(window_state, PlatformWindowState::kMinimized);

  if (ShouldTriggerStateChange(window_state, target_display_id)) {
    // TODO(crbug.com/40276379): Use `GetLatestRequestedState().window_state`
    // instead once the window state becomes async.
    auto previous_state = applied_state().window_state;

    // We want to remember whether it was previously maximized, for cases like
    // fullscreening to a different output while already in fullscreen, so we
    // can still restore back to the previous non-fullscreen state.
    if (previous_state != window_state) {
      previously_maximized_ = previous_state == PlatformWindowState::kMaximized;
    }

    // Remember the display id if we are going to fullscreen - otherwise reset.
    fullscreen_display_id_ = (window_state == PlatformWindowState::kFullScreen)
                                 ? target_display_id
                                 : display::kInvalidDisplayId;

    TriggerStateChanges(window_state);
  }
}

bool WaylandToplevelWindow::ShouldTriggerStateChange(
    PlatformWindowState window_state,
    int64_t target_display_id) const {
  // Allow the state transition if the state is different.
  //
  // The latest requested state from the client is stored as
  // `applied_state().window_state` so use it as a previous state.
  // TODO(crbug.com/40276379): Use `GetLatestRequestedState().window_state`
  // instead once the window state becomes async.
  if (applied_state().window_state != window_state) {
    return true;
  }

  // Allow the state transition if the state is fullscreen and the screen has
  // changed to something explicit - or different.
  if (window_state == PlatformWindowState::kFullScreen &&
      target_display_id != display::kInvalidDisplayId &&
      target_display_id != fullscreen_display_id_) {
    return true;
  }

  // Otherwise do not allow the transition.
  return false;
}

WaylandOutput* WaylandToplevelWindow::GetWaylandOutputForDisplayId(
    int64_t display_id) {
  auto* output_manager = connection()->wayland_output_manager();
  if (auto* screen = output_manager->wayland_screen()) {
    return screen->GetWaylandOutputForDisplayId(display_id);
  }
  return nullptr;
}

WmMoveResizeHandler* WaylandToplevelWindow::AsWmMoveResizeHandler() {
  return static_cast<WmMoveResizeHandler*>(this);
}

void WaylandToplevelWindow::SetSizeConstraints() {
  DCHECK(delegate());

  auto min_size_dip = delegate()->GetMinimumSizeForWindow();
  auto max_size_dip = delegate()->GetMaximumSizeForWindow();

  if (min_size_dip.has_value())
    shell_toplevel_->SetMinSize(min_size_dip->width(), min_size_dip->height());

  if (max_size_dip.has_value())
    shell_toplevel_->SetMaxSize(max_size_dip->width(), max_size_dip->height());

  connection()->Flush();
}

void WaylandToplevelWindow::SetUpShellIntegration() {
  // This method should be called after the XDG surface is initialized.
  DCHECK(shell_toplevel_);
  // We must not request a new GtkSurface if we already have one, else we get a
  // "gtk_shell::get_gtk_surface already requested" error. (crbug.com/1380419)
  if (connection()->gtk_shell1() && !gtk_surface1_) {
    gtk_surface1_ =
        connection()->gtk_shell1()->GetGtkSurface1(root_surface()->surface());
  }
}

void WaylandToplevelWindow::OnDecorationModeChanged() {
  DCHECK(shell_toplevel_);
  if (use_native_frame_) {
    // Set server-side decoration for windows using a native frame,
    // e.g. taskmanager
    shell_toplevel_->SetDecoration(
        ShellToplevelWrapper::DecorationMode::kServerSide);
  } else {
    shell_toplevel_->SetDecoration(
        ShellToplevelWrapper::DecorationMode::kClientSide);
  }
}

void WaylandToplevelWindow::UpdateWindowMask() {
  std::vector<gfx::Rect> region{gfx::Rect({}, latched_state().size_px)};
  root_surface()->set_opaque_region(
      opaque_region_px_.has_value()
          ? opaque_region_px_
          : (IsOpaqueWindow() ? std::optional<std::vector<gfx::Rect>>(region)
                              : std::nullopt));
  root_surface()->set_input_region(input_region_px_ ? input_region_px_
                                                    : region);
}

bool WaylandToplevelWindow::GetTabletMode() {
  return connection()->GetTabletMode();
}

}  // namespace ui
