// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_window.h"

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor_position.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"
#include "ui/ozone/platform/wayland/host/wayland_subsurface.h"
#include "ui/ozone/public/mojom/wayland/wayland_overlay_config.mojom.h"

namespace {

bool OverlayStackOrderCompare(
    const ui::ozone::mojom::WaylandOverlayConfigPtr& i,
    const ui::ozone::mojom::WaylandOverlayConfigPtr& j) {
  return i->z_order < j->z_order;
}

}  // namespace

namespace ui {

WaylandWindow::WaylandWindow(PlatformWindowDelegate* delegate,
                             WaylandConnection* connection)
    : delegate_(delegate),
      connection_(connection),
      accelerated_widget_(
          connection->wayland_window_manager()->AllocateAcceleratedWidget()) {}

WaylandWindow::~WaylandWindow() {
  shutting_down_ = true;

  PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);

  for (const auto& widget_subsurface : wayland_subsurfaces()) {
    connection_->wayland_window_manager()->RemoveSubsurface(
        GetWidget(), widget_subsurface.get());
  }
  if (root_surface_)
    connection_->wayland_window_manager()->RemoveWindow(GetWidget());

  if (parent_window_)
    parent_window_->set_child_window(nullptr);
}

void WaylandWindow::OnWindowLostCapture() {
  delegate_->OnLostCapture();
}

void WaylandWindow::UpdateBufferScale(bool update_bounds) {
  DCHECK(connection_->wayland_output_manager());
  const auto* screen = connection_->wayland_output_manager()->wayland_screen();

  // The client might not create screen at all.
  if (!screen)
    return;

  const auto widget = GetWidget();

  int32_t new_scale = 0;
  if (parent_window_) {
    new_scale = parent_window_->buffer_scale();
    ui_scale_ = parent_window_->ui_scale_;
  } else {
    const auto display = (widget == gfx::kNullAcceleratedWidget)
                             ? screen->GetPrimaryDisplay()
                             : screen->GetDisplayForAcceleratedWidget(widget);
    new_scale = connection_->wayland_output_manager()
                    ->GetOutput(display.id())
                    ->scale_factor();

    if (display::Display::HasForceDeviceScaleFactor())
      ui_scale_ = display::Display::GetForcedDeviceScaleFactor();
    else
      ui_scale_ = display.device_scale_factor();
  }
  // At this point, buffer_scale() still returns the old scale.
  if (update_bounds)
    SetBoundsDip(gfx::ScaleToRoundedRect(bounds_px_, 1.0 / buffer_scale()));

  root_surface_->SetBufferScale(new_scale, update_bounds);
}

gfx::AcceleratedWidget WaylandWindow::GetWidget() const {
  return accelerated_widget_;
}

void WaylandWindow::SetPointerFocus(bool focus) {
  has_pointer_focus_ = focus;

  // Whenever the window gets the pointer focus back, we must reinitialize the
  // cursor. Otherwise, it is invalidated whenever the pointer leaves the
  // surface and is not restored by the Wayland compositor.
  if (has_pointer_focus_ && bitmap_)
    connection_->SetCursorBitmap(bitmap_->bitmaps(), bitmap_->hotspot());
}

void WaylandWindow::Show(bool inactive) {
  NOTREACHED();
}

void WaylandWindow::Hide() {
  NOTREACHED();
}

void WaylandWindow::Close() {
  delegate_->OnClosed();
}

bool WaylandWindow::IsVisible() const {
  NOTREACHED();
  return false;
}

void WaylandWindow::PrepareForShutdown() {}

void WaylandWindow::SetBounds(const gfx::Rect& bounds_px) {
  if (bounds_px_ == bounds_px)
    return;
  bounds_px_ = bounds_px;

  root_surface_->SetOpaqueRegion(bounds_px);
  delegate_->OnBoundsChanged(bounds_px_);
}

gfx::Rect WaylandWindow::GetBounds() {
  return bounds_px_;
}

void WaylandWindow::SetTitle(const base::string16& title) {}

void WaylandWindow::SetCapture() {
  // Wayland doesn't allow explicit grabs. Instead, it sends events to "entered"
  // windows. That is, if user enters their mouse pointer to a window, that
  // window starts to receive events. However, Chromium may want to reroute
  // these events to another window. In this case, tell the window manager that
  // this specific window has grabbed the events, and they will be rerouted in
  // WaylandWindow::DispatchEvent method.
  if (!HasCapture())
    connection_->wayland_window_manager()->GrabLocatedEvents(this);
}

void WaylandWindow::ReleaseCapture() {
  if (HasCapture())
    connection_->wayland_window_manager()->UngrabLocatedEvents(this);
  // See comment in SetCapture() for details on wayland and grabs.
}

bool WaylandWindow::HasCapture() const {
  return connection_->wayland_window_manager()->located_events_grabber() ==
         this;
}

void WaylandWindow::ToggleFullscreen() {}

void WaylandWindow::Maximize() {}

void WaylandWindow::Minimize() {}

void WaylandWindow::Restore() {}

PlatformWindowState WaylandWindow::GetPlatformWindowState() const {
  // Remove normal state for all the other types of windows as it's only the
  // WaylandToplevelWindow that supports state changes.
  return PlatformWindowState::kNormal;
}

void WaylandWindow::Activate() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WaylandWindow::Deactivate() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WaylandWindow::SetUseNativeFrame(bool use_native_frame) {
  // See comment below in ShouldUseNativeFrame.
  NOTIMPLEMENTED_LOG_ONCE();
}

bool WaylandWindow::ShouldUseNativeFrame() const {
  // This depends on availability of XDG-Decoration protocol extension.
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
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

void WaylandWindow::SetRestoredBoundsInPixels(const gfx::Rect& bounds_px) {
  restored_bounds_px_ = bounds_px;
}

gfx::Rect WaylandWindow::GetRestoredBoundsInPixels() const {
  return restored_bounds_px_;
}

bool WaylandWindow::ShouldWindowContentsBeTransparent() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void WaylandWindow::SetAspectRatio(const gfx::SizeF& aspect_ratio) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WaylandWindow::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                   const gfx::ImageSkia& app_icon) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WaylandWindow::SizeConstraintsChanged() {}

bool WaylandWindow::CanDispatchEvent(const PlatformEvent& event) {
  if (event->IsMouseEvent())
    return has_pointer_focus_;
  if (event->IsKeyEvent())
    return has_keyboard_focus_;
  if (event->IsTouchEvent())
    return has_touch_focus_;
  if (event->IsScrollEvent())
    return has_pointer_focus_;
  return false;
}

uint32_t WaylandWindow::DispatchEvent(const PlatformEvent& native_event) {
  Event* event = static_cast<Event*>(native_event);

  if (event->IsLocatedEvent()) {
    auto* event_grabber =
        connection_->wayland_window_manager()->located_events_grabber();
    auto* root_parent_window = GetRootParentWindow();

    // Wayland sends locations in DIP so they need to be translated to
    // physical pixels.
    event->AsLocatedEvent()->set_location_f(gfx::ScalePoint(
        event->AsLocatedEvent()->location_f(), buffer_scale(), buffer_scale()));

    // We must reroute the events to the event grabber iff these windows belong
    // to the same root parent window. For example, there are 2 top level
    // Wayland windows. One of them (window_1) has a child menu window that is
    // the event grabber. If the mouse is moved over the window_1, it must
    // reroute the events to the event grabber. If the mouse is moved over the
    // window_2, the events mustn't be rerouted, because that belongs to another
    // stack of windows. Remember that Wayland sends local surface coordinates,
    // and continuing rerouting all the events may result in events sent to the
    // grabber even though the mouse is over another root window.
    //
    if (event_grabber &&
        root_parent_window == event_grabber->GetRootParentWindow()) {
      ConvertEventLocationToTargetWindowLocation(
          event_grabber->GetBounds().origin(), GetBounds().origin(),
          event->AsLocatedEvent());
      return event_grabber->DispatchEventToDelegate(native_event);
    }
  }

  // Dispatch all keyboard events to the root window.
  if (event->IsKeyEvent())
    return GetRootParentWindow()->DispatchEventToDelegate(event);

  return DispatchEventToDelegate(native_event);
}

void WaylandWindow::HandleSurfaceConfigure(int32_t widht,
                                           int32_t height,
                                           bool is_maximized,
                                           bool is_fullscreen,
                                           bool is_activated) {
  NOTREACHED()
      << "Only shell surfaces must receive HandleSurfaceConfigure calls.";
}

void WaylandWindow::HandlePopupConfigure(const gfx::Rect& bounds_dip) {
  NOTREACHED() << "Only shell popups must receive HandlePopupConfigure calls.";
}

void WaylandWindow::OnCloseRequest() {
  delegate_->OnCloseRequest();
}

void WaylandWindow::OnDragEnter(const gfx::PointF& point,
                                std::unique_ptr<OSExchangeData> data,
                                int operation) {}

int WaylandWindow::OnDragMotion(const gfx::PointF& point, int operation) {
  return -1;
}

void WaylandWindow::OnDragDrop(std::unique_ptr<OSExchangeData> data) {}

void WaylandWindow::OnDragLeave() {}

void WaylandWindow::OnDragSessionClose(uint32_t dnd_action) {}

void WaylandWindow::SetBoundsDip(const gfx::Rect& bounds_dip) {
  SetBounds(gfx::ScaleToRoundedRect(bounds_dip, buffer_scale()));
}

bool WaylandWindow::Initialize(PlatformWindowInitProperties properties) {
  root_surface_ = std::make_unique<WaylandSurface>(connection_, this);
  if (!root_surface_->Initialize()) {
    LOG(ERROR) << "Failed to create wl_surface";
    return false;
  }

  // Properties contain DIP bounds but the buffer scale is initially 1 so it's
  // OK to assign.  The bounds will be recalculated when the buffer scale
  // changes.
  bounds_px_ = properties.bounds;
  opacity_ = properties.opacity;
  type_ = properties.type;

  connection_->wayland_window_manager()->AddWindow(GetWidget(), this);

  if (!OnInitialize(std::move(properties)))
    return false;

  connection_->ScheduleFlush();

  PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  delegate_->OnAcceleratedWidgetAvailable(GetWidget());

  // Will do nothing for menus because they have got their scale above.
  UpdateBufferScale(false);
  root_surface_->SetOpaqueRegion(bounds_px_);

  return true;
}

WaylandWindow* WaylandWindow::GetRootParentWindow() {
  return parent_window_ ? parent_window_->GetRootParentWindow() : this;
}

void WaylandWindow::AddEnteredOutputId(struct wl_output* output) {
  // Wayland does weird things for menus so instead of tracking outputs that
  // we entered or left, we take that from the parent window and ignore this
  // event.
  if (wl::IsMenuType(type()))
    return;

  const uint32_t entered_output_id =
      connection_->wayland_output_manager()->GetIdForOutput(output);
  DCHECK_NE(entered_output_id, 0u);
  auto result = entered_outputs_ids_.insert(entered_output_id);
  DCHECK(result.first != entered_outputs_ids_.end());

  UpdateBufferScale(true);
}

void WaylandWindow::RemoveEnteredOutputId(struct wl_output* output) {
  // Wayland does weird things for menus so instead of tracking outputs that
  // we entered or left, we take that from the parent window and ignore this
  // event.
  if (wl::IsMenuType(type()))
    return;

  const uint32_t left_output_id =
      connection_->wayland_output_manager()->GetIdForOutput(output);
  auto entered_output_id_it = entered_outputs_ids_.find(left_output_id);
  // Workaround: when a user switches physical output between two displays,
  // a window does not necessarily receive enter events immediately or until
  // a user resizes/moves the window. It means that switching output between
  // displays in a single output mode results in leave events, but the surface
  // might not have received enter event before. Thus, remove the id of left
  // output only if it was stored before.
  if (entered_output_id_it != entered_outputs_ids_.end())
    entered_outputs_ids_.erase(entered_output_id_it);

  UpdateBufferScale(true);
}

void WaylandWindow::UpdateCursorPositionFromEvent(
    std::unique_ptr<Event> event) {
  DCHECK(event->IsLocatedEvent());

  // This is a tricky part. Initially, Wayland sends events to surfaces the
  // events are targeted for. But, in order to fulfill Chromium's assumptions
  // about event targets, some of the events are rerouted and their locations
  // are converted. The event we got here is rerouted and it has had its
  // location fixed.
  //
  // Basically, this method must translate coordinates of all events
  // in regards to top-level windows' coordinates as it's always located at
  // origin (0,0) from Chromium point of view (remember that Wayland doesn't
  // provide global coordinates to its clients). And it's totally fine to use it
  // as the target. Thus, the location of the |event| is always converted using
  // the top-level window's bounds as the target excluding cases, when the
  // mouse/touch is over a top-level window.
  auto* toplevel_window = GetRootParentWindow();
  if (toplevel_window != this) {
    ConvertEventLocationToTargetWindowLocation(
        toplevel_window->GetBounds().origin(), GetBounds().origin(),
        event->AsLocatedEvent());
  }
  auto* cursor_position = connection_->wayland_cursor_position();
  if (cursor_position) {
    cursor_position->OnCursorPositionChanged(
        event->AsLocatedEvent()->location());
  }
}

WaylandWindow* WaylandWindow::GetTopLevelWindow() {
  return parent_window_ ? parent_window_->GetTopLevelWindow() : this;
}

WaylandWindow* WaylandWindow::GetTopMostChildWindow() {
  return child_window_ ? child_window_->GetTopMostChildWindow() : this;
}

bool WaylandWindow::IsOpaqueWindow() const {
  return opacity_ == ui::PlatformWindowOpacity::kOpaqueWindow;
}

bool WaylandWindow::IsActive() const {
  // Please read the comment where the IsActive method is declared.
  return false;
}

uint32_t WaylandWindow::DispatchEventToDelegate(
    const PlatformEvent& native_event) {
  auto* event = static_cast<Event*>(native_event);
  if (event->IsLocatedEvent())
    UpdateCursorPositionFromEvent(Event::Clone(*event));

  bool handled = DispatchEventFromNativeUiEvent(
      native_event, base::BindOnce(&PlatformWindowDelegate::DispatchEvent,
                                   base::Unretained(delegate_)));
  return handled ? POST_DISPATCH_STOP_PROPAGATION : POST_DISPATCH_NONE;
}

std::unique_ptr<WaylandSurface> WaylandWindow::TakeWaylandSurface() {
  DCHECK(shutting_down_);
  DCHECK(root_surface_);
  root_surface_->UnsetRootWindow();
  return std::move(root_surface_);
}

bool WaylandWindow::RequestSubsurface() {
  auto subsurface = std::make_unique<WaylandSubsurface>(connection_, this);
  if (!subsurface->surface())
    return false;
  connection_->wayland_window_manager()->AddSubsurface(GetWidget(),
                                                       subsurface.get());
  subsurface_stack_above_.push_back(subsurface.get());
  auto result = wayland_subsurfaces_.emplace(std::move(subsurface));
  DCHECK(result.second);
  return true;
}

bool WaylandWindow::ArrangeSubsurfaceStack(size_t above, size_t below) {
  while (wayland_subsurfaces_.size() < above + below) {
    if (!RequestSubsurface())
      return false;
  }

  DCHECK(subsurface_stack_below_.size() + subsurface_stack_above_.size() >=
         above + below);

  if (subsurface_stack_above_.size() < above) {
    auto splice_start = subsurface_stack_below_.begin();
    for (size_t i = 0; i < below; ++i)
      ++splice_start;
    subsurface_stack_above_.splice(subsurface_stack_above_.end(),
                                   subsurface_stack_below_, splice_start,
                                   subsurface_stack_below_.end());

  } else if (subsurface_stack_below_.size() < below) {
    auto splice_start = subsurface_stack_above_.end();
    for (size_t i = 0; i < below - subsurface_stack_below_.size(); ++i)
      --splice_start;
    subsurface_stack_below_.splice(subsurface_stack_below_.end(),
                                   subsurface_stack_above_, splice_start,
                                   subsurface_stack_above_.end());
  }

  DCHECK(subsurface_stack_below_.size() >= below);
  DCHECK(subsurface_stack_above_.size() >= above);
  return true;
}

bool WaylandWindow::CommitOverlays(
    std::vector<ui::ozone::mojom::WaylandOverlayConfigPtr>& overlays) {
  // |overlays| is sorted from bottom to top.
  std::sort(overlays.begin(), overlays.end(), OverlayStackOrderCompare);

  // Find the location where z_oder becomes non-negative.
  ozone::mojom::WaylandOverlayConfigPtr value =
      ozone::mojom::WaylandOverlayConfig::New();
  auto split = std::lower_bound(overlays.begin(), overlays.end(), value,
                                OverlayStackOrderCompare);
  CHECK(split == overlays.end() || (*split)->z_order >= 0);
  size_t num_primary_planes =
      (split != overlays.end() && (*split)->z_order == 0) ? 1 : 0;

  size_t above = (overlays.end() - split) - num_primary_planes;
  size_t below = split - overlays.begin();
  // Re-arrange the list of subsurfaces to fit the |overlays|. Request extra
  // subsurfaces if needed.
  if (!ArrangeSubsurfaceStack(above, below))
    return false;

  {
    // Iterate through |subsurface_stack_below_|, setup subsurfaces and place
    // them in corresponding order. Commit wl_buffers once a subsurface is
    // configured.
    auto overlay_iter = split - 1;
    for (auto iter = subsurface_stack_below_.begin();
         iter != subsurface_stack_below_.end(); ++iter, --overlay_iter) {
      if (overlay_iter >= overlays.begin()) {
        WaylandSurface* reference_above = nullptr;
        if (overlay_iter == split - 1) {
          // It's possible that |overlays| does not contain primary plane, we
          // still want to place relative to the surface with z_order=0.
          reference_above = root_surface();
        } else {
          reference_above = (*std::next(iter))->wayland_surface();
        }
        (*iter)->ConfigureAndShowSurface(
            (*overlay_iter)->transform, (*overlay_iter)->crop_rect,
            (*overlay_iter)->bounds_rect, (*overlay_iter)->enable_blend,
            nullptr, reference_above);
        connection_->buffer_manager_host()->CommitBufferInternal(
            (*iter)->wayland_surface(), (*overlay_iter)->buffer_id, gfx::Rect(),
            /*wait_for_frame_callback=*/false);
      } else {
        // If there're more subsurfaces requested that we don't need at the
        // moment, hide them.
        (*iter)->Hide();
      }
    }

    // Iterate through |subsurface_stack_above_|, setup subsurfaces and place
    // them in corresponding order. Commit wl_buffers once a subsurface is
    // configured.
    overlay_iter = split + num_primary_planes;
    for (auto iter = subsurface_stack_above_.begin();
         iter != subsurface_stack_above_.end(); ++iter, ++overlay_iter) {
      if (overlay_iter < overlays.end()) {
        WaylandSurface* reference_below = nullptr;
        if (overlay_iter == split + num_primary_planes) {
          // It's possible that |overlays| does not contain primary plane, we
          // still want to place relative to the surface with z_order=0.
          reference_below = root_surface();
        } else {
          reference_below = (*std::prev(iter))->wayland_surface();
        }
        (*iter)->ConfigureAndShowSurface(
            (*overlay_iter)->transform, (*overlay_iter)->crop_rect,
            (*overlay_iter)->bounds_rect, (*overlay_iter)->enable_blend,
            reference_below, nullptr);
        connection_->buffer_manager_host()->CommitBufferInternal(
            (*iter)->wayland_surface(), (*overlay_iter)->buffer_id, gfx::Rect(),
            /*wait_for_frame_callback=*/false);
      } else {
        // If there're more subsurfaces requested that we don't need at the
        // moment, hide them.
        (*iter)->Hide();
      }
    }
  }

  if (num_primary_planes) {
    connection_->buffer_manager_host()->CommitBufferInternal(
        root_surface(), (*split)->buffer_id, (*split)->damage_region,
        /*wait_for_frame_callback=*/true);
  } else {
    // Subsurfaces are set to sync, above operations will only take effects
    // when root_surface is committed.
    connection_->buffer_manager_host()->CommitWithoutBufferInternal(
        root_surface(), /*wait_for_frame_callback=*/true);
  }

  return true;
}

}  // namespace ui
