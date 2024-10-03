// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"

#include <aura-shell-client-protocol.h>

#include <string>

#include "base/nix/xdg_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "build/chromeos_buildflags.h"
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
#include "ui/ozone/platform/wayland/host/wayland_zaura_shell.h"
#include "ui/ozone/platform/wayland/host/wayland_zwp_pointer_constraints.h"
#include "ui/ozone/platform/wayland/host/xdg_activation.h"
#include "ui/platform_window/common/platform_window_defaults.h"
#include "ui/platform_window/extensions/wayland_extension.h"
#include "ui/platform_window/platform_window_delegate.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#endif

namespace ui {

namespace {

bool ShouldSetBounds(PlatformWindowState state) {
  return state == PlatformWindowState::kNormal ||
         state == PlatformWindowState::kSnappedPrimary ||
         state == PlatformWindowState::kSnappedSecondary ||
         state == PlatformWindowState::kFloated;
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool IsPinnedOrFullscreen(const WaylandWindow::WindowStates& states) {
  return states.is_fullscreen || states.is_pinned_fullscreen ||
         states.is_trusted_pinned_fullscreen;
}
#endif  // BUILDFLAG(IS_CHOMEOS_LACROS)

}  // namespace

constexpr int kVisibleOnAllWorkspaces = -1;

WaylandToplevelWindow::WaylandToplevelWindow(PlatformWindowDelegate* delegate,
                                             WaylandConnection* connection)
    : WaylandWindow(delegate, connection),
      screen_coordinates_enabled_(kDefaultScreenCoordinateEnabled) {
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

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  screen_coordinates_enabled_ &= shell_toplevel_->SupportsScreenCoordinates();
  screen_coordinates_enabled_ &= !use_native_frame_;

  if (screen_coordinates_enabled_) {
    shell_toplevel_->EnableScreenCoordinates();
  }
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  shell_toplevel_->SetAppId(window_unique_id_);
#else
  shell_toplevel_->SetAppId(app_id_);
#endif
  shell_toplevel_->SetTitle(window_title_);
  SetSizeConstraints();
  TriggerStateChanges(GetPlatformWindowState());
  SetUpShellIntegration();
  OnDecorationModeChanged();

  auto* zaura_surface = GetZAuraSurface();
  if (system_modal_ && zaura_surface) {
    zaura_surface->SetFrame(ZAURA_SURFACE_FRAME_TYPE_SHADOW);
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (screen_coordinates_enabled_) {
    auto bounds_dip = GetBoundsInDIP();
    WaylandWindow::SetBoundsInDIP(bounds_dip);
    if (shell_toplevel_) {
      shell_toplevel_->RequestWindowBounds(bounds_dip, initial_display_id_);
    }
  }
#endif

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
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(crbug.com/40917147): Revisit to resolve the correct impl.
  connection()->event_source()->ReleasePressedPointerButtons(this,
                                                             EventTimeForNow());
#endif
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

  // Request the compositor to cease any possible ongoing snapping
  // preview/commit. Use any value for `snap_ratio` since it will not be used.
  CommitSnap(WaylandWindowSnapDirection::kNone, /*snap_ratio=*/1.f);

  if (root_surface()) {
    root_surface()->ResetZAuraSurface();
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

  if (!SupportsConfigureMinimizedState() && IsSurfaceConfigured()) {
    // Wayland standard does not have API to notify client apps about
    // window minimized, while exo has an extension (in
    // zaura_shell::configure) for it.
    // In the former case we update the window state here synchronously,
    // while in the latter case update the window state in the handler of
    // configure (HandleAuraToplevelConfigure) asynchronously.
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
  // xdg-activation implementation in some compositors is still buggy and
  // Mutter crashes were observed when windows are activated during window
  // dragging sessions. See https://crbug.com/1366504.
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
  //
  // Exo provides activation through aura-shell, Mutter--through gtk-shell.
  auto* zaura_surface = GetZAuraSurface();
  if (shell_toplevel_ && shell_toplevel_->SupportsActivation()) {
    shell_toplevel_->Activate();
  } else if (zaura_surface && zaura_surface->SupportsActivate()) {
    zaura_surface->Activate();
  } else if (connection()->xdg_activation()) {
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
  // a while. Example: Ash calls OpenURL in Lacros, which activates a window
  // but nothing more happens (until the user moves the mouse over a Lacros
  // window in which case events will start and the activation will come
  // through).
  connection()->Flush();

  WaylandWindow::Activate();
}

void WaylandToplevelWindow::Deactivate() {
  if (shell_toplevel_ && shell_toplevel_->SupportsActivation()) {
    shell_toplevel_->Deactivate();
    connection()->Flush();
  }
  WaylandWindow::Deactivate();
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
  if (shell_toplevel_)
    shell_toplevel_->SetZOrder(order);

  z_order_ = order;
}

ZOrderLevel WaylandToplevelWindow::GetZOrderLevel() const {
  return z_order_;
}

void WaylandToplevelWindow::SetShape(std::unique_ptr<ShapeRects> native_shape,
                                     const gfx::Transform& transform) {
  if (shell_toplevel_) {
    shell_toplevel_->SetShape(std::move(native_shape));
    // The surface shape is double-buffered state maintained by the shell
    // surface server-side and applied to the root surface. We must also commit
    // the surface tree to ensure state is applied correctly.
    root_surface()->Commit(false);
  }
}

std::string WaylandToplevelWindow::GetWindowUniqueId() const {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return window_unique_id_;
#else
  return app_id_;
#endif
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

void WaylandToplevelWindow::SetAspectRatio(const gfx::SizeF& aspect_ratio) {
  if (auto* zaura_surface = GetZAuraSurface()) {
    zaura_surface->SetAspectRatio(aspect_ratio.width(), aspect_ratio.height());
  }
}

bool WaylandToplevelWindow::IsScreenCoordinatesEnabled() const {
  return screen_coordinates_enabled_;
}

bool WaylandToplevelWindow::SupportsConfigureMinimizedState() const {
  return shell_toplevel_ && shell_toplevel_->IsSupportedOnAuraToplevel(
                                ZAURA_TOPLEVEL_STATE_MINIMIZED_SINCE_VERSION);
}

bool WaylandToplevelWindow::SupportsConfigurePinnedState() const {
  return shell_toplevel_ &&
         shell_toplevel_->IsSupportedOnAuraToplevel(
             ZAURA_TOPLEVEL_STATE_TRUSTED_PINNED_SINCE_VERSION);
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

void WaylandToplevelWindow::OnRotateFocus(uint32_t serial,
                                          uint32_t direction,
                                          bool restart) {
  if (!is_active_ || !HasKeyboardFocus()) {
    VLOG(1) << "requested focus rotation when surface is not active or does "
               "not have keyboard focus {active, focus}: {"
            << is_active_ << ", " << HasKeyboardFocus()
            << "}. This might be caused by delay in exo. Ignoring request.";
    shell_toplevel()->AckRotateFocus(
        serial, ZAURA_TOPLEVEL_ROTATE_HANDLED_STATE_NOT_HANDLED);
    return;
  }

  auto platform_direction =
      direction == ZAURA_TOPLEVEL_ROTATE_DIRECTION_FORWARD
          ? PlatformWindowDelegate::RotateDirection::kForward
          : PlatformWindowDelegate::RotateDirection::kBackward;
  bool rotated = delegate()->OnRotateFocus(platform_direction, restart);
  shell_toplevel()->AckRotateFocus(
      serial, rotated ? ZAURA_TOPLEVEL_ROTATE_HANDLED_STATE_HANDLED
                      : ZAURA_TOPLEVEL_ROTATE_HANDLED_STATE_NOT_HANDLED);
}

void WaylandToplevelWindow::OnOverviewChange(uint32_t in_overview_as_int) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  const bool in_overview =
      in_overview_as_int == ZAURA_TOPLEVEL_IN_OVERVIEW_IN_OVERVIEW;
  delegate()->OnOverviewModeChanged(in_overview);
#endif
}

void WaylandToplevelWindow::LockFrame() {
  OnFrameLockingChanged(true);
}

void WaylandToplevelWindow::UnlockFrame() {
  OnFrameLockingChanged(false);
}

void WaylandToplevelWindow::OcclusionStateChanged(
    PlatformWindowOcclusionState occlusion_state) {
  WaylandWindow::OcclusionStateChanged(occlusion_state);
  delegate()->OnOcclusionStateChanged(occlusion_state);
}

void WaylandToplevelWindow::DeskChanged(int state) {
  OnDeskChanged(state);
}

void WaylandToplevelWindow::StartThrottle() {
  delegate()->SetFrameRateThrottleEnabled(true);
}

void WaylandToplevelWindow::EndThrottle() {
  delegate()->SetFrameRateThrottleEnabled(false);
}

void WaylandToplevelWindow::TooltipShown(const char* text,
                                         int32_t x,
                                         int32_t y,
                                         int32_t width,
                                         int32_t height) {
  delegate()->OnTooltipShownOnServer(base::UTF8ToUTF16(text),
                                     gfx::Rect(x, y, width, height));
}

void WaylandToplevelWindow::TooltipHidden() {
  delegate()->OnTooltipHiddenOnServer();
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
  if ((!SupportsConfigureMinimizedState() &&
       GetLatestRequestedState().window_state ==
           PlatformWindowState::kMinimized &&
       !window_states.is_activated) ||
      window_states.is_minimized) {
    window_state = PlatformWindowState::kMinimized;
  } else if (window_states.is_fullscreen) {
    window_state = PlatformWindowState::kFullScreen;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  } else if (window_states.is_pinned_fullscreen) {
    window_state = PlatformWindowState::kPinnedFullscreen;
  } else if (window_states.is_trusted_pinned_fullscreen) {
    window_state = PlatformWindowState::kTrustedPinnedFullscreen;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  } else if (window_states.is_maximized) {
    window_state = PlatformWindowState::kMaximized;
  } else if (window_states.is_snapped_primary) {
    window_state = PlatformWindowState::kSnappedPrimary;
  } else if (window_states.is_snapped_secondary) {
    window_state = PlatformWindowState::kSnappedSecondary;
  } else if (window_states.is_floated) {
    window_state = PlatformWindowState::kFloated;
  } else if (window_states.is_pip) {
    window_state = PlatformWindowState::kPip;
  } else {
    window_state = PlatformWindowState::kNormal;
  }

  // No matter what mode we have, the display id doesn't matter at this time
  // anymore.
  fullscreen_display_id_ = display::kInvalidDisplayId;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  CHECK(!window_states.is_immersive_fullscreen ||
        IsPinnedOrFullscreen(window_states))
      << "Immersive state should not be set when it's not fullscreen.";

  // TODO(crbug.com/41485096): Refer to window_states.is_pinned_fullscreen and
  // is_trusted_window_fullscreen and set kPinned/kTrustedPinned as a fullscreen
  // type when it's supported.
  PlatformFullscreenType fullscreen_type =
      window_states.is_immersive_fullscreen
          ? PlatformFullscreenType::kImmersive
          : (IsPinnedOrFullscreen(window_states)
                 ? PlatformFullscreenType::kPlain
                 : PlatformFullscreenType::kNone);
  pending_configure_state_.fullscreen_type = fullscreen_type;
#endif

  // Update state before notifying delegate.
  const bool did_active_change = is_active_ != window_states.is_activated;
  is_active_ = window_states.is_activated;

#if BUILDFLAG(IS_LINUX)
  // The tiled state affects the window geometry, so apply it here.
  if (window_states.tiled_edges != tiled_state_) {
    // This configure changes the decoration insets.  We should adjust the
    // bounds appropriately.
    tiled_state_ = window_states.tiled_edges;
    delegate()->OnWindowTiledStateChanged(window_states.tiled_edges);
  }
#endif  // IS_LINUX || IS_CHROMEOS_LACROS

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
  // We reset `restored_bounds_dip_` if the window is normal, snapped or floated
  // state, or update it to the applied bounds if we don't have any meaningful
  // value stored.
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
}

void WaylandToplevelWindow::SetBoundsInPixels(const gfx::Rect& bounds) {
  WaylandWindow::SetBoundsInPixels(bounds);
  if (shell_toplevel_ && screen_coordinates_enabled_) {
    gfx::Rect bounds_in_dip = delegate()->ConvertRectToDIP(bounds);
    shell_toplevel_->RequestWindowBounds(bounds_in_dip);
  }
}

void WaylandToplevelWindow::SetBoundsInDIP(const gfx::Rect& bounds_dip) {
  WaylandWindow::SetBoundsInDIP(bounds_dip);
  if (shell_toplevel_ && screen_coordinates_enabled_)
    shell_toplevel_->RequestWindowBounds(bounds_dip);
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

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto token = base::UnguessableToken::Create();
  window_unique_id_ =
      std::string(crosapi::kLacrosAppIdPrefix) + token.ToString();
#else
  app_id_ = properties.wayland_app_id;
#endif
  SetWaylandToplevelExtension(this, this);
  SetWmMoveLoopHandler(this, static_cast<WmMoveLoopHandler*>(this));
  SetWorkspaceExtension(this, static_cast<WorkspaceExtension*>(this));
  SetWorkspaceExtensionDelegate(properties.workspace_extension_delegate);
  SetDeskExtension(this, static_cast<DeskExtension*>(this));

  // When we are initializing and we do not already have a `shell_toplevel_`,
  // this will simply set `z_order_` and then set it as the window's initial z
  // order in `SetUpShellIntegration()`.
  SetZOrderLevel(properties.z_order);

  if (!properties.workspace.empty()) {
    int workspace;
    base::StringToInt(properties.workspace, &workspace);
    workspace_ = workspace;
  } else if (properties.visible_on_all_workspaces) {
    workspace_ = kVisibleOnAllWorkspaces;
  }
  restore_session_id_ = properties.restore_session_id;
  restore_window_id_ = properties.restore_window_id;
  restore_window_id_source_ = properties.restore_window_id_source;
  persistable_ = properties.persistable;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (properties.display_id.has_value()) {
    initial_display_id_ = *properties.display_id;
  }
#endif
  SetPinnedModeExtension(this, static_cast<PinnedModeExtension*>(this));
  SetSystemModalExtension(this, static_cast<SystemModalExtension*>(this));
  return true;
}

bool WaylandToplevelWindow::IsActive() const {
  return is_active_;
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

void WaylandToplevelWindow::PropagateBufferScale(float new_scale) {
  if (!IsSurfaceConfigured())
    return;

  if (!last_sent_buffer_scale_ ||
      last_sent_buffer_scale_.value() != new_scale) {
    shell_toplevel()->SetScaleFactor(new_scale);
    last_sent_buffer_scale_ = new_scale;
  }
}

base::WeakPtr<WaylandWindow> WaylandToplevelWindow::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void WaylandToplevelWindow::ShowTooltip(
    const std::u16string& text,
    const gfx::Point& position,
    const PlatformWindowTooltipTrigger trigger,
    const base::TimeDelta show_delay,
    const base::TimeDelta hide_delay) {
  auto* zaura_surface = GetZAuraSurface();
  const auto zaura_shell_trigger =
      trigger == PlatformWindowTooltipTrigger::kCursor
          ? ZAURA_SURFACE_TOOLTIP_TRIGGER_CURSOR
          : ZAURA_SURFACE_TOOLTIP_TRIGGER_KEYBOARD;
  if (zaura_surface &&
      zaura_surface->ShowTooltip(text, position, zaura_shell_trigger,
                                 show_delay, hide_delay)) {
    connection()->Flush();
  }
}

void WaylandToplevelWindow::HideTooltip() {
  auto* zaura_surface = GetZAuraSurface();
  if (zaura_surface && zaura_surface->HideTooltip()) {
    connection()->Flush();
  }
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

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void WaylandToplevelWindow::SetImmersiveFullscreenStatus(bool status) {
  // Skip if `status` is same as the last request.
  if (last_requested_immersive_status_ == status) {
    return;
  }
  last_requested_immersive_status_ = std::make_optional(status);

  if (shell_toplevel_) {
    shell_toplevel_->SetUseImmersiveMode(status);
  }
}

void WaylandToplevelWindow::SetTopInset(int height) {
  if (shell_toplevel_) {
    shell_toplevel_->SetTopInset(height);
  }
}

gfx::RoundedCornersF WaylandToplevelWindow::GetWindowCornersRadii() {
  auto* zaura_shell = connection()->zaura_shell();
  return zaura_shell->GetWindowCornersRadii();
}

void WaylandToplevelWindow::SetShadowCornersRadii(
    const gfx::RoundedCornersF& radii) {
  if (shell_toplevel_) {
    shell_toplevel_->SetShadowCornersRadii(radii);
  }
}

#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

void WaylandToplevelWindow::ShowSnapPreview(
    WaylandWindowSnapDirection snap_direction,
    bool allow_haptic_feedback) {
  if (shell_toplevel_ && shell_toplevel_->IsSupportedOnAuraToplevel(
                             ZAURA_TOPLEVEL_INTENT_TO_SNAP_SINCE_VERSION)) {
    shell_toplevel_->ShowSnapPreview(snap_direction, allow_haptic_feedback);
    return;
  }

  auto* zaura_surface = GetZAuraSurface();
  zaura_surface_snap_direction zaura_shell_snap_direction =
      ZAURA_SURFACE_SNAP_DIRECTION_NONE;
  switch (snap_direction) {
    case WaylandWindowSnapDirection::kPrimary:
      zaura_shell_snap_direction = ZAURA_SURFACE_SNAP_DIRECTION_LEFT;
      break;
    case WaylandWindowSnapDirection::kSecondary:
      zaura_shell_snap_direction = ZAURA_SURFACE_SNAP_DIRECTION_RIGHT;
      break;
    case WaylandWindowSnapDirection::kNone:
      break;
  }
  if (zaura_surface &&
      zaura_surface->IntentToSnap(zaura_shell_snap_direction)) {
    return;
  }

  // Window snapping isn't available for non-lacros builds.
  NOTIMPLEMENTED_LOG_ONCE();
}

void WaylandToplevelWindow::CommitSnap(
    WaylandWindowSnapDirection snap_direction,
    float snap_ratio) {
  // If aura_toplevel does not support `WaylandWindowSnapDirection::kNone` let
  // it fallthrough to `zaura_surface_unset_snap()`.
  const bool use_shell_toplevel =
      shell_toplevel_ &&
      (shell_toplevel_->IsSupportedOnAuraToplevel(
           ZAURA_TOPLEVEL_UNSET_SNAP_SINCE_VERSION) ||
       (snap_direction != WaylandWindowSnapDirection::kNone &&
        shell_toplevel_->IsSupportedOnAuraToplevel(
            ZAURA_TOPLEVEL_SET_SNAP_PRIMARY_SINCE_VERSION)));
  if (use_shell_toplevel) {
    shell_toplevel_->CommitSnap(snap_direction, snap_ratio);
    return;
  }

  auto* zaura_surface = GetZAuraSurface();
  if (zaura_surface && zaura_surface->SupportsUnsetSnap()) {
    switch (snap_direction) {
      case WaylandWindowSnapDirection::kPrimary:
        zaura_surface->SetSnapLeft();
        return;
      case WaylandWindowSnapDirection::kSecondary:
        zaura_surface->SetSnapRight();
        return;
      case WaylandWindowSnapDirection::kNone:
        zaura_surface->UnsetSnap();
        return;
    }
  }
  // Window snapping isn't available for non-lacros builds.
  NOTIMPLEMENTED_LOG_ONCE();
}

void WaylandToplevelWindow::SetCanGoBack(bool value) {
  if (auto* zaura_surface = GetZAuraSurface()) {
    if (value) {
      zaura_surface->SetCanGoBack();
    } else {
      zaura_surface->UnsetCanGoBack();
    }
  }
}

void WaylandToplevelWindow::SetPip() {
  if (auto* zaura_surface = GetZAuraSurface()) {
    zaura_surface->SetPip();
  }
}

void WaylandToplevelWindow::Lock(WaylandOrientationLockType lock_type) {
  shell_toplevel_->Lock(lock_type);
}

void WaylandToplevelWindow::Unlock() {
  shell_toplevel_->Unlock();
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

int WaylandToplevelWindow::GetNumberOfDesks() const {
  auto* zaura_shell = connection()->zaura_shell();
  return zaura_shell ? zaura_shell->GetNumberOfDesks() : 0;
}

int WaylandToplevelWindow::GetActiveDeskIndex() const {
  auto* zaura_shell = connection()->zaura_shell();
  // The index of the active desk is 0 when there is no virtual desk supported.
  return zaura_shell ? zaura_shell->GetActiveDeskIndex() : 0;
}

std::u16string WaylandToplevelWindow::GetDeskName(int index) const {
  auto* zaura_shell = connection()->zaura_shell();
  return zaura_shell ? base::UTF8ToUTF16(zaura_shell->GetDeskName(index))
                     : std::u16string();
}

void WaylandToplevelWindow::SendToDeskAtIndex(int index) {
  if (auto* zaura_surface = GetZAuraSurface()) {
    zaura_surface->MoveToDesk(index);
  }
}

void WaylandToplevelWindow::Pin(bool trusted) {
  if (SupportsConfigurePinnedState()) {
    auto new_state = trusted ? PlatformWindowState::kTrustedPinnedFullscreen
                             : PlatformWindowState::kPinnedFullscreen;
    SetWindowState(new_state, display::kInvalidDisplayId);
  } else {
    if (auto* zaura_surface = GetZAuraSurface()) {
      zaura_surface->SetPin(trusted);
    }
  }
}

void WaylandToplevelWindow::Unpin() {
  if (SupportsConfigurePinnedState()) {
    auto new_state = previously_maximized_ ? PlatformWindowState::kMaximized
                                           : PlatformWindowState::kNormal;
    SetWindowState(new_state, display::kInvalidDisplayId);
  } else {
    if (auto* zaura_surface = GetZAuraSurface()) {
      zaura_surface->UnsetPin();
    }
  }
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
      << ", restore_session_id=" << restore_session_id_;
  if (restore_window_id_source_) {
    out << ", source=" << *restore_window_id_source_;
  }
  out << ", persistable=" << ToBoolString(persistable_)
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
  SendToDeskAtIndex(always_visible ? kVisibleOnAllWorkspaces
                                   : GetActiveDeskIndex());
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
    } else if (window_state == PlatformWindowState::kPinnedFullscreen ||
               window_state == PlatformWindowState::kTrustedPinnedFullscreen) {
      if (auto* zaura_surface = GetZAuraSurface()) {
        zaura_surface->SetPin(window_state ==
                              PlatformWindowState::kTrustedPinnedFullscreen);
      }
    } else if (GetLatestRequestedState().window_state ==
               PlatformWindowState::kFullScreen) {
      shell_toplevel_->UnSetFullscreen();
    } else if (GetLatestRequestedState().window_state ==
                   PlatformWindowState::kPinnedFullscreen ||
               GetLatestRequestedState().window_state ==
                   PlatformWindowState::kTrustedPinnedFullscreen) {
      if (auto* zaura_surface = GetZAuraSurface()) {
        zaura_surface->UnsetPin();
      }
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

  shell_toplevel_->SetCanMaximize(delegate()->CanMaximize());
  shell_toplevel_->SetCanFullscreen(delegate()->CanFullscreen());

  connection()->Flush();
}

void WaylandToplevelWindow::SetUpShellIntegration() {
  // This method should be called after the XDG surface is initialized.
  DCHECK(shell_toplevel_);
  if (connection()->zaura_shell()) {
    if (auto* zaura_surface = root_surface()->CreateZAuraSurface()) {
      zaura_surface->set_delegate(AsWeakPtr());

      // If the server does not support the synchronized occlusion pathway,
      // enable the unsynchronized occlusion pathway and disable native
      // occlusion.
      if (!shell_toplevel_->IsSupportedOnAuraToplevel(
              ZAURA_TOPLEVEL_CONFIGURE_OCCLUSION_STATE_SINCE_VERSION)) {
        zaura_surface->SetOcclusionTracking();
        delegate()->DisableNativeWindowOcclusion();
      }
    }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    SetImmersiveFullscreenStatus(false);

    if (shell_toplevel_->IsSupportedOnAuraToplevel(
            ZAURA_TOPLEVEL_SET_PERSISTABLE_SINCE_VERSION)) {
      shell_toplevel_->SetPersistable(persistable_);
    }
#endif

    // We pass the value of `z_order_` to the `shell_toplevel_` here in order to
    // set the initial z order of the window.
    SetZOrderLevel(z_order_);
    SetInitialWorkspace();
    if (restore_window_id_) {
      DCHECK(!restore_window_id_source_);
      shell_toplevel_->SetRestoreInfo(restore_session_id_,
                                      restore_window_id_.value());
    } else if (restore_window_id_source_) {
      shell_toplevel_->SetRestoreInfoWithWindowIdSource(
          restore_session_id_, restore_window_id_source_.value());
    }
    UpdateSystemModal();
  }

  // We must not request a new GtkSurface if we already have one, else we get a
  // "gtk_shell::get_gtk_surface already requested" error. (crbug.com/1380419)
  if (connection()->gtk_shell1() && !gtk_surface1_) {
    gtk_surface1_ =
        connection()->gtk_shell1()->GetGtkSurface1(root_surface()->surface());
  }
}

void WaylandToplevelWindow::OnDecorationModeChanged() {
  DCHECK(shell_toplevel_);
  auto* zaura_surface = GetZAuraSurface();
  if (use_native_frame_) {
    // Set server-side decoration for windows using a native frame,
    // e.g. taskmanager
    shell_toplevel_->SetDecoration(
        ShellToplevelWrapper::DecorationMode::kServerSide);
  } else if (zaura_surface && zaura_surface->SupportsSetServerStartResize()) {
    // Sets custom-decoration mode for window that supports aura_shell.
    // e.g. lacros-browser.
    zaura_surface->SetServerStartResize();
  } else {
    shell_toplevel_->SetDecoration(
        ShellToplevelWrapper::DecorationMode::kClientSide);
  }
}

void WaylandToplevelWindow::OnFrameLockingChanged(bool lock) {
  DCHECK(delegate());
  delegate()->OnSurfaceFrameLockingChanged(lock);
}

void WaylandToplevelWindow::OnDeskChanged(int state) {
  DCHECK(delegate());
  workspace_ = state;
  if (workspace_extension_delegate_)
    workspace_extension_delegate_->OnWorkspaceChanged();
}

void WaylandToplevelWindow::SetInitialWorkspace() {
  if (!workspace_.has_value())
    return;

  if (auto* zaura_surface = GetZAuraSurface()) {
    zaura_surface->SetInitialWorkspace(workspace_.value());
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

void WaylandToplevelWindow::SetFloatToLocation(
    WaylandFloatStartLocation float_start_location) {
  CHECK(shell_toplevel_);
  shell_toplevel_->SetFloatToLocation(float_start_location);
}

void WaylandToplevelWindow::UnSetFloat() {
  CHECK(shell_toplevel_);
  shell_toplevel_->UnSetFloat();
}

}  // namespace ui
