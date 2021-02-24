// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"

#include <aura-shell-client-protocol.h>

#include "base/run_loop.h"
#include "base/unguessable_token.h"
#include "build/chromeos_buildflags.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/host/shell_object_factory.h"
#include "ui/ozone/platform/wayland/host/shell_toplevel_wrapper.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window_drag_controller.h"
#include "ui/ozone/platform/wayland/host/wayland_zaura_shell.h"
#include "ui/platform_window/extensions/wayland_extension.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// TODO(jamescook): The nogncheck is to work around false-positive failures on
// the code search bot. Remove after https://crrev.com/c/2432137 lands.
#include "chromeos/crosapi/cpp/crosapi_constants.h"  // nogncheck
#endif

namespace ui {

WaylandToplevelWindow::WaylandToplevelWindow(PlatformWindowDelegate* delegate,
                                             WaylandConnection* connection)
    : WaylandWindow(delegate, connection),
      state_(PlatformWindowState::kNormal) {
  // Set a class property key, which allows |this| to be used for interactive
  // events, e.g. move or resize.
  SetWmMoveResizeHandler(this, AsWmMoveResizeHandler());
}

WaylandToplevelWindow::~WaylandToplevelWindow() = default;

bool WaylandToplevelWindow::CreateShellToplevel() {
  ShellObjectFactory factory;
  shell_toplevel_ = factory.CreateShellToplevelWrapper(connection(), this);
  if (!shell_toplevel_) {
    LOG(ERROR) << "Failed to create a ShellToplevel.";
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  shell_toplevel_->SetAppId(window_unique_id_);
#else
  shell_toplevel_->SetAppId(wm_class_class_);
#endif
  shell_toplevel_->SetTitle(window_title_);
  SetSizeConstraints();
  TriggerStateChanges();
  InitializeAuraShellSurface();
  OnDecorationModeChanged();
  // This could be the proper time to update window mask using
  // NonClientView::GetWindowMask, since |non_client_view| is not created yet
  // during the call to WaylandWindow::Initialize().
  UpdateWindowMask();
  return true;
}

void WaylandToplevelWindow::ApplyPendingBounds() {
  if (pending_bounds_dip_.IsEmpty())
    return;
  DCHECK(shell_toplevel_);

  SetBoundsDip(pending_bounds_dip_);
  pending_bounds_dip_ = gfx::Rect();
  connection()->ScheduleFlush();
}

void WaylandToplevelWindow::DispatchHostWindowDragMovement(
    int hittest,
    const gfx::Point& pointer_location_in_px) {
  DCHECK(shell_toplevel_);

  connection()->event_source()->ResetPointerFlags();
  if (hittest == HTCAPTION)
    shell_toplevel_->SurfaceMove(connection());
  else
    shell_toplevel_->SurfaceResize(connection(), hittest);

  connection()->ScheduleFlush();
}

void WaylandToplevelWindow::Show(bool inactive) {
  if (shell_toplevel_)
    return;

  if (!CreateShellToplevel()) {
    Close();
    return;
  }

  UpdateBufferScale(false);

  if (auto* drag_controller = connection()->window_drag_controller())
    drag_controller->OnToplevelWindowCreated(this);

  WaylandWindow::Show(inactive);
}

void WaylandToplevelWindow::Hide() {
  if (!shell_toplevel_)
    return;

  if (child_window()) {
    child_window()->Hide();
    set_child_window(nullptr);
  }

  shell_toplevel_.reset();
  connection()->ScheduleFlush();

  // Detach buffer from surface in order to completely shutdown menus and
  // tooltips, and release resources.
  connection()->buffer_manager_host()->ResetSurfaceContents(root_surface());
}

bool WaylandToplevelWindow::IsVisible() const {
  // X and Windows return true if the window is minimized. For consistency, do
  // the same.
  return !!shell_toplevel_ || state_ == PlatformWindowState::kMinimized;
}

void WaylandToplevelWindow::SetTitle(const base::string16& title) {
  if (window_title_ == title)
    return;

  window_title_ = title;

  if (shell_toplevel_) {
    shell_toplevel_->SetTitle(title);
    connection()->ScheduleFlush();
  }
}

void WaylandToplevelWindow::ToggleFullscreen() {
  // TODO(msisov, tonikitoo): add multiscreen support. As the documentation says
  // if xdg_toplevel_set_fullscreen() is not provided with wl_output, it's up
  // to the compositor to choose which display will be used to map this surface.

  // We must track the previous state to correctly say our state as long as it
  // can be the maximized instead of normal one.
  PlatformWindowState new_state = PlatformWindowState::kUnknown;
  if (state_ == PlatformWindowState::kFullScreen) {
    if (previous_state_ == PlatformWindowState::kMaximized)
      new_state = previous_state_;
    else
      new_state = PlatformWindowState::kNormal;
  } else {
    new_state = PlatformWindowState::kFullScreen;
  }

  SetWindowState(new_state);
}

void WaylandToplevelWindow::Maximize() {
  SetWindowState(PlatformWindowState::kMaximized);
}

void WaylandToplevelWindow::Minimize() {
  SetWindowState(PlatformWindowState::kMinimized);
}

void WaylandToplevelWindow::Restore() {
  DCHECK(shell_toplevel_);

  // Differently from other platforms, under Wayland, unmaximizing the dragged
  // window before starting the drag loop is not needed as it is assumed to be
  // handled at compositor side, just like in xdg_toplevel_surface::move. So
  // skip it if there's a window drag session running.
  auto* drag_controller = connection()->window_drag_controller();
  if (drag_controller &&
      drag_controller->state() != WaylandWindowDragController::State::kIdle) {
    return;
  }

  SetWindowState(PlatformWindowState::kNormal);
}

PlatformWindowState WaylandToplevelWindow::GetPlatformWindowState() const {
  return state_;
}

void WaylandToplevelWindow::Activate() {
  // Only supported by compositors that support zaura_shell (e.g. exo).
  // TODO(https://crbug.com/1175327): Use standard Wayland extensions, such as
  // xdg-activation, when those are available.
  if (aura_surface_)
    zaura_surface_activate(aura_surface_.get());
}

void WaylandToplevelWindow::SizeConstraintsChanged() {
  // Size constraints only make sense for normal windows.
  if (!shell_toplevel_)
    return;

  SetSizeConstraints();
}

std::string WaylandToplevelWindow::GetWindowUniqueId() const {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return window_unique_id_;
#else
  return std::string();
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
  return use_native_frame_ && const_cast<WaylandToplevelWindow*>(this)
                                  ->connection()
                                  ->xdg_decoration_manager_v1();
}

base::Optional<std::vector<gfx::Rect>> WaylandToplevelWindow::GetWindowShape()
    const {
  return window_shape_in_dips_;
}

void WaylandToplevelWindow::HandleToplevelConfigure(int32_t width,
                                                    int32_t height,
                                                    bool is_maximized,
                                                    bool is_fullscreen,
                                                    bool is_activated) {
  // Store the old state to propagte state changes if Wayland decides to change
  // the state to something else.
  PlatformWindowState old_state = state_;
  if (state_ == PlatformWindowState::kMinimized && !is_activated) {
    state_ = PlatformWindowState::kMinimized;
  } else if (is_fullscreen) {
    state_ = PlatformWindowState::kFullScreen;
  } else if (is_maximized) {
    state_ = PlatformWindowState::kMaximized;
  } else {
    state_ = PlatformWindowState::kNormal;
  }

  const bool is_normal = state_ == PlatformWindowState::kNormal;

  bool did_send_delegate_notification = !!requested_window_show_state_count_;
  if (requested_window_show_state_count_)
    requested_window_show_state_count_--;

  const bool did_window_show_state_change = old_state != state_;

  // Update state before notifying delegate.
  const bool did_active_change = is_active_ != is_activated;
  is_active_ = is_activated;

  // Rather than call SetBounds here for every configure event, just save the
  // most recent bounds, and have WaylandConnection call ApplyPendingBounds
  // when it has finished processing events. We may get many configure events
  // in a row during an interactive resize, and only the last one matters.
  //
  // Width or height set to 0 means that we should decide on width and height by
  // ourselves, but we don't want to set them to anything else. Use restored
  // bounds size or the current bounds iff the current state is normal (neither
  // maximized nor fullscreen).
  //
  // Note: if the browser was started with --start-fullscreen and a user exits
  // the fullscreen mode, wayland may set the width and height to be 1. Instead,
  // explicitly set the bounds to the current desired ones or the previous
  // bounds.
  if (width > 1 && height > 1) {
    pending_bounds_dip_ = gfx::Rect(0, 0, width, height);
  } else if (is_normal) {
    pending_bounds_dip_.set_size(
        gfx::ScaleToRoundedSize(GetRestoredBoundsInPixels().IsEmpty()
                                    ? GetBounds().size()
                                    : GetRestoredBoundsInPixels().size(),
                                1.0 / buffer_scale()));
  }

  // Store the restored bounds of current state differs from the normal state.
  // It can be client or compositor side change from normal to something else.
  // Thus, we must store previous bounds to restore later.
  SetOrResetRestoredBounds();

  if (did_window_show_state_change && !did_send_delegate_notification) {
    previous_state_ = old_state;
    delegate()->OnWindowStateChanged(state_);
  }

  if (did_active_change)
    delegate()->OnActivationChanged(is_active_);
}

void WaylandToplevelWindow::HandleSurfaceConfigure(uint32_t serial) {
  if (pending_bounds_dip_.size() ==
      gfx::ScaleToRoundedSize(GetBounds().size(), 1.f / buffer_scale())) {
    // If |pending_bounds_dip_| matches GetBounds(), then a frame matching
    // |pending_bounds_dip_| may not arrive soon, despite the window delegate
    // receives the updated bounds. Without a new frame, UpdateVisualSize() is
    // not invoked, leaving this |configure| unacknowledged.
    //   E.g. With static window content, |configure| that does not
    //     change window size will not cause the window to redraw.
    // Hence, acknowledge this |configure| now to tell the Wayland compositor
    // that this window has been configured.
    shell_toplevel()->SetWindowGeometry(pending_bounds_dip_);
    shell_toplevel()->AckConfigure(serial);
  } else {
    // Otherwise, push the pending |configure| to |pending_configures_|, wait
    // for a frame update, which will invoke UpdateVisualSize().
    DCHECK_LT(pending_configures_.size(), 25u);
    pending_configures_.push_back({pending_bounds_dip_.size(), serial});
  }

  ApplyPendingBounds();
}

void WaylandToplevelWindow::UpdateVisualSize(const gfx::Size& size_px) {
  WaylandWindow::UpdateVisualSize(size_px);

  if (!shell_toplevel_)
    return;
  auto size_dip = gfx::ScaleToRoundedSize(size_px, 1.f / buffer_scale());
  auto result = std::find_if(
      pending_configures_.begin(), pending_configures_.end(),
      [&size_dip](auto& configure) { return size_dip == configure.size_dip; });

  if (result == pending_configures_.end())
    return;

  shell_toplevel()->SetWindowGeometry(gfx::Rect(size_dip));
  shell_toplevel()->AckConfigure(result->serial);
  pending_configures_.erase(pending_configures_.begin(), ++result);
}

bool WaylandToplevelWindow::OnInitialize(
    PlatformWindowInitProperties properties) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto token = base::UnguessableToken::Create();
  window_unique_id_ =
      std::string(crosapi::kLacrosAppIdPrefix) + token.ToString();
#else
  wm_class_class_ = properties.wm_class_class;
#endif
  SetWaylandExtension(this, static_cast<WaylandExtension*>(this));
  SetWmMoveLoopHandler(this, static_cast<WmMoveLoopHandler*>(this));
  return true;
}

bool WaylandToplevelWindow::IsActive() const {
  return is_active_;
}

bool WaylandToplevelWindow::RunMoveLoop(const gfx::Vector2d& drag_offset) {
  DCHECK(connection()->window_drag_controller());
  return connection()->window_drag_controller()->Drag(this, drag_offset);
}

void WaylandToplevelWindow::EndMoveLoop() {
  DCHECK(connection()->window_drag_controller());
  connection()->window_drag_controller()->StopDragging();
}

void WaylandToplevelWindow::StartWindowDraggingSessionIfNeeded() {
  DCHECK(connection()->window_drag_controller());
  connection()->window_drag_controller()->StartDragSession();
}

void WaylandToplevelWindow::SetImmersiveFullscreenStatus(bool status) {
  if (aura_surface_) {
    auto mode = status ? ZAURA_SURFACE_FULLSCREEN_MODE_IMMERSIVE
                       : ZAURA_SURFACE_FULLSCREEN_MODE_PLAIN;
    zaura_surface_set_fullscreen_mode(aura_surface_.get(), mode);
  } else {
    // TODO(https://crbug.com/1113900): Implement AuraShell support for
    // non-browser windows and replace this if-else clause by a DCHECK.
    NOTIMPLEMENTED_LOG_ONCE()
        << "Implement AuraShell support for non-browser windows.";
  }
}

void WaylandToplevelWindow::ShowSnapPreview(
    WaylandWindowSnapDirection snap_direction) {
  if (aura_surface_ && zaura_surface_get_version(aura_surface_.get()) >=
                           ZAURA_SURFACE_INTENT_TO_SNAP_SINCE_VERSION) {
    uint32_t zaura_shell_snap_direction = ZAURA_SURFACE_SNAP_DIRECTION_NONE;
    switch (snap_direction) {
      case WaylandWindowSnapDirection::kLeft:
        zaura_shell_snap_direction = ZAURA_SURFACE_SNAP_DIRECTION_LEFT;
        break;
      case WaylandWindowSnapDirection::kRight:
        zaura_shell_snap_direction = ZAURA_SURFACE_SNAP_DIRECTION_RIGHT;
        break;
      case WaylandWindowSnapDirection::kNone:
        break;
    }
    zaura_surface_intent_to_snap(aura_surface_.get(),
                                 zaura_shell_snap_direction);
    return;
  }

  NOTIMPLEMENTED_LOG_ONCE()
      << "Window snapping isn't available for non-lacros builds.";
}

void WaylandToplevelWindow::CommitSnap(
    WaylandWindowSnapDirection snap_direction) {
  if (aura_surface_ && zaura_surface_get_version(aura_surface_.get()) >=
                           ZAURA_SURFACE_UNSET_SNAP_SINCE_VERSION) {
    switch (snap_direction) {
      case WaylandWindowSnapDirection::kLeft:
        zaura_surface_set_snap_left(aura_surface_.get());
        return;
      case WaylandWindowSnapDirection::kRight:
        zaura_surface_set_snap_right(aura_surface_.get());
        return;
      case WaylandWindowSnapDirection::kNone:
        zaura_surface_unset_snap(aura_surface_.get());
        return;
    }
  }

  NOTIMPLEMENTED_LOG_ONCE()
      << "Window snapping isn't available for non-lacros builds.";
}

void WaylandToplevelWindow::TriggerStateChanges() {
  if (!shell_toplevel_)
    return;

  // Call UnSetMaximized only if current state is normal. Otherwise, if the
  // current state is fullscreen and the previous is maximized, calling
  // UnSetMaximized may result in wrong restored window position that clients
  // are not allowed to know about.
  if (state_ == PlatformWindowState::kMinimized) {
    shell_toplevel_->SetMinimized();
  } else if (state_ == PlatformWindowState::kFullScreen) {
    shell_toplevel_->SetFullscreen();
  } else if (previous_state_ == PlatformWindowState::kFullScreen) {
    shell_toplevel_->UnSetFullscreen();
  } else if (state_ == PlatformWindowState::kMaximized) {
    shell_toplevel_->SetMaximized();
  } else if (state_ == PlatformWindowState::kNormal) {
    shell_toplevel_->UnSetMaximized();
  }

  delegate()->OnWindowStateChanged(state_);

  connection()->ScheduleFlush();
}

void WaylandToplevelWindow::SetWindowState(PlatformWindowState state) {
  if (state_ != state) {
    previous_state_ = state_;
    state_ = state;

    // Tracks this window show state change request, coming from the Browser.
    requested_window_show_state_count_++;

    TriggerStateChanges();
  }
}

WmMoveResizeHandler* WaylandToplevelWindow::AsWmMoveResizeHandler() {
  return static_cast<WmMoveResizeHandler*>(this);
}

void WaylandToplevelWindow::SetSizeConstraints() {
  DCHECK(delegate());

  min_size_ = delegate()->GetMinimumSizeForWindow();
  max_size_ = delegate()->GetMaximumSizeForWindow();

  if (min_size_.has_value()) {
    auto min_size_dip =
        gfx::ScaleToRoundedSize(min_size_.value(), 1.0f / buffer_scale());
    shell_toplevel_->SetMinSize(min_size_dip.width(), min_size_dip.height());
  }

  if (max_size_.has_value()) {
    auto max_size_dip =
        gfx::ScaleToRoundedSize(max_size_.value(), 1.0f / buffer_scale());
    shell_toplevel_->SetMaxSize(max_size_dip.width(), max_size_dip.height());
  }

  connection()->ScheduleFlush();
}

void WaylandToplevelWindow::SetOrResetRestoredBounds() {
  // The |restored_bounds_| are used when the window gets back to normal
  // state after it went maximized or fullscreen.  So we reset these if the
  // window has just become normal and store the current bounds if it is
  // either going out of normal state or simply changes the state and we don't
  // have any meaningful value stored.
  if (GetPlatformWindowState() == PlatformWindowState::kNormal) {
    SetRestoredBoundsInPixels({});
  } else if (GetRestoredBoundsInPixels().IsEmpty()) {
    SetRestoredBoundsInPixels(GetBounds());
  }
}

void WaylandToplevelWindow::InitializeAuraShellSurface() {
  // InitializeAuraShellSurface() should be called after the XDG surface is
  // initialized.
  DCHECK(shell_toplevel_);

  if (connection()->zaura_shell() && !aura_surface_) {
    aura_surface_.reset(zaura_shell_get_aura_surface(
        connection()->zaura_shell()->wl_object(), root_surface()->surface()));
    SetImmersiveFullscreenStatus(false);
  }
}

void WaylandToplevelWindow::OnDecorationModeChanged() {
  DCHECK(shell_toplevel_);
  if (use_native_frame_) {
    // Set server-side decoration for windows using a native frame,
    // e.g. taskmanager
    shell_toplevel_->SetDecoration(
        ShellToplevelWrapper::DecorationMode::kServerSide);
  } else if (aura_surface_ &&
             zaura_surface_get_version(aura_surface_.get()) >=
                 ZAURA_SURFACE_SET_SERVER_START_RESIZE_SINCE_VERSION) {
    // Sets custom-decoration mode for window that supports aura_shell.
    // e.g. lacros-browser.
    zaura_surface_set_server_start_resize(aura_surface_.get());
  } else {
    shell_toplevel_->SetDecoration(
        ShellToplevelWrapper::DecorationMode::kClientSide);
  }
}

void WaylandToplevelWindow::UpdateWindowMask() {
  // TODO(http://crbug.com/1158733): When supporting PlatformWindow::SetShape,
  // update window region with the given |shape|.
  WaylandWindow::UpdateWindowMask();
  root_surface()->SetInputRegion(gfx::Rect(visual_size_px()));
}

void WaylandToplevelWindow::UpdateWindowShape() {
  // Create |window_shape_in_dips_| using the window mask of
  // PlatformWindowDelegate otherwise resets it.
  SkPath window_mask_in_pixels =
      delegate()->GetWindowMaskForWindowShapeInPixels();
  if (window_mask_in_pixels.isEmpty()) {
    window_shape_in_dips_.reset();
    return;
  }
  SkPath window_mask_in_dips =
      wl::ConvertPathToDIP(window_mask_in_pixels, buffer_scale());
  window_shape_in_dips_ = wl::CreateRectsFromSkPath(window_mask_in_dips);
}

}  // namespace ui
