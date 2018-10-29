// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_window.h"

#include <wayland-client.h>

#include "base/bind.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/hit_test.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_pointer.h"
#include "ui/ozone/platform/wayland/xdg_popup_wrapper_v5.h"
#include "ui/ozone/platform/wayland/xdg_popup_wrapper_v6.h"
#include "ui/ozone/platform/wayland/xdg_surface_wrapper_v5.h"
#include "ui/ozone/platform/wayland/xdg_surface_wrapper_v6.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

namespace {

// Factory, which decides which version type of xdg object to build.
class XDGShellObjectFactory {
 public:
  XDGShellObjectFactory() = default;
  ~XDGShellObjectFactory() = default;

  std::unique_ptr<XDGSurfaceWrapper> CreateXDGSurface(
      WaylandConnection* connection,
      WaylandWindow* wayland_window) {
    if (connection->shell_v6())
      return std::make_unique<XDGSurfaceWrapperV6>(wayland_window);

    DCHECK(connection->shell());
    return std::make_unique<XDGSurfaceWrapperV5>(wayland_window);
  }

  std::unique_ptr<XDGPopupWrapper> CreateXDGPopup(
      WaylandConnection* connection,
      WaylandWindow* wayland_window) {
    if (connection->shell_v6()) {
      std::unique_ptr<XDGSurfaceWrapper> surface =
          CreateXDGSurface(connection, wayland_window);
      surface->Initialize(connection, wayland_window->surface(), false);
      return std::make_unique<XDGPopupWrapperV6>(std::move(surface),
                                                 wayland_window);
    }
    DCHECK(connection->shell());
    return std::make_unique<XDGPopupWrapperV5>(wayland_window);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(XDGShellObjectFactory);
};

gfx::Rect TranslateBoundsToParentCoordinates(const gfx::Rect& child_bounds,
                                             const gfx::Rect& parent_bounds) {
  int x = child_bounds.x() - parent_bounds.x();
  int y = child_bounds.y() - parent_bounds.y();
  return gfx::Rect(gfx::Point(x, y), child_bounds.size());
}

}  // namespace

WaylandWindow::WaylandWindow(PlatformWindowDelegate* delegate,
                             WaylandConnection* connection)
    : delegate_(delegate),
      connection_(connection),
      xdg_shell_objects_factory_(new XDGShellObjectFactory()),
      state_(PlatformWindowState::PLATFORM_WINDOW_STATE_NORMAL),
      pending_state_(PlatformWindowState::PLATFORM_WINDOW_STATE_UNKNOWN) {
  // Set a class property key, which allows |this| to be used for interactive
  // events, e.g. move or resize.
  SetWmMoveResizeHandler(this, AsWmMoveResizeHandler());

  // Set a class property key, which allows |this| to be used for drag action.
  SetWmDragHandler(this, this);
}

WaylandWindow::~WaylandWindow() {
  if (drag_closed_callback_) {
    std::move(drag_closed_callback_)
        .Run(DragDropTypes::DragOperation::DRAG_NONE);
  }

  PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
  connection_->RemoveWindow(surface_.id());

  if (parent_window_)
    parent_window_->set_child_window(nullptr);

  if (has_pointer_focus_)
    connection_->pointer()->reset_window_with_pointer_focus();
}

// static
WaylandWindow* WaylandWindow::FromSurface(wl_surface* surface) {
  return static_cast<WaylandWindow*>(
      wl_proxy_get_user_data(reinterpret_cast<wl_proxy*>(surface)));
}

bool WaylandWindow::Initialize(PlatformWindowInitProperties properties) {
  DCHECK(xdg_shell_objects_factory_);

  bounds_ = properties.bounds;
  parent_window_ = GetParentWindow(properties.parent_widget);

  surface_.reset(wl_compositor_create_surface(connection_->compositor()));
  if (!surface_) {
    LOG(ERROR) << "Failed to create wl_surface";
    return false;
  }
  wl_surface_set_user_data(surface_.get(), this);

  ui::PlatformWindowType ui_window_type = properties.type;
  switch (ui_window_type) {
    case ui::PlatformWindowType::kMenu:
    case ui::PlatformWindowType::kPopup:
      // TODO(msisov, jkim): Handle notification windows, which are marked
      // as popup windows as well. Those are the windows that do not have
      // parents and pop up when the browser receives a notification.
      CreateXdgPopup();
      break;
    case ui::PlatformWindowType::kTooltip:
      // Tooltips subsurfaces are created on demand, upon ::Show calls.
      is_tooltip_ = true;
      break;
    case ui::PlatformWindowType::kWindow:
      CreateXdgSurface();
      break;
  }

  connection_->ScheduleFlush();

  connection_->AddWindow(surface_.id(), this);
  PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  delegate_->OnAcceleratedWidgetAvailable(surface_.id());

  return true;
}

void WaylandWindow::CreateXdgPopup() {
  if (bounds_.IsEmpty())
    return;

  DCHECK(parent_window_ && !xdg_popup_);

  gfx::Rect bounds =
      TranslateBoundsToParentCoordinates(bounds_, parent_window_->GetBounds());

  xdg_popup_ = xdg_shell_objects_factory_->CreateXDGPopup(connection_, this);
  if (!xdg_popup_ ||
      !xdg_popup_->Initialize(connection_, surface(), parent_window_, bounds)) {
    CHECK(false) << "Failed to create xdg_popup";
  }

  parent_window_->set_child_window(this);
}

void WaylandWindow::CreateXdgSurface() {
  xdg_surface_ =
      xdg_shell_objects_factory_->CreateXDGSurface(connection_, this);
  if (!xdg_surface_ || !xdg_surface_->Initialize(connection_, surface_.get())) {
    CHECK(false) << "Failed to create xdg_surface";
  }
}

void WaylandWindow::CreateTooltipSubSurface() {
  // Since Aura does not not provide a reference parent window, needed by
  // Wayland, we get the current focused window to place and show the tooltips.
  parent_window_ = connection_->GetCurrentFocusedWindow();

  // Tooltip creation is an async operation. By the time Aura actually creates
  // the tooltip, it is possible that the user has already moved the
  // mouse/pointer out of the window that triggered the tooptip. In this case,
  // parent_window_ is NULL.
  if (!parent_window_) {
    Hide();
    return;
  }

  wl_subcompositor* subcompositor = connection_->subcompositor();
  DCHECK(subcompositor);
  tooltip_subsurface_.reset(wl_subcompositor_get_subsurface(
      subcompositor, surface_.get(), parent_window_->surface()));

  wl_subsurface_set_position(tooltip_subsurface_.get(), bounds_.x(),
                             bounds_.y());
  wl_subsurface_set_desync(tooltip_subsurface_.get());
  wl_surface_commit(parent_window_->surface());
  connection_->ScheduleFlush();
}

void WaylandWindow::ApplyPendingBounds() {
  if (pending_bounds_.IsEmpty())
    return;

  SetBounds(pending_bounds_);
  DCHECK(xdg_surface_);
  xdg_surface_->SetWindowGeometry(bounds_);
  xdg_surface_->AckConfigure();
  pending_bounds_ = gfx::Rect();
  connection_->ScheduleFlush();
}

void WaylandWindow::DispatchHostWindowDragMovement(
    int hittest,
    const gfx::Point& pointer_location) {
  DCHECK(xdg_surface_);

  connection_->ResetPointerFlags();
  if (hittest == HTCAPTION)
    xdg_surface_->SurfaceMove(connection_);
  else
    xdg_surface_->SurfaceResize(connection_, hittest);

  connection_->ScheduleFlush();
}

void WaylandWindow::StartDrag(const ui::OSExchangeData& data,
                              int operation,
                              gfx::NativeCursor cursor,
                              base::OnceCallback<void(int)> callback) {
  DCHECK(!drag_closed_callback_);
  drag_closed_callback_ = std::move(callback);
  connection_->StartDrag(data, operation);
}

void WaylandWindow::Show() {
  if (!is_tooltip_)  // Tooltip windows should not get keyboard focus
    set_keyboard_focus(true);

  if (xdg_surface_)
    return;
  if (is_tooltip_) {
    if (!tooltip_subsurface_)
      CreateTooltipSubSurface();
    return;
  }
  if (!xdg_popup_) {
    CreateXdgPopup();
    connection_->ScheduleFlush();
  }
}

void WaylandWindow::Hide() {
  if (is_tooltip_) {
    parent_window_ = nullptr;
    wl_surface_attach(surface_.get(), NULL, 0, 0);
    wl_surface_commit(surface_.get());
    // Tooltip subsurface must be reset only after the buffer is detached.
    // Otherwise, gnome shell, for example, can end up with a broken event
    // pipe.
    tooltip_subsurface_.reset();
    return;
  }
  if (child_window_)
    child_window_->Hide();
  if (xdg_popup_) {
    parent_window_->set_child_window(nullptr);
    xdg_popup_.reset();
    // Detach buffer from surface in order to completely shutdown popups and
    // release resources.
    wl_surface_attach(surface_.get(), NULL, 0, 0);
    wl_surface_commit(surface_.get());
  }
}

void WaylandWindow::Close() {
  NOTIMPLEMENTED();
}

void WaylandWindow::PrepareForShutdown() {}

void WaylandWindow::SetBounds(const gfx::Rect& bounds) {
  if (bounds == bounds_)
    return;
  bounds_ = bounds;
  delegate_->OnBoundsChanged(bounds);
}

gfx::Rect WaylandWindow::GetBounds() {
  return bounds_;
}

void WaylandWindow::SetTitle(const base::string16& title) {
  DCHECK(xdg_surface_);
  xdg_surface_->SetTitle(title);
  connection_->ScheduleFlush();
}

void WaylandWindow::SetCapture() {
  // Wayland does implicit grabs, and doesn't allow for explicit grabs. The
  // exception to that are popups, but we explicitly send events to a
  // parent popup if such exists.
}

void WaylandWindow::ReleaseCapture() {
  // See comment in SetCapture() for details on wayland and grabs.
}

bool WaylandWindow::HasCapture() const {
  // If WaylandWindow is a popup window, assume it has the capture.
  return xdg_popup() ? true : has_implicit_grab_;
}

void WaylandWindow::ToggleFullscreen() {
  DCHECK(xdg_surface_);

  // There are some cases, when Chromium triggers a fullscreen state change
  // before the surface is activated. In such cases, Wayland may ignore state
  // changes and such flags as --kiosk or --start-fullscreen will be ignored.
  // To overcome this, set a pending state, and once the surface is activated,
  // trigger the change.
  if (!is_active_) {
    DCHECK(!IsFullscreen());
    pending_state_ = PlatformWindowState::PLATFORM_WINDOW_STATE_FULLSCREEN;
    return;
  }

  // TODO(msisov, tonikitoo): add multiscreen support. As the documentation says
  // if xdg_surface_set_fullscreen() is not provided with wl_output, it's up to
  // the compositor to choose which display will be used to map this surface.
  if (!IsFullscreen()) {
    // Fullscreen state changes have to be handled manually and then checked
    // against configuration events, which come from a compositor. The reason
    // of manually changing the |state_| is that the compositor answers about
    // state changes asynchronously, which leads to a wrong return value in
    // DesktopWindowTreeHostPlatform::IsFullscreen, for example, and media
    // files can never be set to fullscreen.
    state_ = PlatformWindowState::PLATFORM_WINDOW_STATE_FULLSCREEN;
    // Client might have requested a fullscreen state while the window was in
    // a maximized state. Thus, |restored_bounds_| can contain the bounds of a
    // "normal" state before the window was maximized. We don't override them
    // unless they are empty, because |bounds_| can contain bounds of a
    // maximized window instead.
    if (restored_bounds_.IsEmpty())
      SetRestoredBoundsInPixels(bounds_);

    xdg_surface_->SetFullscreen();
  } else {
    // Check the comment above. If it's not handled synchronously, media files
    // may not leave the fullscreen mode.
    state_ = PlatformWindowState::PLATFORM_WINDOW_STATE_UNKNOWN;
    xdg_surface_->UnSetFullscreen();
  }

  connection_->ScheduleFlush();
}

void WaylandWindow::Maximize() {
  DCHECK(xdg_surface_);

  if (IsFullscreen())
    ToggleFullscreen();

  // Keeps track of the previous bounds, which are used to restore a window
  // after unmaximize call. We don't override |restored_bounds_| if they have
  // already had value, which means the previous state has been a fullscreen
  // state. That is, the bounds can be stored during a change from a normal
  // state to a maximize state, and then preserved to be the same, when changing
  // from maximized to fullscreen and back to a maximized state.
  if (restored_bounds_.IsEmpty())
    SetRestoredBoundsInPixels(bounds_);

  xdg_surface_->SetMaximized();
  connection_->ScheduleFlush();
}

void WaylandWindow::Minimize() {
  DCHECK(xdg_surface_);
  DCHECK(!is_minimizing_);
  // Wayland doesn't explicitly say if a window is minimized. Instead, it
  // notifies that the window is not activated. But there are many cases, when
  // the window is not minimized and deactivated. In order to properly record
  // the minimized state, mark this window as being minimized. And as soon as a
  // configuration event comes, check if the window has been deactivated and has
  // |is_minimizing_| set.
  is_minimizing_ = true;
  xdg_surface_->SetMinimized();
  connection_->ScheduleFlush();
}

void WaylandWindow::Restore() {
  DCHECK(xdg_surface_);

  // Unfullscreen the window if it is fullscreen.
  if (IsFullscreen())
    ToggleFullscreen();

  xdg_surface_->UnSetMaximized();
  connection_->ScheduleFlush();
}

PlatformWindowState WaylandWindow::GetPlatformWindowState() const {
  return state_;
}

void WaylandWindow::SetCursor(PlatformCursor cursor) {
  scoped_refptr<BitmapCursorOzone> bitmap =
      BitmapCursorFactoryOzone::GetBitmapCursor(cursor);
  if (bitmap_ == bitmap)
    return;

  bitmap_ = bitmap;

  if (bitmap_) {
    connection_->SetCursorBitmap(bitmap_->bitmaps(), bitmap_->hotspot());
  } else {
    connection_->SetCursorBitmap(std::vector<SkBitmap>(), gfx::Point());
  }
}

void WaylandWindow::MoveCursorTo(const gfx::Point& location) {
  NOTIMPLEMENTED();
}

void WaylandWindow::ConfineCursorToBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

PlatformImeController* WaylandWindow::GetPlatformImeController() {
  NOTIMPLEMENTED();
  return nullptr;
}

void WaylandWindow::SetRestoredBoundsInPixels(const gfx::Rect& bounds) {
  restored_bounds_ = bounds;
}

gfx::Rect WaylandWindow::GetRestoredBoundsInPixels() const {
  return restored_bounds_;
}

bool WaylandWindow::CanDispatchEvent(const PlatformEvent& event) {
  // This window is a nested popup window, all the events must be forwarded
  // to the main popup window.
  if (child_window_ && child_window_->xdg_popup())
    return !!xdg_popup_.get();

  // If this is a nested menu window with a parent, it mustn't recieve any
  // events.
  if (parent_window_ && parent_window_->xdg_popup())
    return false;

  // If another window has capture, return early before checking focus.
  if (HasCapture())
    return true;

  if (event->IsMouseEvent())
    return has_pointer_focus_;
  if (event->IsKeyEvent())
    return has_keyboard_focus_;
  if (event->IsTouchEvent())
    return has_touch_focus_;
  return false;
}

uint32_t WaylandWindow::DispatchEvent(const PlatformEvent& native_event) {
  Event* event = static_cast<Event*>(native_event);
  // If the window does not have a pointer focus, but received this event, it
  // means the window is a popup window with a child popup window. In this case,
  // the location of the event must be converted from the nested popup to the
  // main popup, which the menu controller needs to properly handle events.
  if (event->IsLocatedEvent() && xdg_popup()) {
    // Parent window of the main menu window is not a popup, but rather an
    // xdg surface.
    DCHECK(!parent_window_->xdg_popup() && parent_window_->xdg_surface());
    WaylandWindow* window = connection_->GetCurrentFocusedWindow();
    if (window) {
      ConvertEventLocationToTargetWindowLocation(GetBounds().origin(),
                                                 window->GetBounds().origin(),
                                                 event->AsLocatedEvent());
    }
  }

  DispatchEventFromNativeUiEvent(
      native_event, base::BindOnce(&PlatformWindowDelegate::DispatchEvent,
                                   base::Unretained(delegate_)));
  return POST_DISPATCH_STOP_PROPAGATION;
}

void WaylandWindow::HandleSurfaceConfigure(int32_t width,
                                           int32_t height,
                                           bool is_maximized,
                                           bool is_fullscreen,
                                           bool is_activated) {
  // Propagate the window state information to the client.
  PlatformWindowState old_state = state_;

  // Ensure that manually handled state changes to fullscreen correspond to the
  // configuration events from a compositor.
  DCHECK_EQ(is_fullscreen, IsFullscreen());

  // There are two cases, which must be handled for the minimized state.
  // The first one is the case, when the surface goes into the minimized state
  // (check comment in WaylandWindow::Minimize), and the second case is when the
  // surface still has been minimized, but another cofiguration event with
  // !is_activated comes. For this, check if the WaylandWindow has been
  // minimized before and !is_activated is sent.
  if ((is_minimizing_ || IsMinimized()) && !is_activated) {
    is_minimizing_ = false;
    state_ = PlatformWindowState::PLATFORM_WINDOW_STATE_MINIMIZED;
  } else if (is_fullscreen) {
    // To ensure the |delegate_| is notified about state changes to fullscreen,
    // assume the old_state is UNKNOWN (check comment in ToggleFullscreen).
    old_state = PlatformWindowState::PLATFORM_WINDOW_STATE_UNKNOWN;
    DCHECK(state_ == PlatformWindowState::PLATFORM_WINDOW_STATE_FULLSCREEN);
  } else if (is_maximized) {
    state_ = PlatformWindowState::PLATFORM_WINDOW_STATE_MAXIMIZED;
  } else {
    state_ = PlatformWindowState::PLATFORM_WINDOW_STATE_NORMAL;
  }

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
  // bounds size or the current bounds.
  //
  // Note: if the browser was started with --start-fullscreen and a user exits
  // the fullscreen mode, wayland may set the width and height to be 1. Instead,
  // explicitly set the bounds to the current desired ones or the previous
  // bounds.
  if (width <= 1 || height <= 1) {
    pending_bounds_.set_size(restored_bounds_.IsEmpty()
                                 ? GetBounds().size()
                                 : restored_bounds_.size());
  } else {
    pending_bounds_ = gfx::Rect(0, 0, width, height);
  }

  const bool is_normal = !IsFullscreen() && !IsMaximized();
  const bool state_changed = old_state != state_;
  if (is_normal && state_changed)
    restored_bounds_ = gfx::Rect();

  if (state_changed)
    delegate_->OnWindowStateChanged(state_);

  if (did_active_change)
    delegate_->OnActivationChanged(is_active_);

  MaybeTriggerPendingStateChange();
}

void WaylandWindow::OnCloseRequest() {
  // Before calling OnCloseRequest, the |xdg_popup_| must become hidden and
  // only then call OnCloseRequest().
  DCHECK(!xdg_popup_);
  delegate_->OnCloseRequest();
}

void WaylandWindow::OnDragEnter(const gfx::PointF& point,
                                std::unique_ptr<OSExchangeData> data,
                                int operation) {
  NOTIMPLEMENTED_LOG_ONCE();
}

int WaylandWindow::OnDragMotion(const gfx::PointF& point,
                                uint32_t time,
                                int operation) {
  NOTIMPLEMENTED_LOG_ONCE();
  return 0;
}

void WaylandWindow::OnDragDrop(std::unique_ptr<OSExchangeData> data) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WaylandWindow::OnDragLeave() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WaylandWindow::OnDragSessionClose(uint32_t dnd_action) {
  std::move(drag_closed_callback_).Run(dnd_action);
}

bool WaylandWindow::IsMinimized() const {
  return state_ == PlatformWindowState::PLATFORM_WINDOW_STATE_MINIMIZED;
}

bool WaylandWindow::IsMaximized() const {
  return state_ == PlatformWindowState::PLATFORM_WINDOW_STATE_MAXIMIZED;
}

bool WaylandWindow::IsFullscreen() const {
  return state_ == PlatformWindowState::PLATFORM_WINDOW_STATE_FULLSCREEN;
}

void WaylandWindow::MaybeTriggerPendingStateChange() {
  if (pending_state_ == PlatformWindowState::PLATFORM_WINDOW_STATE_UNKNOWN ||
      !is_active_)
    return;
  DCHECK_EQ(pending_state_,
            PlatformWindowState::PLATFORM_WINDOW_STATE_FULLSCREEN);
  pending_state_ = PlatformWindowState::PLATFORM_WINDOW_STATE_UNKNOWN;
  ToggleFullscreen();
}

WaylandWindow* WaylandWindow::GetParentWindow(
    gfx::AcceleratedWidget parent_widget) {
  WaylandWindow* parent_window = connection_->GetWindow(parent_widget);

  // If propagated parent has already had a child, it means that |this| is a
  // submenu of a 3-dot menu. In aura, the parent of a 3-dot menu and its
  // submenu is the main native widget, which is the main window. In contrast,
  // Wayland requires a menu window to be a parent of a submenu window. Thus,
  // check if the suggested parent has a child. If yes, take its child as a
  // parent of |this|.
  // Another case is a notifcation window or a drop down window, which do not
  // have a parent in aura. In this case, take the current focused window as a
  // parent.
  if (parent_window && parent_window->child_window_)
    return parent_window->child_window_;
  if (!parent_window)
    return connection_->GetCurrentFocusedWindow();
  return parent_window;
}

WmMoveResizeHandler* WaylandWindow::AsWmMoveResizeHandler() {
  return static_cast<WmMoveResizeHandler*>(this);
}

}  // namespace ui
