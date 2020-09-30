// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"

#include <aura-shell-client-protocol.h>

#include "base/run_loop.h"
#include "base/unguessable_token.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/host/shell_object_factory.h"
#include "ui/ozone/platform/wayland/host/shell_surface_wrapper.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_data_drag_controller.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window_drag_controller.h"
#include "ui/platform_window/extensions/wayland_extension.h"
#include "ui/platform_window/wm/wm_drop_handler.h"

#if BUILDFLAG(IS_LACROS)
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

  // Set a class property key, which allows |this| to be used for drag action.
  SetWmDragHandler(this, this);
}

WaylandToplevelWindow::~WaylandToplevelWindow() {
  if (drag_handler_delegate_) {
    drag_handler_delegate_->OnDragFinished(
        DragDropTypes::DragOperation::DRAG_NONE);
  }
  CancelDrag();
}

bool WaylandToplevelWindow::CreateShellSurface() {
  ShellObjectFactory factory;
  shell_surface_ = factory.CreateShellSurfaceWrapper(connection(), this);
  if (!shell_surface_) {
    LOG(ERROR) << "Failed to create a ShellSurface.";
    return false;
  }

#if BUILDFLAG(IS_LACROS)
  shell_surface_->SetAppId(window_unique_id_);
#else
  shell_surface_->SetAppId(wm_class_class_);
#endif
  shell_surface_->SetTitle(window_title_);
  SetSizeConstraints();
  TriggerStateChanges();
  return true;
}

void WaylandToplevelWindow::ApplyPendingBounds() {
  if (pending_bounds_dip_.IsEmpty())
    return;
  DCHECK(shell_surface_);

  SetBoundsDip(pending_bounds_dip_);
  shell_surface_->SetWindowGeometry(pending_bounds_dip_);
  pending_bounds_dip_ = gfx::Rect();
  connection()->ScheduleFlush();
}

void WaylandToplevelWindow::DispatchHostWindowDragMovement(
    int hittest,
    const gfx::Point& pointer_location_in_px) {
  DCHECK(shell_surface_);

  connection()->event_source()->ResetPointerFlags();
  if (hittest == HTCAPTION)
    shell_surface_->SurfaceMove(connection());
  else
    shell_surface_->SurfaceResize(connection(), hittest);

  connection()->ScheduleFlush();
}

bool WaylandToplevelWindow::StartDrag(const ui::OSExchangeData& data,
                                      int operation,
                                      gfx::NativeCursor cursor,
                                      bool can_grab_pointer,
                                      WmDragHandler::Delegate* delegate) {
  DCHECK(!drag_handler_delegate_);
  drag_handler_delegate_ = delegate;
  connection()->data_drag_controller()->StartSession(data, operation);

  base::RunLoop drag_loop(base::RunLoop::Type::kNestableTasksAllowed);
  drag_loop_quit_closure_ = drag_loop.QuitClosure();

  auto alive = weak_ptr_factory_.GetWeakPtr();
  drag_loop.Run();
  if (!alive)
    return false;
  return true;
}

void WaylandToplevelWindow::CancelDrag() {
  if (drag_loop_quit_closure_.is_null())
    return;
  std::move(drag_loop_quit_closure_).Run();
}

void WaylandToplevelWindow::Show(bool inactive) {
  if (shell_surface_)
    return;

  if (!CreateShellSurface()) {
    Close();
    return;
  }

  UpdateBufferScale(false);
}

void WaylandToplevelWindow::Hide() {
  if (!shell_surface_)
    return;

  if (child_window()) {
    child_window()->Hide();
    set_child_window(nullptr);
  }

  shell_surface_.reset();
  connection()->ScheduleFlush();

  // Detach buffer from surface in order to completely shutdown menus and
  // tooltips, and release resources.
  connection()->buffer_manager_host()->ResetSurfaceContents(root_surface());
}

bool WaylandToplevelWindow::IsVisible() const {
  // X and Windows return true if the window is minimized. For consistency, do
  // the same.
  return !!shell_surface_ || state_ == PlatformWindowState::kMinimized;
}

void WaylandToplevelWindow::SetTitle(const base::string16& title) {
  if (window_title_ == title)
    return;

  window_title_ = title;

  if (shell_surface_) {
    shell_surface_->SetTitle(title);
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
  DCHECK(shell_surface_);
  SetWindowState(PlatformWindowState::kNormal);
}

PlatformWindowState WaylandToplevelWindow::GetPlatformWindowState() const {
  return state_;
}

void WaylandToplevelWindow::SizeConstraintsChanged() {
  // Size constraints only make sense for normal windows.
  if (!shell_surface_)
    return;

  DCHECK(delegate());
  min_size_ = delegate()->GetMinimumSizeForWindow();
  max_size_ = delegate()->GetMaximumSizeForWindow();
  SetSizeConstraints();
}

std::string WaylandToplevelWindow::GetWindowUniqueId() const {
#if BUILDFLAG(IS_LACROS)
  return window_unique_id_;
#else
  return std::string();
#endif
}

void WaylandToplevelWindow::HandleSurfaceConfigure(int32_t width,
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

  const bool state_changed = old_state != state_;
  const bool is_normal = state_ == PlatformWindowState::kNormal;

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
  ApplyPendingBounds();

  if (state_changed)
    delegate()->OnWindowStateChanged(state_);

  if (did_active_change)
    delegate()->OnActivationChanged(is_active_);
}

void WaylandToplevelWindow::OnDragEnter(const gfx::PointF& point,
                                        std::unique_ptr<OSExchangeData> data,
                                        int operation) {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler)
    return;

  // Wayland sends locations in DIP so they need to be translated to
  // physical pixels.
  // TODO(crbug.com/1102857): get the real event modifier here.
  drop_handler->OnDragEnter(
      gfx::ScalePoint(point, buffer_scale(), buffer_scale()), std::move(data),
      operation,
      /*modifiers=*/0);
}

int WaylandToplevelWindow::OnDragMotion(const gfx::PointF& point,
                                        int operation) {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler)
    return 0;

  // Wayland sends locations in DIP so they need to be translated to
  // physical pixels.
  // TODO(crbug.com/1102857): get the real event modifier here.
  return drop_handler->OnDragMotion(
      gfx::ScalePoint(point, buffer_scale(), buffer_scale()), operation,
      /*modifiers=*/0);
}

void WaylandToplevelWindow::OnDragDrop(std::unique_ptr<OSExchangeData> data) {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler)
    return;
  // TODO(crbug.com/1102857): get the real event modifier here.
  drop_handler->OnDragDrop(std::move(data), /*modifiers=*/0);
}

void WaylandToplevelWindow::OnDragLeave() {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler)
    return;
  drop_handler->OnDragLeave();
}

void WaylandToplevelWindow::OnDragSessionClose(uint32_t dnd_action) {
  DCHECK(drag_handler_delegate_);
  drag_handler_delegate_->OnDragFinished(dnd_action);
  drag_handler_delegate_ = nullptr;
  connection()->event_source()->ResetPointerFlags();
  std::move(drag_loop_quit_closure_).Run();
}

bool WaylandToplevelWindow::OnInitialize(
    PlatformWindowInitProperties properties) {
#if BUILDFLAG(IS_LACROS)
  auto token = base::UnguessableToken::Create();
  window_unique_id_ =
      std::string(crosapi::kLacrosAppIdPrefix) + token.ToString();
#else
  wm_class_class_ = properties.wm_class_class;
#endif
  SetWaylandExtension(this, static_cast<WaylandExtension*>(this));
  SetWmMoveLoopHandler(this, static_cast<WmMoveLoopHandler*>(this));
  InitializeAuraShell();
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

void WaylandToplevelWindow::TriggerStateChanges() {
  if (!shell_surface_)
    return;

  if (state_ == PlatformWindowState::kFullScreen)
    shell_surface_->SetFullscreen();
  else
    shell_surface_->UnSetFullscreen();

  // Call UnSetMaximized only if current state is normal. Otherwise, if the
  // current state is fullscreen and the previous is maximized, calling
  // UnSetMaximized may result in wrong restored window position that clients
  // are not allowed to know about.
  if (state_ == PlatformWindowState::kMaximized)
    shell_surface_->SetMaximized();
  else if (state_ == PlatformWindowState::kNormal)
    shell_surface_->UnSetMaximized();

  if (state_ == PlatformWindowState::kMinimized)
    shell_surface_->SetMinimized();

  connection()->ScheduleFlush();
}

void WaylandToplevelWindow::SetWindowState(PlatformWindowState state) {
  previous_state_ = state_;
  state_ = state;
  TriggerStateChanges();
}

WmMoveResizeHandler* WaylandToplevelWindow::AsWmMoveResizeHandler() {
  return static_cast<WmMoveResizeHandler*>(this);
}

void WaylandToplevelWindow::SetSizeConstraints() {
  if (min_size_.has_value())
    shell_surface_->SetMinSize(min_size_->width(), min_size_->height());
  if (max_size_.has_value())
    shell_surface_->SetMaxSize(max_size_->width(), max_size_->height());

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

void WaylandToplevelWindow::InitializeAuraShell() {
  if (connection()->aura_shell()) {
    DCHECK(!aura_surface_);
    aura_surface_.reset(zaura_shell_get_aura_surface(
        connection()->aura_shell(), root_surface()->surface()));
    zaura_surface_set_fullscreen_mode(aura_surface_.get(),
                                      ZAURA_SURFACE_FULLSCREEN_MODE_IMMERSIVE);
  }
}

}  // namespace ui
