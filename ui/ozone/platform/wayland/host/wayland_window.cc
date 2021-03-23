// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_window.h"

#include <wayland-cursor.h>
#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/common/features.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor_position.h"
#include "ui/ozone/platform/wayland/host/wayland_data_drag_controller.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"
#include "ui/ozone/platform/wayland/host/wayland_subsurface.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_cursor_shapes.h"
#include "ui/ozone/public/mojom/wayland/wayland_overlay_config.mojom.h"
#include "ui/platform_window/wm/wm_drag_handler.h"
#include "ui/platform_window/wm/wm_drop_handler.h"

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
      wayland_overlay_delegation_enabled_(connection->viewporter() &&
                                          IsWaylandOverlayDelegationEnabled()),
      accelerated_widget_(
          connection->wayland_window_manager()->AllocateAcceleratedWidget()) {
  // Set a class property key, which allows |this| to be used for drag action.
  SetWmDragHandler(this, this);
}

WaylandWindow::~WaylandWindow() {
  shutting_down_ = true;

  PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);

  if (wayland_overlay_delegation_enabled_) {
    connection_->wayland_window_manager()->RemoveSubsurface(
        GetWidget(), primary_subsurface_.get());
  }
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
  int32_t old_scale = buffer_scale();
  root_surface_->SetBufferScale(new_scale, update_bounds);
  // We need to keep DIP size of the window the same whenever the scale changes.
  if (update_bounds)
    SetBoundsDip(gfx::ScaleToRoundedRect(bounds_px_, 1.0 / old_scale));
}

gfx::AcceleratedWidget WaylandWindow::GetWidget() const {
  return accelerated_widget_;
}

void WaylandWindow::SetPointerFocus(bool focus) {
  has_pointer_focus_ = focus;

  // Whenever the window gets the pointer focus back, we must reinitialize the
  // cursor. Otherwise, it is invalidated whenever the pointer leaves the
  // surface and is not restored by the Wayland compositor.
  if (has_pointer_focus_ && bitmap_) {
    // Check for theme-provided cursor.
    if (bitmap_->platform_data()) {
      connection_->SetPlatformCursor(
          reinterpret_cast<wl_cursor*>(bitmap_->platform_data()),
          buffer_scale());
    } else {
      // Translate physical pixels to DIPs.
      gfx::Point hotspot_in_dips =
          gfx::ScaleToRoundedPoint(bitmap_->hotspot(), 1.0f / ui_scale_);
      connection_->SetCursorBitmap(bitmap_->bitmaps(), hotspot_in_dips,
                                   buffer_scale());
    }
  }
}

bool WaylandWindow::StartDrag(const ui::OSExchangeData& data,
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

void WaylandWindow::CancelDrag() {
  if (drag_loop_quit_closure_.is_null())
    return;
  std::move(drag_loop_quit_closure_).Run();
}

void WaylandWindow::Show(bool inactive) {
  if (background_buffer_id_ != 0u)
    should_attach_background_buffer_ = true;
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

void WaylandWindow::PrepareForShutdown() {
  if (drag_handler_delegate_)
    OnDragSessionClose(DragDropTypes::DRAG_NONE);
}

void WaylandWindow::SetBounds(const gfx::Rect& bounds_px) {
  if (bounds_px_ == bounds_px)
    return;
  bounds_px_ = bounds_px;

  if (update_visual_size_immediately_)
    UpdateVisualSize(bounds_px.size());
  delegate_->OnBoundsChanged(bounds_px_);
}

gfx::Rect WaylandWindow::GetBounds() const {
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

void WaylandWindow::Activate() {}

void WaylandWindow::Deactivate() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WaylandWindow::SetUseNativeFrame(bool use_native_frame) {
  // Do nothing here since only shell surfaces can handle server-side
  // decoration.
}

bool WaylandWindow::ShouldUseNativeFrame() const {
  // Always returns false here since only shell surfaces can handle server-side
  // decoration.
  return false;
}

void WaylandWindow::SetCursor(PlatformCursor cursor) {
  scoped_refptr<BitmapCursorOzone> bitmap =
      BitmapCursorFactoryOzone::GetBitmapCursor(cursor);
  if (bitmap_ == bitmap)
    return;

  bitmap_ = bitmap;

  if (!bitmap_) {
    // Hide the cursor.
    connection_->SetCursorBitmap(std::vector<SkBitmap>(), gfx::Point(),
                                 buffer_scale());
    return;
  }
  // Check for theme-provided cursor.
  if (bitmap_->platform_data()) {
    connection_->SetPlatformCursor(
        reinterpret_cast<wl_cursor*>(bitmap_->platform_data()), buffer_scale());
    return;
  }
  // Check for Wayland server-side cursor support (e.g. exo for lacros).
  if (connection_->zcr_cursor_shapes()) {
    base::Optional<int32_t> shape =
        WaylandZcrCursorShapes::ShapeFromType(bitmap->type());
    // If the server supports this cursor type, use a server-side cursor.
    if (shape.has_value()) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      // Lacros should not load image assets for default cursors. See
      // BitmapCursorFactoryOzone::GetDefaultCursor().
      DCHECK(bitmap_->bitmaps().empty());
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
      connection_->zcr_cursor_shapes()->SetCursorShape(shape.value());
      return;
    }
    // Fall through to client-side bitmap cursors.
  }
  // Translate physical pixels to DIPs.
  gfx::Point hotspot_in_dips =
      gfx::ScaleToRoundedPoint(bitmap_->hotspot(), 1.0f / ui_scale_);
  connection_->SetCursorBitmap(bitmap_->bitmaps(), hotspot_in_dips,
                               buffer_scale());
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

bool WaylandWindow::ShouldUseLayerForShapedWindow() const {
  return true;
}

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
    UpdateCursorPositionFromEvent(Event::Clone(*event));
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

void WaylandWindow::HandleSurfaceConfigure(uint32_t serial) {
  NOTREACHED()
      << "Only shell surfaces must receive HandleSurfaceConfigure calls.";
}

void WaylandWindow::HandleToplevelConfigure(int32_t widht,
                                            int32_t height,
                                            bool is_maximized,
                                            bool is_fullscreen,
                                            bool is_activated) {
  NOTREACHED()
      << "Only shell toplevels must receive HandleToplevelConfigure calls.";
}

void WaylandWindow::HandlePopupConfigure(const gfx::Rect& bounds_dip) {
  NOTREACHED() << "Only shell popups must receive HandlePopupConfigure calls.";
}

void WaylandWindow::UpdateVisualSize(const gfx::Size& size_px) {
  visual_size_px_ = size_px;
  UpdateWindowMask();
}

void WaylandWindow::OnCloseRequest() {
  delegate_->OnCloseRequest();
}

base::Optional<std::vector<gfx::Rect>> WaylandWindow::GetWindowShape() const {
  return base::nullopt;
}

void WaylandWindow::UpdateWindowMask() {
  UpdateWindowShape();
  root_surface_->SetOpaqueRegion(gfx::Rect(visual_size_px()));
}

void WaylandWindow::UpdateWindowShape() {}

void WaylandWindow::OnDragEnter(const gfx::PointF& point,
                                std::unique_ptr<OSExchangeData> data,
                                int operation) {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler)
    return;

  auto location_px = gfx::ScalePoint(TranslateLocationToRootWindow(point),
                                     buffer_scale(), buffer_scale());

  // Wayland sends locations in DIP so they need to be translated to
  // physical pixels.
  // TODO(crbug.com/1102857): get the real event modifier here.
  drop_handler->OnDragEnter(location_px, std::move(data), operation,
                            /*modifiers=*/0);
}

int WaylandWindow::OnDragMotion(const gfx::PointF& point, int operation) {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler)
    return 0;

  auto location_px = gfx::ScalePoint(TranslateLocationToRootWindow(point),
                                     buffer_scale(), buffer_scale());

  // Wayland sends locations in DIP so they need to be translated to
  // physical pixels.
  // TODO(crbug.com/1102857): get the real event modifier here.
  return drop_handler->OnDragMotion(location_px, operation,
                                    /*modifiers=*/0);
}

void WaylandWindow::OnDragDrop() {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler)
    return;
  // TODO(crbug.com/1102857): get the real event modifier here.
  drop_handler->OnDragDrop({}, /*modifiers=*/0);
}

void WaylandWindow::OnDragLeave() {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler)
    return;
  drop_handler->OnDragLeave();
}

void WaylandWindow::OnDragSessionClose(uint32_t dnd_action) {
  DCHECK(drag_handler_delegate_);
  drag_handler_delegate_->OnDragFinished(dnd_action);
  drag_handler_delegate_ = nullptr;
  connection()->event_source()->ResetPointerFlags();
  std::move(drag_loop_quit_closure_).Run();
}

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

  if (wayland_overlay_delegation_enabled_) {
    primary_subsurface_ =
        std::make_unique<WaylandSubsurface>(connection_, this);
    if (!primary_subsurface_->surface())
      return false;
    connection_->wayland_window_manager()->AddSubsurface(
        GetWidget(), primary_subsurface_.get());
  }

  connection_->ScheduleFlush();

  PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  delegate_->OnAcceleratedWidgetAvailable(GetWidget());

  // Will do nothing for menus because they have got their scale above.
  UpdateBufferScale(false);
  root_surface_->SetOpaqueRegion(gfx::Rect(bounds_px_.size()));

  return true;
}

WaylandWindow* WaylandWindow::GetRootParentWindow() {
  return parent_window_ ? parent_window_->GetRootParentWindow() : this;
}

void WaylandWindow::AddEnteredOutputId(struct wl_output* output) {
  // Wayland does weird things for menus so instead of tracking outputs that
  // we entered or left, we take that from the parent window and ignore this
  // event.
  if (wl::IsMenuType(type()) || type() == ui::PlatformWindowType::kTooltip)
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

gfx::PointF WaylandWindow::TranslateLocationToRootWindow(
    const gfx::PointF& location) {
  auto* root_window = GetRootParentWindow();
  DCHECK(root_window);
  if (root_window == this)
    return location;

  gfx::Vector2d offset =
      GetBounds().origin() - root_window->GetBounds().origin();
  return location + gfx::Vector2dF(offset);
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

  if (overlays.front()->z_order == INT32_MIN)
    --below;

  // Re-arrange the list of subsurfaces to fit the |overlays|. Request extra
  // subsurfaces if needed.
  if (!ArrangeSubsurfaceStack(above, below))
    return false;

  if (wayland_overlay_delegation_enabled_)
    connection_->buffer_manager_host()->StartFrame(root_surface());

  {
    // Iterate through |subsurface_stack_below_|, setup subsurfaces and place
    // them in corresponding order. Commit wl_buffers once a subsurface is
    // configured.
    auto overlay_iter = split - 1;
    for (auto iter = subsurface_stack_below_.begin();
         iter != subsurface_stack_below_.end(); ++iter, --overlay_iter) {
      if (overlays.front()->z_order == INT32_MIN
              ? overlay_iter >= ++overlays.begin()
              : overlay_iter >= overlays.begin()) {
        WaylandSurface* reference_above = nullptr;
        if (overlay_iter == split - 1) {
          // It's possible that |overlays| does not contain primary plane, we
          // still want to place relative to the surface with z_order=0.
          reference_above = primary_subsurface_->wayland_surface();
        } else {
          reference_above = (*std::next(iter))->wayland_surface();
        }
        (*iter)->ConfigureAndShowSurface(
            (*overlay_iter)->transform, (*overlay_iter)->crop_rect,
            (*overlay_iter)->bounds_rect, (*overlay_iter)->enable_blend,
            nullptr, reference_above);
        connection_->buffer_manager_host()->CommitBufferInternal(
            (*iter)->wayland_surface(), (*overlay_iter)->buffer_id, gfx::Rect(),
            /*wait_for_frame_callback=*/true,
            /*commit_synced_subsurface=*/true,
            std::move((*overlay_iter)->access_fence_handle));
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
          reference_below = primary_subsurface_->wayland_surface();
        } else {
          reference_below = (*std::prev(iter))->wayland_surface();
        }
        (*iter)->ConfigureAndShowSurface(
            (*overlay_iter)->transform, (*overlay_iter)->crop_rect,
            (*overlay_iter)->bounds_rect, (*overlay_iter)->enable_blend,
            reference_below, nullptr);
        connection_->buffer_manager_host()->CommitBufferInternal(
            (*iter)->wayland_surface(), (*overlay_iter)->buffer_id, gfx::Rect(),
            /*wait_for_frame_callback=*/true,
            /*commit_synced_subsurface=*/true,
            std::move((*overlay_iter)->access_fence_handle));
      } else {
        // If there're more subsurfaces requested that we don't need at the
        // moment, hide them.
        (*iter)->Hide();
      }
    }
  }

  if (!num_primary_planes && overlays.front()->z_order == INT32_MIN)
    split = overlays.begin();
  UpdateVisualSize((*split)->bounds_rect.size());
  root_surface_->SetViewportDestination(visual_size_px_);

  if (!wayland_overlay_delegation_enabled_) {
    root_surface_->SetViewportSource((*split)->crop_rect);
    connection_->buffer_manager_host()->CommitBufferInternal(
        root_surface(), (*split)->buffer_id, (*split)->damage_region,
        /*wait_for_frame_callback=*/true);
    return true;
  }

  if (num_primary_planes) {
    primary_subsurface_->ConfigureAndShowSurface(
        (*split)->transform, (*split)->crop_rect, (*split)->bounds_rect,
        (*split)->enable_blend, nullptr, nullptr);
    connection_->buffer_manager_host()->CommitBufferInternal(
        primary_subsurface_->wayland_surface(), (*split)->buffer_id,
        (*split)->damage_region,
        /*wait_for_frame_callback=*/true,
        /*commit_synced_subsurface=*/true,
        std::move((*split)->access_fence_handle));
  }

  gfx::Rect background_damage;
  if (overlays.front()->z_order == INT32_MIN) {
    background_buffer_id_ = overlays.front()->buffer_id;
    background_damage = overlays.front()->damage_region;
    should_attach_background_buffer_ = true;
  }

  if (should_attach_background_buffer_) {
    connection_->buffer_manager_host()->EndFrame(background_buffer_id_,
                                                 background_damage);
    should_attach_background_buffer_ = false;
  } else {
    // Subsurfaces are set to sync, above surface configs will only take effect
    // when root_surface is committed.
    connection_->buffer_manager_host()->EndFrame();
  }

  return true;
}

}  // namespace ui
