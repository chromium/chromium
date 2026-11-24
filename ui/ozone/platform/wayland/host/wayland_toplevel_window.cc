// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "base/logging.h"
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
#include "ui/ozone/platform/wayland/host/dump_util.h"
#include "ui/ozone/platform/wayland/host/org_kde_kwin_appmenu.h"
#include "ui/ozone/platform/wayland/host/wayland_bubble.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_data_drag_controller.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_frame_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_popup.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window_drag_controller.h"
#include "ui/ozone/platform/wayland/host/wayland_zwp_pointer_constraints.h"
#include "ui/ozone/platform/wayland/host/xdg_activation.h"
#include "ui/ozone/platform/wayland/host/xdg_session.h"
#include "ui/ozone/platform/wayland/host/xdg_surface.h"
#include "ui/ozone/platform/wayland/host/xdg_toplevel.h"
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

WaylandToplevelWindow::~WaylandToplevelWindow() {
  set_shutting_down();
  // Child popups must be unmapped before `xdg_toplevel_` is destroyed.
  HideChildPopups();
}

bool WaylandToplevelWindow::CreateXdgToplevel() {
  if (auto xdg_surface = std::make_unique<XdgSurface>(this, connection())) {
    if (xdg_surface->Initialize()) {
      auto xdg_toplevel = std::make_unique<XdgToplevel>(std::move(xdg_surface));
      if (xdg_toplevel && xdg_toplevel->Initialize()) {
        xdg_toplevel_ = std::move(xdg_toplevel);
      }
    }
  }
  if (!xdg_toplevel_) {
    LOG(ERROR) << "Failed to create a XdgToplevel.";
    return false;
  }

  xdg_toplevel_->SetAppId(app_id_);
  xdg_toplevel_->SetTitle(window_title_);
  SetSizeConstraints();
  if (base::FeatureList::IsEnabled(features::kAsyncFullscreenWindowState)) {
    if (fullscreen_state_ == FullscreenState::kClient) {
      SetFullscreen(true, display::kInvalidDisplayId);
    } else {
      switch (GetPlatformWindowState()) {
        case PlatformWindowState::kMaximized:
          Maximize();
          break;
        case PlatformWindowState::kMinimized:
          Minimize();
          break;
        default:
          break;
      }
    }
  } else {
    TriggerStateChanges(GetPlatformWindowState(), display::kInvalidDisplayId);
  }
  SetUpShellIntegration();
  OnDecorationModeChanged();

  // If session management is supported and session data was passed in at
  // construction time, with a valid `restore_id`, restoring this toplevel
  // must be done now, before the first wl_surface commit. The remaining
  // handling steps are done when the first xdg_surface.configure comes in
  // (see HandleToplevelConfigure and UpdateSessionStateIfNeeded).
  if (IsRestoredFromSession()) {
    toplevel_session_ = session_->TrackToplevel(
        this, session_data_->restore_id.value(), XdgSession::Action::kRestore);
  }

  if (!initial_icon_.isNull()) {
    SetWindowIcons(gfx::ImageSkia(), initial_icon_);
    initial_icon_ = gfx::ImageSkia();
  }

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
  if (!xdg_toplevel_) {
    return;
  }

  if (hittest == HTCAPTION) {
    xdg_toplevel_->SurfaceMove(connection());
  } else {
    xdg_toplevel_->SurfaceResize(connection(), hittest);
  }

  connection()->Flush();
  // TODO(crbug.com/40917147): Revisit to resolve the correct impl.
  // `event_source` is owned by the connection, so it outlives this window even
  // if dispatching the events below destroys it.
  auto* event_source = connection()->event_source();
  event_source->ReleasePressedPointerButtons(this, EventTimeForNow());
  // The compositor now owns any touch sequence that started the move, and need
  // not send wl_touch.up or wl_touch.cancel for it, so end it here.
  event_source->CancelActiveTouches(EventTimeForNow());
}

void WaylandToplevelWindow::Show(bool inactive) {
  if (hide_deferred_) {
    // Finish the deferred Hide() now and map the window again from scratch, as
    // configure events have been ignored meanwhile.
    hide_deferred_ = false;
    auto weak_this = AsWeakPtr();
    HideNow();
    if (!weak_this) {
      return;
    }
  }

  if (xdg_toplevel_) {
    return;
  }

  if (!CreateXdgToplevel()) {
    Close();
    return;
  }

  UpdateWindowScale(false);

  if (inactive) {
    Deactivate();
  } else {
    Activate();
  }

  WaylandWindow::Show(inactive);
}

void WaylandToplevelWindow::Hide() {
  if (!xdg_toplevel_ || hide_deferred_) {
    return;
  }

  // If this window is the requestor of an in-flight activation token request
  // (e.g. it activated another window right before being hidden), keep it
  // mapped until the token has been used. Some compositors (e.g. Mutter) check
  // that the requestor surface still has input focus at that point.
  if (const auto* xdg_activation = connection()->xdg_activation()) {
    if (xdg_activation->RunAfterTokenRequestFrom(
            this, base::BindOnce(&WaylandToplevelWindow::OnDeferredHide,
                                 weak_ptr_factory_.GetWeakPtr()))) {
      hide_deferred_ = true;
      return;
    }
  }

  HideNow();
}

void WaylandToplevelWindow::HideNow() {
  if (!xdg_toplevel_) {
    return;
  }

  auto weak_this = AsWeakPtr();
  // Deactivate the active bubble first so that hiding children doesn't
  // reactivate this hiding toplevel.
  if (WaylandBubble* prev = std::exchange(active_bubble_, nullptr)) {
    ++bubble_activation_epoch_;
    if (!prev->shutting_down()) {
      prev->delegate()->OnActivationChanged(false);
      if (!weak_this || !xdg_toplevel_) {
        return;
      }
    }
  }
  HideChildPopups();
  if (!weak_this || !xdg_toplevel_) {
    return;
  }
  set_child_popup(nullptr);
  std::vector<base::WeakPtr<WaylandWindow>> bubbles;
  bubbles.reserve(child_bubbles().size());
  for (WaylandBubble* bubble : child_bubbles()) {
    bubbles.push_back(bubble->AsWeakPtr());
  }
  for (const auto& bubble : bubbles) {
    if (bubble) {
      bubble->Hide();
    }
    if (!weak_this || !xdg_toplevel_) {
      return;
    }
  }

  // Note that the xdg_toplevel object should be destroyed before we touch
  // anything else in order to provide the compositor a good reference point
  // when the window contents can be frozen in case a window closing animation
  // needs to be played. Ideally, the xdg_toplevel object should also be
  // destroyed before any subsurface is destroyed, otherwise the window may have
  // missing contents when the compositor animates it.
  //
  // The xdg-shell spec provides another way to hide a window: attach a nil
  // buffer to the root surface. However, compositors often get it wrong, and it
  // makes sense only if the xdg_toplevel object is going to be reused, which is
  // not the case here.
  xdg_toplevel_.reset();
  if (fullscreen_state_ == FullscreenState::kCompositor) {
    fullscreen_state_ = FullscreenState::kNone;
  }
  last_configured_window_state_ = PlatformWindowState::kUnknown;
  minimize_sync_callback_.reset();
  fullscreen_sync_callback_.reset();
  last_sent_geometry_size_dip_ = gfx::Size();
  ClearPendingBoundsSync();
  toplevel_session_.reset();
  appmenu_.reset();

  WaylandWindow::Hide();
  ClearInFlightRequestsSerial();

  connection()->Flush();
}

bool WaylandToplevelWindow::IsVisible() const {
  // X and Windows return true if the window is minimized. For consistency, do
  // the same.
  return xdg_toplevel_ && !hide_deferred_;
}

void WaylandToplevelWindow::OnDeferredHide() {
  if (!hide_deferred_) {
    return;
  }
  hide_deferred_ = false;
  HideNow();
}

void WaylandToplevelWindow::SetTitle(const std::u16string& title) {
  if (window_title_ == title) {
    return;
  }

  window_title_ = title;

  if (xdg_toplevel_) {
    xdg_toplevel_->SetTitle(title);
    connection()->Flush();
  }
}

void WaylandToplevelWindow::SetWindowClass(const std::string& wm_class_name,
                                           const std::string& wm_class_class,
                                           const std::string& wayland_app_id) {
  if (app_id_ == wayland_app_id) {
    return;
  }

  app_id_ = wayland_app_id;

  // Unlike most xdg_toplevel state, the app_id isn't double-buffered, and may
  // be changed at any time. Compositors re-evaluate the window's application
  // when it changes.
  if (xdg_toplevel_) {
    xdg_toplevel_->SetAppId(app_id_);
    connection()->Flush();
  }
}

void WaylandToplevelWindow::SetFullscreen(bool fullscreen,
                                          int64_t target_display_id) {
  // TODO(msisov, tonikitoo): add multiscreen support. As the documentation says
  // if xdg_toplevel_set_fullscreen() is not provided with wl_output, it's up
  // to the compositor to choose which display will be used to map this surface.

  // The `target_display_id` must be invalid if not going into fullscreen.
  CHECK(fullscreen || target_display_id == display::kInvalidDisplayId,
        base::NotFatalUntil::M163);

  if (base::FeatureList::IsEnabled(features::kAsyncFullscreenWindowState)) {
    if (!xdg_toplevel_) {
      fullscreen_state_ =
          fullscreen ? FullscreenState::kClient : FullscreenState::kNone;
      return;
    }
    if (fullscreen) {
      ClearPendingBoundsSync();
      SendSetFullscreen(target_display_id);
      auto state = GetLatestRequestedState();
      // Compositors such as Sway and Mutter do not emit a new configure when
      // set_fullscreen is requested on an already-fullscreen toplevel.
      if (last_configured_window_state_ == PlatformWindowState::kFullScreen &&
          !pending_minimize_ &&
          state.window_state != PlatformWindowState::kMinimized) {
        state.window_state = PlatformWindowState::kFullScreen;
        RequestStateFromClient(state);
      }
    } else {
      SendUnsetFullscreen();
      if (previously_maximized_) {
        xdg_toplevel_->SetMaximized();
      }
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
  if (base::FeatureList::IsEnabled(features::kAsyncFullscreenWindowState)) {
    if (!xdg_toplevel_) {
      fullscreen_state_ = FullscreenState::kNone;
      return;
    }
    if (fullscreen_state_ != FullscreenState::kNone) {
      SendUnsetFullscreen();
    }
    ClearPendingBoundsSync();
    xdg_toplevel_->SetMaximized();
  } else {
    SetWindowState(PlatformWindowState::kMaximized);
  }
}

void WaylandToplevelWindow::Minimize() {
  if (!xdg_toplevel_) {
    // TODO(crbug.com/40276379): Store `PlatformWindowState::kMinimized` to a
    // pending state.
    return;
  }

  if (IsSurfaceConfigured()) {
    ClearPendingBoundsSync();
    if (base::FeatureList::IsEnabled(features::kAsyncFullscreenWindowState)) {
      SendSetMinimized();
      pending_minimize_ = true;
    } else {
      auto previous_state = applied_state().window_state;
      UpdatePreviouslyMaximized(GetRawAppliedWindowState(),
                                PlatformWindowState::kMinimized);
      fullscreen_sync_callback_.reset();
      SendSetMinimized();
      ForceApplyWindowStateDoNotUse(PlatformWindowState::kMinimized);
      delegate()->OnWindowStateChanged(previous_state,
                                       PlatformWindowState::kMinimized);
    }
  }
}

void WaylandToplevelWindow::Restore() {
  CHECK(xdg_toplevel_, base::NotFatalUntil::M163);

  // Differently from other platforms, under Wayland, unmaximizing the dragged
  // window before starting the drag loop is not needed as it is assumed to be
  // handled at compositor side, just like in xdg_toplevel_surface::move. So
  // skip it if there's a window drag session running.
  auto* drag_controller = connection()->window_drag_controller();
  if (drag_controller && drag_controller->IsDragInProgress()) {
    return;
  }

  if (base::FeatureList::IsEnabled(features::kAsyncFullscreenWindowState)) {
    if (!xdg_toplevel_) {
      return;
    }
    PlatformWindowState current_state = applied_state().window_state;
    // If the window is minimized, the DesktopWindowTreeHostPlatform::Restore()
    // calls DesktopWindowTreeHostPlatform::Show() which unminimizes the window.
    if (current_state == PlatformWindowState::kMinimized) {
      return;
    }
    if (fullscreen_state_ != FullscreenState::kNone) {
      SetFullscreen(false, display::kInvalidDisplayId);
    } else if (current_state == PlatformWindowState::kMaximized) {
      xdg_toplevel_->UnSetMaximized();
    }
  } else {
    SetWindowState(previously_maximized_ ? PlatformWindowState::kMaximized
                                         : PlatformWindowState::kNormal);
  }
}

void WaylandToplevelWindow::ShowWindowControlsMenu(const gfx::Point& point) {
  if (xdg_toplevel_) {
    xdg_toplevel_->ShowWindowMenu(
        connection(),
        gfx::ScaleToRoundedPoint(point, applied_state().ui_scale));
  }
}

void WaylandToplevelWindow::ActivateWithToken(std::string token) {
  CHECK(connection()->xdg_activation(), base::NotFatalUntil::M163);

  // Stacking the dragged xdg toplevel as the topmost one (and tied to the
  // pointer cursor) is reponsibility of the Wayland compositor, so bail out
  // if `this` is currently being dragged.
  if (auto* drag_controller = connection()->window_drag_controller()) {
    if (drag_controller->IsDraggingWindow(this)) {
      return;
    }
  }

  if (IsSurfaceConfigured()) {
    connection()->xdg_activation()->Activate(root_surface()->surface(), token);
  } else {
    pending_configure_activation_token_ = token;
  }
}

void WaylandToplevelWindow::Activate() {
  if (connection()->xdg_activation()) {
    // While a session-restored toplevel is not yet configured, leave any
    // launch token in `base::nix` for a window opened for the launch itself
    // (if any). Once configured, runtime tokens (e.g. from external links or
    // notifications) may activate it.
    const bool restoring_from_session =
        IsRestoredFromSession() && !IsSurfaceConfigured();
    if (pending_configure_activation_token_.has_value()) {
      if (IsSurfaceConfigured()) {
        connection()->xdg_activation()->Activate(
            root_surface()->surface(),
            pending_configure_activation_token_.value());
        pending_configure_activation_token_.reset();
      }
    } else if (auto token = restoring_from_session
                                ? std::nullopt
                                : base::nix::TakeXdgActivationToken()) {
      ActivateWithToken(token.value());
    } else if (!IsRestoredFromSession() ||
               connection()->xdg_activation()->HasUserPressSerial()) {
      // A toplevel restored from a session is placed by the compositor, e.g.
      // on another workspace. A token requested without a user press serial
      // (including one with only a wl_pointer.enter serial from mapping under
      // the cursor) cannot activate it on compositors such as Mutter, which
      // mark the window as demanding attention instead, so do not request one
      // until there is a press event.
      connection()->xdg_activation()->RequestNewToken(
          base::BindOnce(&WaylandToplevelWindow::ActivateWithToken,
                         weak_ptr_factory_.GetWeakPtr()));
    }
    connection()->Flush();
  }
  SetActiveBubble(nullptr);
}

void WaylandToplevelWindow::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                           const gfx::ImageSkia& app_icon) {
  if (!xdg_toplevel_) {
    return;
  }
  // Let the app icon take precedence over the window icon.
  if (!app_icon.isNull()) {
    xdg_toplevel_->SetIcon(app_icon);
  } else if (!window_icon.isNull()) {
    xdg_toplevel_->SetIcon(window_icon);
  } else {
    // Don't reset the icon if a null icon is passed in. There are callers
    // that attempt to set a null icon after the initial icon has been set,
    // but don't intend to reset the icon. This matches the behavior of the
    // X11 backend.
    return;
  }
}

void WaylandToplevelWindow::SizeConstraintsChanged() {
  // Size constraints only make sense for normal windows.
  if (!xdg_toplevel_) {
    return;
  }

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
  if (use_native_frame_ == use_native_frame) {
    return;
  }
  use_native_frame_ = use_native_frame;
  if (xdg_toplevel_) {
    frame_type_changed_ = true;
    OnDecorationModeChanged();
  }

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

void WaylandToplevelWindow::UpdateWindowScale(bool update_bounds) {
  auto old_scale = applied_state().window_scale;
  auto old_ui_scale = applied_state().ui_scale;
  auto weak_this = AsWeakPtr();
  WaylandWindow::UpdateWindowScale(update_bounds);
  if (!weak_this) {
    return;
  }
  if (old_scale == applied_state().window_scale &&
      old_ui_scale == applied_state().ui_scale) {
    return;
  }

  // Update min/max size if buffer or ui scale is updated.
  SizeConstraintsChanged();
}

WaylandToplevelWindow* WaylandToplevelWindow::AsWaylandToplevelWindow() {
  return this;
}

const WaylandToplevelWindow* WaylandToplevelWindow::AsWaylandToplevelWindow()
    const {
  return this;
}

void WaylandToplevelWindow::SetActiveBubble(WaylandBubble* new_bubble) {
  // Like Aura activation, a bubble may become active within an unfocused
  // toplevel (e.g. one shown before the compositor activates the toplevel).
  const auto can_activate = [this]() {
    return !shutting_down() && !hide_deferred_;
  };
  if (new_bubble) {
    CHECK(new_bubble->activatable());
    CHECK_EQ(new_bubble->GetRootParentWindow(), this);
    if (!can_activate() || !new_bubble->IsVisible() ||
        new_bubble->shutting_down()) {
      return;
    }
  }
  if (active_bubble_ == new_bubble) {
    return;
  }

  const uint64_t epoch = ++bubble_activation_epoch_;
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  base::WeakPtr<WaylandWindow> weak_new =
      new_bubble ? new_bubble->AsWeakPtr() : nullptr;

  if (WaylandBubble* prev = std::exchange(active_bubble_, nullptr)) {
    if (!prev->shutting_down()) {
      prev->delegate()->OnActivationChanged(false);
    }
  } else if (new_bubble) {
    delegate()->OnActivationChanged(false);
  }
  if (!weak_this || bubble_activation_epoch_ != epoch || !can_activate()) {
    return;
  }

  if (weak_new && new_bubble->IsVisible() && !new_bubble->shutting_down() &&
      new_bubble->GetRootParentWindow() == this) {
    active_bubble_ = new_bubble;
    new_bubble->delegate()->OnActivationChanged(true);
  } else if (is_active_) {
    delegate()->OnActivationChanged(true);
  }
}

bool WaylandToplevelWindow::IsInBubbleAncestry(
    const WaylandWindow* window) const {
  for (const WaylandWindow* cur = active_bubble_; cur;
       cur = cur->parent_window()) {
    if (cur == window) {
      return true;
    }
  }
  return false;
}

void WaylandToplevelWindow::UpdateActivationState() {
  bool prev_is_active = is_active_;

  // Determine active state from keyboard focus. If keyboard is unavailable,
  // determine it from zwp_text_input_v3::{enter,leave}.
  // If neither of those are available, use xdg-shell "activated" state as
  // that's the only other hint the compositor provides us on whether our window
  // is considered active.
  if (connection()->IsKeyboardAvailable()) {
    auto* keyboard_focused_window =
        connection()->window_manager()->GetCurrentKeyboardFocusedWindow();
    is_active_ = keyboard_focused_window &&
                 keyboard_focused_window->GetRootParentWindow() == this;
  } else if (connection()->SupportsTextInputFocus()) {
    // Note: Some compositors (sway, niri, cosmic etc.) may not send
    // text-input-v3 enter/leave events if an IM framework is not
    // installed/running. So text input focus cannot be used always instead of
    // keyboard focus above. However, if there is no physical keyboard, there
    // should be an IM framework to facilitate inputting text in some way, e.g.
    // using a virtual keyboard, and so it should be okay to expect focus to be
    // received from text-input in that case if text-input-v3 is available.
    auto* text_input_focused_window =
        connection()->window_manager()->GetCurrentTextInputFocusedWindow();
    is_active_ = text_input_focused_window &&
                 text_input_focused_window->GetRootParentWindow() == this;
  } else {
    is_active_ = is_xdg_active_;
  }

  if (prev_is_active != is_active_) {
    ++bubble_activation_epoch_;
    if (is_active_) {
      // A bubble activated while unfocused stays active.
      if (!active_bubble_) {
        delegate()->OnActivationChanged(true);
      }
    } else if (WaylandBubble* prev = std::exchange(active_bubble_, nullptr)) {
      if (!prev->shutting_down()) {
        prev->delegate()->OnActivationChanged(false);
      }
    } else {
      delegate()->OnActivationChanged(false);
    }
  }
}

void WaylandToplevelWindow::HandleToplevelConfigure(
    int32_t width_dip,
    int32_t height_dip,
    const WindowStates& window_states) {
  if (hide_deferred_) {
    return;
  }
  // HandleToplevelConfigureWithOrigin() calls into the delegate
  // (OnActivationChanged et al.), which the views layer documents may
  // synchronously close the widget and destroy this platform window. See
  // DesktopWindowTreeHostPlatform::OnActivationChanged().
  auto alive = weak_ptr_factory_.GetWeakPtr();
  HandleToplevelConfigureWithOrigin(0, 0, width_dip, height_dip, window_states);
  if (!alive) {
    return;
  }
  UpdateSessionStateIfNeeded();
}

PlatformWindowState WaylandToplevelWindow::UpdateConfiguredWindowState(
    int32_t width_dip,
    int32_t height_dip,
    const WindowStates& window_states) {
  const bool async_fullscreen =
      base::FeatureList::IsEnabled(features::kAsyncFullscreenWindowState);
  // The compositor does not report the minimized state, so a client-requested
  // minimize is assumed to persist until the compositor activates the window.
  // An activated configure sent before the compositor processed set_minimized
  // does not count, since it predates the minimize.
  const bool can_unminimize =
      window_states.is_activated && !minimize_sync_callback_;
  const bool is_minimized = window_states.is_minimized ||
                            ((GetLatestRequestedState().window_state ==
                                  PlatformWindowState::kMinimized ||
                              (async_fullscreen && pending_minimize_)) &&
                             !can_unminimize);
  pending_minimize_ = false;
  const PlatformWindowState compositor_state =
      window_states.is_minimized    ? PlatformWindowState::kMinimized
      : window_states.is_fullscreen ? PlatformWindowState::kFullScreen
      : window_states.is_maximized  ? PlatformWindowState::kMaximized
                                    : PlatformWindowState::kNormal;
  // Some compositors (e.g. Sway) send a 0x0 unactivated configure for the
  // initial commit before applying the requested state on map.
  const bool is_pre_map_configure =
      !IsSurfaceConfigured() && width_dip == 0 && height_dip == 0 &&
      !window_states.is_activated &&
      compositor_state == PlatformWindowState::kNormal;
  // Configures sent before the compositor processed set_fullscreen do not yet
  // report fullscreen.
  const bool is_pending_fullscreen =
      fullscreen_state_ == FullscreenState::kClient &&
      (fullscreen_sync_callback_ || is_pre_map_configure);

  // `raw_window_state` reports any xdg_toplevel fullscreen as kFullScreen.
  PlatformWindowState raw_window_state = compositor_state;
  if (is_minimized) {
    raw_window_state = PlatformWindowState::kMinimized;
  } else if (window_states.is_fullscreen) {
    if (fullscreen_state_ == FullscreenState::kNone) {
      fullscreen_state_ = FullscreenState::kCompositor;
    }
  } else if (is_pending_fullscreen) {
    if (!async_fullscreen) {
      raw_window_state = PlatformWindowState::kFullScreen;
    }
  } else {
    fullscreen_state_ = FullscreenState::kNone;
  }

  // The compositor does not report the minimized state, and a configure that
  // predates set_fullscreen does not yet report fullscreen, but either still
  // reports the state underneath, which is what Restore() or exiting
  // fullscreen should return to. Use that instead of the previous configure
  // while the client keeps the window minimized or fullscreen.
  if (!is_pre_map_configure) {
    const PlatformWindowState previous_state =
        raw_window_state != compositor_state ? compositor_state
                                             : last_configured_window_state_;
    UpdatePreviouslyMaximized(previous_state, raw_window_state);
    // Some compositors (e.g. Mutter) keep reporting the maximized state under
    // fullscreen.
    if (window_states.is_fullscreen && window_states.is_maximized) {
      previously_maximized_ = true;
    }
    last_configured_window_state_ = raw_window_state;
  }

  // Follow X11 / EWMH (crbug.com/1035772, crbug.com/374101953): map
  // compositor-initiated fullscreen to kMaximized so browser chrome stays
  // visible while filling the output without frame borders or shadows.
  return raw_window_state == PlatformWindowState::kFullScreen &&
                 fullscreen_state_ != FullscreenState::kClient
             ? PlatformWindowState::kMaximized
             : raw_window_state;
}

void WaylandToplevelWindow::HandleToplevelConfigureWithOrigin(
    int32_t x,
    int32_t y,
    int32_t width_dip,
    int32_t height_dip,
    const WindowStates& window_states) {
  // TODO(crbug.com/369952980): Remove once arrays get logged by libwayland.
  VLOG(3) << __func__ << " states=[ " << window_states.ToString() << "]";

  const PlatformWindowState window_state =
      UpdateConfiguredWindowState(width_dip, height_dip, window_states);
  const bool frame_type_changed = frame_type_changed_;
  frame_type_changed_ = false;

  // Update state before notifying delegate.
  bool prev_xdg_active = is_xdg_active_;
  is_xdg_active_ = window_states.is_activated;
  // xdg_toplevel::activated is a paint-only hint, separate from input
  // activation which is driven by keyboard focus in UpdateActivationState.
  if (prev_xdg_active != is_xdg_active_) {
    auto weak_this = AsWeakPtr();
    delegate()->OnPaintAsActiveChanged(is_xdg_active_);
    if (!weak_this) {
      return;
    }
  }
  bool prev_suspended = is_suspended_;
  is_suspended_ = window_states.is_suspended;

  // The tiled state affects the window geometry, so apply it here.
  // TODO(crbug.com/414831391): Remove this and notify in
  // WindowTreeHostPlatform::OnStateUpdate instead like all other state changes.
  // The only issue there is when doing that a regression was seen in kwin. See
  // the bug description for additional details.
  if (window_states.tiled_edges != applied_state().tiled_edges) {
    // This configure changes the decoration insets.  We should adjust the
    // bounds appropriately.
    auto weak_this = AsWeakPtr();
    delegate()->OnWindowTiledStateChanged(window_states.tiled_edges);
    if (!weak_this) {
      return;
    }
  }

  pending_configure_state_.tiled_edges = window_states.tiled_edges;
  pending_configure_state_.window_state = window_state;

  // Width or height set to 0 means that we should decide on width and height by
  // ourselves, but we don't want to set them to anything else. Use restored
  // bounds size or the current bounds when the current state is normal, or keep
  // the size from an earlier configure in this batch (falling back to the
  // current bounds) when maximized or fullscreen.
  //
  // Note: if the browser was started with --start-fullscreen and a user exits
  // the fullscreen mode, wayland may set the width and height to be 1. Instead,
  // explicitly set the bounds to the current desired ones or the previous
  // bounds.
  gfx::Rect bounds_dip(
      pending_configure_state_.bounds_dip.value_or(gfx::Rect()));
  const bool is_stale_fullscreen_configure =
      window_state == PlatformWindowState::kFullScreen &&
      !window_states.is_fullscreen;
  // The compositor may echo the last committed geometry on state-only
  // configures. Ignore these during a client-initiated resize via
  // SetBoundsInDIP() (see OnStateRequestApplied() for the sequence).
  const bool is_stale_bounds_configure =
      ShouldSetBounds(window_state) &&
      window_states.tiled_edges == WindowTiledEdges() &&
      !window_states.is_resizing &&
      (pending_bounds_viz_seq_.has_value() || bounds_sync_callback_) &&
      std::ranges::contains(stale_geometry_sizes_dip_,
                            gfx::Size(width_dip, height_dip));
  if (window_state == PlatformWindowState::kMinimized ||
      is_stale_bounds_configure ||
      (is_stale_fullscreen_configure &&
       (!window_states.is_maximized || width_dip <= 1 || height_dip <= 1))) {
    // Keep the current (restored) bounds while minimized. Otherwise the bounds
    // shrink to the no-shadow configure size, the throttled renderer keeps that
    // smaller buffer, and on un-minimize it is briefly shown mismatched against
    // the retained window geometry, shifting the content (Wayland-only).
    // Likewise, keep current bounds on configures that echo a pre-resize
    // geometry before a client-initiated resize has committed and synced, and
    // on non-maximized or 0x0 configures that predate set_fullscreen so
    // bounds_dip does not remain empty or shrink to a pre-fullscreen no-shadow
    // size; stale maximized configures are still acked and must obey the
    // configured size (crbug.com/576504456).
    bounds_dip = GetBoundsInDIP();
    pending_configure_state_.geometry_size_dip =
        applied_state().geometry_size_dip;
  } else if (width_dip > 1 && height_dip > 1) {
    ClearPendingBoundsSync();
    // The configured size is the exact window geometry size in Wayland DIP.
    const gfx::Size geometry_size_dip(width_dip, height_dip);
    pending_configure_state_.geometry_size_dip = geometry_size_dip;
    // UI Scale must be applied only for size coming from the server. Restored
    // and current dip bounds (used below) are already ui-scale'd.
    const float ui_scale = applied_state().ui_scale;
    bounds_dip =
        gfx::Rect(gfx::ScaleToEnclosingRectIgnoringError(
                      gfx::Rect(x, y, width_dip, height_dip), 1.f / ui_scale)
                      .origin(),
                  ShouldSetBounds(window_state)
                      ? GeometrySizeToBoundsSizeDIP(geometry_size_dip,
                                                    window_state, ui_scale)
                      : gfx::ScaleToEnclosingRectIgnoringError(
                            gfx::Rect(geometry_size_dip), 1.f / ui_scale)
                            .size());
  } else if (ShouldSetBounds(window_state)) {
    if (!restored_bounds_dip().IsEmpty()) {
      bounds_dip = restored_bounds_dip();
      pending_configure_state_.geometry_size_dip = GetRestoredGeometrySizeDIP();
    } else {
      bounds_dip = GetBoundsInDIP();
      pending_configure_state_.geometry_size_dip =
          applied_state().geometry_size_dip;
      // The frame type changed, so the bounds still have the old frame's
      // insets. Keep the window geometry size and derive the bounds from it
      // with the new insets (crbug.com/378939144).
      if (frame_type_changed && !applied_state().geometry_size_dip.IsEmpty()) {
        bounds_dip.set_size(GeometrySizeToBoundsSizeDIP(
            applied_state().geometry_size_dip, window_state,
            applied_state().ui_scale));
      }
    }
  } else if (bounds_dip.IsEmpty()) {
    // 0x0 maximized/fullscreen configure with no size from an earlier
    // configure in this batch: keep the current bounds.
    bounds_dip = GetBoundsInDIP();
    pending_configure_state_.geometry_size_dip =
        applied_state().geometry_size_dip;
  }

  if (ShouldSetBounds(window_state)) {
    const gfx::Rect adjusted_bounds_dip =
        AdjustBoundsToConstraintsDIP(bounds_dip);
    if (adjusted_bounds_dip.size() != bounds_dip.size()) {
      // Derived from the adjusted bounds in RequestState().
      pending_configure_state_.geometry_size_dip.reset();
    }
    bounds_dip = adjusted_bounds_dip;
  }
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
    SetRestoredGeometry(GetBoundsInDIP(), applied_state().geometry_size_dip);
  }

  auto alive = weak_ptr_factory_.GetWeakPtr();
  UpdateActivationState();
  if (!alive) {
    return;
  }
  if (prev_suspended != is_suspended_) {
    frame_manager()->OnWindowSuspensionChanged();
  }
}

void WaylandToplevelWindow::SetBoundsInDIP(const gfx::Rect& bounds_dip) {
  auto state = GetLatestRequestedState();
  const bool compositor_sized =
      state.window_state == PlatformWindowState::kMaximized ||
      state.window_state == PlatformWindowState::kFullScreen ||
      (ShouldSetBounds(state.window_state) &&
       state.tiled_edges != WindowTiledEdges());
  if (!compositor_sized || !IsSurfaceConfigured() ||
      state.geometry_size_dip.IsEmpty()) {
    WaylandWindow::SetBoundsInDIP(bounds_dip);
    return;
  }

  // The size of a maximized, fullscreen or tiled window is set by the
  // compositor, which may keep it at the configured size regardless of the
  // committed window geometry, and committing a different size while maximized
  // is a protocol error. Keep the configured size so that a client-initiated
  // resize (e.g. window.resizeTo()) does not shrink the window inside its tile
  // or work area (crbug.com/586203821). This matches OnStateRequestApplied(),
  // which treats any tiled edge as compositor-sized.
  state.bounds_dip.set_origin(bounds_dip.origin());
  RequestStateFromClient(state);
}

void WaylandToplevelWindow::SetOrigin(const gfx::Point& origin) {
  gfx::Rect new_bounds(origin, GetBoundsInDIP().size());
  WaylandWindow::SetBoundsInDIP(new_bounds);
}

void WaylandToplevelWindow::HandleSurfaceConfigure(uint32_t serial) {
  if (hide_deferred_) {
    return;
  }
  ProcessPendingConfigureState(serial);
}

void WaylandToplevelWindow::OnCloseRequest() {
  if (hide_deferred_) {
    return;
  }
  WaylandWindow::OnCloseRequest();
}

uint32_t WaylandToplevelWindow::DispatchEvent(const PlatformEvent& event) {
  if (hide_deferred_) {
    return POST_DISPATCH_STOP_PROPAGATION;
  }
  return WaylandWindow::DispatchEvent(event);
}

void WaylandToplevelWindow::OnSequencePoint(int64_t seq) {
  if (!xdg_toplevel_) {
    return;
  }

  ProcessSequencePoint(seq);
  MaybeApplyLatestStateRequest(/*force=*/false);
}

void WaylandToplevelWindow::OnFrameCommitted(int64_t seq) {
  WaylandWindow::OnFrameCommitted(seq);
  if (pending_bounds_viz_seq_.has_value() &&
      (seq == -1 || seq >= *pending_bounds_viz_seq_)) {
    pending_bounds_viz_seq_.reset();
    SendBoundsSync();
  }
}

bool WaylandToplevelWindow::OnInitialize(
    PlatformWindowInitProperties properties,
    PlatformWindowDelegate::State* state) {
  state->window_state = PlatformWindowState::kNormal;

  app_id_ = properties.wayland_app_id;
  if (!properties.startup_id.empty()) {
    pending_configure_activation_token_ = properties.startup_id;
  }
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
  if (properties.icon) {
    initial_icon_ = *properties.icon;
  }

  if (!properties.session_id.empty()) {
    session_data_ = PlatformSessionWindowData{
        .session_id = properties.session_id,
        .window_id = properties.session_window_new_id,
        .restore_id = properties.session_window_restore_id,
    };
    if (auto* session_manager = connection()->session_manager()) {
      session_ = session_manager->GetSession(session_data_->session_id);
      if (session_) {
        session_observer_.Observe(session_);
      }
    }
  }

  return true;
}

bool WaylandToplevelWindow::IsActive() const {
  return is_active_;
}

bool WaylandToplevelWindow::IsSuspended() const {
  return is_suspended_;
}

bool WaylandToplevelWindow::IsSurfaceConfigured() {
  return xdg_toplevel() ? xdg_toplevel()->IsConfigured() : false;
}

bool WaylandToplevelWindow::IsRestoredFromSession() const {
  return session_ && session_data_ && session_data_->restore_id.has_value();
}

void WaylandToplevelWindow::SetWindowGeometry(
    const PlatformWindowDelegate::State& state) {
  CHECK(connection()->SupportsSetWindowGeometry(), base::NotFatalUntil::M163);

  if (!xdg_toplevel_) {
    return;
  }

  // While minimized the window is not visible, and the compositor retains the
  // last committed window geometry. Skipping the update keeps the geometry at
  // its restored value (incl. the shadow-offset origin), so un-minimizing does
  // not move the geometry origin. Otherwise the origin flips to (0,0) here and
  // back on restore, and without a compensating wl_surface.offset the content
  // visibly jumps for a frame (Wayland-only).
  if (state.window_state == PlatformWindowState::kMinimized) {
    return;
  }

  // `geometry_size_dip` is already in Wayland DIP and excludes the decoration
  // insets, and is never empty. The insets are in UI DIP, so the origin must be
  // scaled by `ui_scale`.
  last_sent_geometry_size_dip_ = state.geometry_size_dip;
  xdg_toplevel_->SetWindowGeometry(gfx::Rect(
      GetWindowGeometryOriginInWaylandDIP(state), state.geometry_size_dip));
}

void WaylandToplevelWindow::AckConfigure(uint32_t serial) {
  // We cannot assume the xdg-toplevel is non-NULL because of a corner case in
  // drag n' drop. There could be times when the tab strip change is detected
  // while processing a configure event received from the compositor and hence
  // destroy the xdg-toplevel before an ACK is sent. See crbug.com/1512046 for
  // details.
  if (xdg_toplevel()) {
    xdg_toplevel()->AckConfigure(serial);
  }

  if (pending_configure_activation_token_.has_value()) {
    if (connection()->xdg_activation()) {
      connection()->xdg_activation()->Activate(
          root_surface()->surface(),
          pending_configure_activation_token_.value());
    }
    pending_configure_activation_token_.reset();
  }
}

base::WeakPtr<WaylandWindow> WaylandToplevelWindow::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool WaylandToplevelWindow::IsClientControlledWindowMovementSupported() const {
  auto* window_drag_controller = connection()->window_drag_controller();
  CHECK(window_drag_controller, base::NotFatalUntil::M163);
  return window_drag_controller->IsWindowDragProtocolAvailable();
}

bool WaylandToplevelWindow::ShouldReleaseCaptureForDrag(
    ui::OSExchangeData* data) const {
  auto* data_drag_controller = connection()->data_drag_controller();
  CHECK(data_drag_controller, base::NotFatalUntil::M163);
  return data_drag_controller->ShouldReleaseCaptureForDrag(data);
}

bool WaylandToplevelWindow::RunMoveLoop(const gfx::Vector2d& drag_offset) {
  CHECK(connection()->window_drag_controller(), base::NotFatalUntil::M163);
  return connection()->window_drag_controller()->Drag(this, drag_offset);
}

void WaylandToplevelWindow::EndMoveLoop() {
  CHECK(connection()->window_drag_controller(), base::NotFatalUntil::M163);
  connection()->window_drag_controller()->StopDragging();
}

void WaylandToplevelWindow::StartWindowDraggingSessionIfNeeded(
    ui::mojom::DragEventSource event_source,
    bool allow_system_drag) {
  CHECK(connection()->window_drag_controller(), base::NotFatalUntil::M163);
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
  if (enabled) {
    pointer_constraints->LockPointer(root_surface());
  } else {
    pointer_constraints->UnlockPointer();
  }
}

void WaylandToplevelWindow::SetAppmenu(const std::string& service_name,
                                       const std::string& object_path) {
  appmenu_service_name_ = service_name;
  appmenu_object_path_ = object_path;

  if (xdg_toplevel_) {
    TryAnnounceAppmenu();
  }
}

void WaylandToplevelWindow::UnsetAppmenu() {
  appmenu_.reset();
  appmenu_service_name_.clear();
  appmenu_object_path_.clear();
}

void WaylandToplevelWindow::SetSystemModal(bool modal) {
  system_modal_ = modal;
  if (xdg_toplevel_) {
    xdg_toplevel_->SetSystemModal(modal);
  }
}

void WaylandToplevelWindow::DumpState(std::ostream& out) const {
  WaylandWindow::DumpState(out);
  out << ", title=" << window_title_
      << ", is_active=" << ToBoolString(is_active_)
      << ", system_modal=" << ToBoolString(system_modal_);
}

void WaylandToplevelWindow::OnSessionDestroying() {
  toplevel_session_.reset();
  session_observer_.Reset();
  session_ = nullptr;
}

void WaylandToplevelWindow::UpdateSystemModal() {
  if (xdg_toplevel_) {
    xdg_toplevel_->SetSystemModal(system_modal_);
  }
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
    PlatformWindowState window_state,
    int64_t fullscreen_display_id) {
  CHECK(fullscreen_display_id == display::kInvalidDisplayId ||
        window_state == PlatformWindowState::kFullScreen);
  CHECK(!base::FeatureList::IsEnabled(features::kAsyncFullscreenWindowState));

  const bool was_fullscreen = fullscreen_state_ != FullscreenState::kNone;
  fullscreen_state_ = window_state == PlatformWindowState::kFullScreen
                          ? FullscreenState::kClient
                          : FullscreenState::kNone;
  if (window_state != PlatformWindowState::kFullScreen) {
    fullscreen_sync_callback_.reset();
  }
  if (!ShouldSetBounds(window_state)) {
    ClearPendingBoundsSync();
  }
  if (xdg_toplevel_) {
    // Call UnSetMaximized only if current state is normal. Otherwise, if the
    // current state is fullscreen and the previous is maximized, calling
    // UnSetMaximized may result in wrong restored window position that clients
    // are not allowed to know about.
    if (window_state == PlatformWindowState::kMinimized) {
      SendSetMinimized();
      pending_minimize_ = true;
    } else if (window_state == PlatformWindowState::kFullScreen) {
      SendSetFullscreen(fullscreen_display_id);
    } else if (window_state == PlatformWindowState::kMaximized) {
      if (was_fullscreen) {
        SendUnsetFullscreen();
      }
      xdg_toplevel_->SetMaximized();
    } else if (window_state == PlatformWindowState::kNormal) {
      if (was_fullscreen) {
        SendUnsetFullscreen();
      } else if (applied_state().window_state ==
                     PlatformWindowState::kMaximized ||
                 GetLatestRequestedState().window_state ==
                     PlatformWindowState::kMaximized) {
        // A client-initiated state is in applied_state() before any request
        // carries it, so check both for the state being left.
        xdg_toplevel_->UnSetMaximized();
      }
    }
  }

  auto previous_state = applied_state().window_state;
  ForceApplyWindowStateDoNotUse(window_state);
  if (previous_state != window_state) {
    auto weak_this = AsWeakPtr();
    delegate()->OnWindowStateChanged(previous_state, window_state);
    if (!weak_this) {
      return;
    }
  }
  connection()->Flush();
}

void WaylandToplevelWindow::SetWindowState(PlatformWindowState window_state,
                                           int64_t target_display_id) {
  CHECK(!base::FeatureList::IsEnabled(features::kAsyncFullscreenWindowState));
  if (ShouldTriggerStateChange(window_state, target_display_id)) {
    UpdatePreviouslyMaximized(GetRawAppliedWindowState(), window_state);
    TriggerStateChanges(window_state, target_display_id);
  }
}

bool WaylandToplevelWindow::ShouldTriggerStateChange(
    PlatformWindowState window_state,
    int64_t target_display_id) const {
  // Allow the state transition if the state is different. This includes
  // leaving compositor-initiated fullscreen (mapped to kMaximized) into genuine
  // kMaximized.
  if (applied_state().window_state != window_state ||
      GetRawAppliedWindowState() != window_state) {
    return true;
  }

  // Allow the state transition if the state is fullscreen and the screen has
  // changed to something explicit - or different.
  if (window_state == PlatformWindowState::kFullScreen &&
      target_display_id != display::kInvalidDisplayId) {
    return true;
  }

  // Otherwise do not allow the transition.
  return false;
}

void WaylandToplevelWindow::SendSetMinimized() {
  CHECK(xdg_toplevel_);
  xdg_toplevel_->SetMinimized();

  // Requests are processed in order, so the sync completes only after the
  // compositor has processed set_minimized.
  minimize_sync_callback_.reset(connection()->GetSyncCallback());
  static constexpr wl_callback_listener kMinimizeSyncListener = {
      .done = &OnMinimizeSyncDone,
  };
  wl_callback_add_listener(minimize_sync_callback_.get(),
                           &kMinimizeSyncListener, this);
  connection()->Flush();
}

// static
void WaylandToplevelWindow::OnMinimizeSyncDone(void* data,
                                               wl_callback* callback,
                                               uint32_t time) {
  auto* self = static_cast<WaylandToplevelWindow*>(data);
  CHECK_EQ(self->minimize_sync_callback_.get(), callback);
  self->minimize_sync_callback_.reset();
}

void WaylandToplevelWindow::SendSetFullscreen(int64_t fullscreen_display_id) {
  CHECK(xdg_toplevel_);
  fullscreen_state_ = FullscreenState::kClient;
  xdg_toplevel_->SetFullscreen(
      GetWaylandOutputForDisplayId(fullscreen_display_id));

  // Requests are processed in order, so the sync completes only after the
  // compositor has processed set_fullscreen.
  fullscreen_sync_callback_.reset(connection()->GetSyncCallback());
  static constexpr wl_callback_listener kFullscreenSyncListener = {
      .done = &OnFullscreenSyncDone,
  };
  wl_callback_add_listener(fullscreen_sync_callback_.get(),
                           &kFullscreenSyncListener, this);
  connection()->Flush();
}

void WaylandToplevelWindow::SendUnsetFullscreen() {
  CHECK(xdg_toplevel_);
  fullscreen_state_ = FullscreenState::kNone;
  fullscreen_sync_callback_.reset();
  xdg_toplevel_->UnSetFullscreen();
}

PlatformWindowState WaylandToplevelWindow::GetRawAppliedWindowState() const {
  const PlatformWindowState state = applied_state().window_state;
  return state != PlatformWindowState::kMinimized &&
                 fullscreen_state_ == FullscreenState::kCompositor
             ? PlatformWindowState::kFullScreen
             : state;
}

// static
void WaylandToplevelWindow::OnFullscreenSyncDone(void* data,
                                                 wl_callback* callback,
                                                 uint32_t time) {
  auto* self = static_cast<WaylandToplevelWindow*>(data);
  CHECK_EQ(self->fullscreen_sync_callback_.get(), callback);
  self->fullscreen_sync_callback_.reset();
}

void WaylandToplevelWindow::OnStateRequestApplied(
    const PlatformWindowDelegate::State& old_state,
    const PlatformWindowDelegate::State& new_state,
    int64_t viz_seq,
    bool from_client) {
  // A client-initiated resize proceeds as follows:
  // 1. SetBoundsInDIP() requests the new state.
  // 2. OnStateRequestApplied() (here) records the pre-resize geometries as
  //    stale and the viz sequence number of the resized frame. This normally
  //    runs synchronously from SetBoundsInDIP(), but may be deferred while
  //    too many earlier requests are in flight.
  // 3. OnSequencePoint() latches the state and sends the new window geometry,
  //    then the root surface commit for that frame calls OnFrameCommitted().
  // 4. OnFrameCommitted() calls SendBoundsSync() to send a wl_display.sync.
  // 5. OnBoundsSyncDone() runs once the compositor has processed the commit
  //    carrying the new geometry, and clears the stale geometries.
  // Until then, HandleToplevelConfigure() ignores configures that echo a
  // stale geometry.
  if (!from_client || !xdg_toplevel_) {
    return;
  }
  if (!ShouldSetBounds(new_state.window_state) ||
      new_state.tiled_edges != WindowTiledEdges()) {
    ClearPendingBoundsSync();
    return;
  }
  const gfx::Size& new_geometry = new_state.geometry_size_dip;
  std::erase(stale_geometry_sizes_dip_, new_geometry);
  auto add_stale_geometry = [&](const gfx::Size& size) {
    if (!size.IsEmpty() && size != new_geometry &&
        !std::ranges::contains(stale_geometry_sizes_dip_, size)) {
      stale_geometry_sizes_dip_.push_back(size);
    }
  };
  if (!last_sent_geometry_size_dip_.IsEmpty()) {
    add_stale_geometry(last_sent_geometry_size_dip_);
    if (ShouldSetBounds(old_state.window_state) &&
        old_state.tiled_edges == WindowTiledEdges()) {
      add_stale_geometry(old_state.geometry_size_dip);
    }
  }
  if (stale_geometry_sizes_dip_.empty()) {
    ClearPendingBoundsSync();
  } else {
    pending_bounds_viz_seq_ =
        std::max(pending_bounds_viz_seq_.value_or(-1), viz_seq);
    bounds_sync_callback_.reset();
  }
}

void WaylandToplevelWindow::SendBoundsSync() {
  if (!xdg_toplevel_) {
    ClearPendingBoundsSync();
    return;
  }
  // Requests are processed in order, so the sync completes only after the
  // compositor has processed the wl_surface.commit carrying the updated window
  // geometry.
  bounds_sync_callback_.reset(connection()->GetSyncCallback());
  static constexpr wl_callback_listener kBoundsSyncListener = {
      .done = &OnBoundsSyncDone,
  };
  wl_callback_add_listener(bounds_sync_callback_.get(), &kBoundsSyncListener,
                           this);
  connection()->Flush();
}

// static
void WaylandToplevelWindow::OnBoundsSyncDone(void* data,
                                             wl_callback* callback,
                                             uint32_t time) {
  auto* self = static_cast<WaylandToplevelWindow*>(data);
  CHECK_EQ(self->bounds_sync_callback_.get(), callback);
  self->bounds_sync_callback_.reset();
  if (!self->pending_bounds_viz_seq_.has_value()) {
    self->stale_geometry_sizes_dip_.clear();
  }
}

void WaylandToplevelWindow::ClearPendingBoundsSync() {
  stale_geometry_sizes_dip_.clear();
  pending_bounds_viz_seq_.reset();
  bounds_sync_callback_.reset();
}

void WaylandToplevelWindow::UpdatePreviouslyMaximized(
    PlatformWindowState previous_state,
    PlatformWindowState new_state) {
  if (new_state == PlatformWindowState::kNormal ||
      new_state == PlatformWindowState::kMaximized) {
    previously_maximized_ = false;
  } else if (previous_state == PlatformWindowState::kNormal ||
             previous_state == PlatformWindowState::kMaximized) {
    previously_maximized_ = previous_state == PlatformWindowState::kMaximized;
  }
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
  CHECK(delegate(), base::NotFatalUntil::M163);

  auto min_size_dip = delegate()->GetMinimumSizeForWindow();
  auto max_size_dip = delegate()->GetMaximumSizeForWindow();
  // Size constraints apply to the window geometry, so convert them like its
  // size.
  const PlatformWindowState window_state = applied_state().window_state;
  const float ui_scale = applied_state().ui_scale;

  if (min_size_dip.has_value()) {
    gfx::Size adjusted_min_size =
        BoundsSizeToGeometrySizeDIP(*min_size_dip, window_state, ui_scale);
    xdg_toplevel_->SetMinSize(adjusted_min_size.width(),
                              adjusted_min_size.height());
  }

  if (max_size_dip.has_value()) {
    gfx::Size adjusted_max_size =
        BoundsSizeToGeometrySizeDIP(*max_size_dip, window_state, ui_scale);
    // Zero means "no maximum" and should be preserved.
    if (max_size_dip->width() == 0) {
      adjusted_max_size.set_width(0);
    }
    if (max_size_dip->height() == 0) {
      adjusted_max_size.set_height(0);
    }
    xdg_toplevel_->SetMaxSize(adjusted_max_size.width(),
                              adjusted_max_size.height());
  }

  connection()->Flush();
}

void WaylandToplevelWindow::SetUpShellIntegration() {
  // This method should be called after the XDG surface is initialized.
  CHECK(xdg_toplevel_, base::NotFatalUntil::M163);
  TryAnnounceAppmenu();
}

void WaylandToplevelWindow::OnDecorationModeChanged() {
  CHECK(xdg_toplevel_, base::NotFatalUntil::M163);
  if (use_native_frame_) {
    // Set server-side decoration for windows using a native frame,
    // e.g. taskmanager
    xdg_toplevel_->SetDecoration(XdgToplevel::DecorationMode::kServerSide);
  } else {
    xdg_toplevel_->SetDecoration(XdgToplevel::DecorationMode::kClientSide);
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

void WaylandToplevelWindow::UpdateSessionStateIfNeeded() {
  CHECK(xdg_toplevel_);
  if (!session_) {
    return;
  }
  // If we're handling the first configure sequence and a `toplevel_session_`
  // was instantiated at window creation (see CreateXdgToplevel), it must be
  // removed now, so the requested `new_id` can be associated to this window.
  // Note that IsConfigured returns true only after the first ack_configure.
  if (!xdg_toplevel_->IsConfigured()) {
    const auto& session_data = session_data_.value();
    if (toplevel_session_) {
      CHECK(session_data.restore_id.has_value());
      toplevel_session_->Remove();
    }
    if (session_data.window_id) {
      toplevel_session_ = session_->TrackToplevel(this, session_data.window_id,
                                                  XdgSession::Action::kAdd);
    } else {
      // Window was just removed from `session_` and no new session window id
      // was provided. Notifying about it can result in `this` being destroyed
      // when the Wayland compositor supports only experimental version of the
      // session management protocol. See comments in XdgSession for details.
      // TODO(crbug.com/409099413): Remove when support for the experimental
      // session management protocol support gets dropped.
      auto alive = weak_ptr_factory_.GetWeakPtr();
      connection()->window_manager()->NotifyWindowRemovedFromSession(this);
      if (!alive) {
        return;
      }
    }
    connection()->Flush();
  }
}

void WaylandToplevelWindow::TryAnnounceAppmenu() {
  if (auto* appmenu_manager = connection()->org_kde_kwin_appmenu_manager()) {
    if (!appmenu_service_name_.empty() && !appmenu_object_path_.empty()) {
      appmenu_ = appmenu_manager->Create(root_surface()->surface());
      appmenu_->SetAddress(appmenu_service_name_, appmenu_object_path_);
    }
  }
}

}  // namespace ui
