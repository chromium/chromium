// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_window.h"

#include <wayland-cursor.h>
#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/cursor/mojom/cursor_type.mojom.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
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
#include "ui/ozone/platform/wayland/host/wayland_surface.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_cursor_shapes.h"
#include "ui/ozone/public/mojom/wayland/wayland_overlay_config.mojom.h"
#include "ui/platform_window/common/platform_window_defaults.h"
#include "ui/platform_window/wm/wm_drag_handler.h"
#include "ui/platform_window/wm/wm_drop_handler.h"

namespace ui {
namespace {

using mojom::CursorType;
using mojom::DragOperation;

bool OverlayStackOrderCompare(
    const ui::ozone::mojom::WaylandOverlayConfigPtr& i,
    const ui::ozone::mojom::WaylandOverlayConfigPtr& j) {
  return i->z_order < j->z_order;
}

}  // namespace

WaylandWindow::WaylandWindow(PlatformWindowDelegate* delegate,
                             WaylandConnection* connection)
    : delegate_(delegate),
      connection_(connection),
      wayland_overlay_delegation_enabled_(connection->viewporter() &&
                                          IsWaylandOverlayDelegationEnabled()),
      accelerated_widget_(
          connection->wayland_window_manager()->AllocateAcceleratedWidget()),
      ui_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
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

  // This might have already been hidden and another window has been shown.
  // Thus, the parent will have another child window. Do not reset it.
  if (parent_window_ && parent_window_->child_window() == this)
    parent_window_->set_child_window(nullptr);
}

void WaylandWindow::OnWindowLostCapture() {
  delegate_->OnLostCapture();
}

void WaylandWindow::UpdateWindowScale(bool update_bounds) {
  DCHECK(connection_->wayland_output_manager());

  auto preferred_outputs_id = GetPreferredEnteredOutputId();
  if (preferred_outputs_id == 0) {
    // If non of the output are entered, use primary output. This is what
    // WaylandScreen returns back to ScreenOzone.
    auto* primary_output =
        connection_->wayland_output_manager()->GetPrimaryOutput();
    // We don't know our primary output - WaylandScreen hasn't been created
    // yet.
    if (!primary_output)
      return;
    preferred_outputs_id = primary_output->output_id();
  }

  auto* output =
      connection_->wayland_output_manager()->GetOutput(preferred_outputs_id);
  // There can be a race between sending leave output event and destroying
  // wl_outputs. Thus, explicitly check if the output exist.
  if (!output)
    return;

  int32_t new_scale = output->scale_factor();
  ui_scale_ = output->GetUIScaleFactor();

  int32_t old_scale = window_scale();
  window_scale_ = new_scale;

  // We need to keep DIP size of the window the same whenever the scale changes.
  if (update_bounds)
    SetBoundsDip(gfx::ScaleToRoundedRect(bounds_px_, 1.0 / old_scale));

  // Propagate update to the child windows
  if (child_window_)
    child_window_->UpdateWindowScale(update_bounds);
}

gfx::AcceleratedWidget WaylandWindow::GetWidget() const {
  return accelerated_widget_;
}

void WaylandWindow::SetWindowScale(int32_t new_scale) {
  DCHECK_GE(new_scale, 0);
  window_scale_ = new_scale;
}

uint32_t WaylandWindow::GetPreferredEnteredOutputId() {
  // Child windows don't store entered outputs. Instead, take the window's
  // root parent window and use its preferred output.
  if (parent_window_)
    return GetRootParentWindow()->GetPreferredEnteredOutputId();

  // It can be either a toplevel window that hasn't entered any outputs yet, or
  // still a non toplevel window that doesn't have a parent (for example, a
  // wl_surface that is being dragged).
  if (root_surface_->entered_outputs().empty())
    return 0;

  // PlatformWindowType::kPopup are created as toplevel windows as well.
  DCHECK(type() == PlatformWindowType::kWindow ||
         type() == PlatformWindowType::kPopup);

  // A window can be located on two or more displays. Thus, return the id of the
  // output that has the biggest scale factor. Otherwise, use the very first one
  // that was entered. This way, we can be sure that the contents of the Window
  // are rendered at correct dpi when a user moves the window between displays.
  auto* preferred_output = *root_surface_->entered_outputs().begin();
  for (WaylandOutput* output : root_surface_->entered_outputs()) {
    if (output->scale_factor() > preferred_output->scale_factor())
      preferred_output = output;
  }

  return preferred_output->output_id();
}

void WaylandWindow::SetPointerFocus(bool focus) {
  has_pointer_focus_ = focus;

  // Whenever the window gets the pointer focus back, the cursor shape must be
  // updated. Otherwise, it is invalidated upon wl_pointer::leave and is not
  // restored by the Wayland compositor.
  if (has_pointer_focus_ && cursor_)
    UpdateCursorShape(cursor_);
}

bool WaylandWindow::StartDrag(const ui::OSExchangeData& data,
                              int operation,
                              gfx::NativeCursor cursor,
                              bool can_grab_pointer,
                              WmDragHandler::Delegate* delegate) {
  if (!connection_->data_drag_controller()->StartSession(data, operation))
    return false;

  DCHECK(!drag_handler_delegate_);
  drag_handler_delegate_ = delegate;

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
  // Mutter compositor crashes if we don't remove subsurface roles when hiding.
  if (primary_subsurface_)
    primary_subsurface()->Hide();
  for (auto& subsurface : wayland_subsurfaces_)
    subsurface->Hide();
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
    OnDragSessionClose(DragOperation::kNone);
}

void WaylandWindow::SetBounds(const gfx::Rect& bounds_px) {
  gfx::Rect adjusted_bounds_px = bounds_px;

  if (const auto min_size = delegate_->GetMinimumSizeForWindow()) {
    if (min_size->width() > 0 && adjusted_bounds_px.width() < min_size->width())
      adjusted_bounds_px.set_width(min_size->width());
    if (min_size->height() > 0 &&
        adjusted_bounds_px.height() < min_size->height())
      adjusted_bounds_px.set_height(min_size->height());
  }
  if (const auto max_size = delegate_->GetMaximumSizeForWindow()) {
    if (max_size->width() > 0 && adjusted_bounds_px.width() > max_size->width())
      adjusted_bounds_px.set_width(max_size->width());
    if (max_size->height() > 0 &&
        adjusted_bounds_px.height() > max_size->height())
      adjusted_bounds_px.set_height(max_size->height());
  }

  if (bounds_px_ == adjusted_bounds_px)
    return;
  bounds_px_ = adjusted_bounds_px;

  pending_buffer_scale_.emplace_back(
      std::make_pair(bounds_px_.size(), window_scale()));

  if (update_visual_size_immediately_)
    UpdateVisualSize(bounds_px.size());
  delegate_->OnBoundsChanged(bounds_px_);
}

gfx::Rect WaylandWindow::GetBounds() const {
  return bounds_px_;
}

gfx::Rect WaylandWindow::GetBoundsInDIP() const {
  return gfx::ScaleToRoundedRect(bounds_px_, 1.0 / window_scale());
}

void WaylandWindow::SetTitle(const std::u16string& title) {}

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

void WaylandWindow::SetCursor(scoped_refptr<PlatformCursor> platform_cursor) {
  DCHECK(platform_cursor);

  if (cursor_ == platform_cursor)
    return;

  UpdateCursorShape(BitmapCursorOzone::FromPlatformCursor(platform_cursor));
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

bool WaylandWindow::IsTranslucentWindowOpacitySupported() const {
  // Wayland compositors always support translucency.
  return true;
}

bool WaylandWindow::ShouldUpdateWindowShape() const {
  return false;
}

bool WaylandWindow::CanDispatchEvent(const PlatformEvent& event) {
  if (event->IsMouseEvent() || event->IsPinchEvent())
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
        event->AsLocatedEvent()->location_f(), window_scale(), window_scale()));

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
  if (visual_size_px_ == size_px)
    return;
  visual_size_px_ = size_px;
  UpdateWindowMask();
}

void WaylandWindow::OnCloseRequest() {
  delegate_->OnCloseRequest();
}

absl::optional<std::vector<gfx::Rect>> WaylandWindow::GetWindowShape() const {
  return absl::nullopt;
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
                                     window_scale(), window_scale());

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
                                     window_scale(), window_scale());

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

void WaylandWindow::OnDragSessionClose(DragOperation operation) {
  DCHECK(drag_handler_delegate_);
  drag_handler_delegate_->OnDragFinished(operation);
  drag_handler_delegate_ = nullptr;
  connection()->event_source()->ResetPointerFlags();
  std::move(drag_loop_quit_closure_).Run();
}

void WaylandWindow::SetBoundsDip(const gfx::Rect& bounds_dip) {
  SetBounds(gfx::ScaleToRoundedRect(bounds_dip, window_scale()));
}

bool WaylandWindow::Initialize(PlatformWindowInitProperties properties) {
  root_surface_ = std::make_unique<WaylandSurface>(connection_, this);
  if (!root_surface_->Initialize()) {
    LOG(ERROR) << "Failed to create wl_surface";
    return false;
  }

  // Update visual size in tests immediately if the test config is set.
  // Otherwise, such tests as interactive_ui_tests fail.
  set_update_visual_size_immediately(UseTestConfigForPlatformWindows());

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

  root_surface_->SetOpaqueRegion(gfx::Rect(bounds_px_.size()));

  return true;
}

WaylandWindow* WaylandWindow::GetRootParentWindow() {
  return parent_window_ ? parent_window_->GetRootParentWindow() : this;
}

void WaylandWindow::OnEnteredOutputIdAdded() {
  // Wayland does weird things for menus so instead of tracking outputs that
  // we entered or left, we take that from the parent window and ignore this
  // event.
  if (AsWaylandPopup())
    return;

  UpdateWindowScale(true);
}

void WaylandWindow::OnEnteredOutputIdRemoved() {
  // Wayland does weird things for menus so instead of tracking outputs that
  // we entered or left, we take that from the parent window and ignore this
  // event.
  if (AsWaylandPopup())
    return;

  UpdateWindowScale(true);
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

WaylandPopup* WaylandWindow::AsWaylandPopup() {
  return nullptr;
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
  if (overlays.empty())
    return true;

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

  if (wayland_overlay_delegation_enabled_) {
    primary_subsurface()->Show();
    connection_->buffer_manager_host()->StartFrame(root_surface());
  }

  // Update buffer scale before subsurfaces are configured.
  {
    auto main_overlay = split;
    if (split == overlays.end() && overlays.front()->z_order == INT32_MIN)
      main_overlay = overlays.begin();

    // Either use current scale of the window or pending scale whenever visual
    // size updates. window_scale() won't be used if we are in process of
    // changing bounds.
    int32_t buffer_scale = window_scale();
    auto result =
        std::find_if(pending_buffer_scale_.begin(), pending_buffer_scale_.end(),
                     [&visual_size = (*main_overlay)->bounds_rect.size()](
                         auto& item) { return visual_size == item.first; });
    if (result != pending_buffer_scale_.end()) {
      buffer_scale = result->second;
      pending_buffer_scale_.erase(pending_buffer_scale_.begin(), ++result);
    }
    root_surface()->SetSurfaceBufferScale(buffer_scale);
  }

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
            (*overlay_iter)->transform, (*overlay_iter)->bounds_rect,
            root_surface()->buffer_scale(), (*overlay_iter)->enable_blend,
            nullptr, reference_above);
        (*iter)->wayland_surface()->SetViewportSource(
            (*overlay_iter)->crop_rect);
        (*iter)->wayland_surface()->SetViewportDestination(
            (*overlay_iter)->bounds_rect.size());
        connection_->buffer_manager_host()->CommitBufferInternal(
            (*iter)->wayland_surface(), (*overlay_iter)->buffer_id,
            (*overlay_iter)->damage_region,
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
            (*overlay_iter)->transform, (*overlay_iter)->bounds_rect,
            root_surface()->buffer_scale(), (*overlay_iter)->enable_blend,
            reference_below, nullptr);
        (*iter)->wayland_surface()->SetViewportSource(
            (*overlay_iter)->crop_rect);
        (*iter)->wayland_surface()->SetViewportDestination(
            (*overlay_iter)->bounds_rect.size());
        connection_->buffer_manager_host()->CommitBufferInternal(
            (*iter)->wayland_surface(), (*overlay_iter)->buffer_id,
            (*overlay_iter)->damage_region,
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

  if (split == overlays.end() && overlays.front()->z_order == INT32_MIN)
    split = overlays.begin();

  UpdateVisualSize((*split)->bounds_rect.size());

  if (!wayland_overlay_delegation_enabled_) {
    root_surface_->SetViewportSource((*split)->crop_rect);
    // TODO(fangzhoug): Refactor some of this logic s.t. the decision of whether
    //   to apply viewport.destination is made at commit time.
    root_surface_->SetViewportDestination((*split)->crop_rect ==
                                                  gfx::RectF(1.f, 1.f)
                                              ? gfx::Size()
                                              : (*split)->bounds_rect.size());
    connection_->buffer_manager_host()->CommitBufferInternal(
        root_surface(), (*split)->buffer_id, (*split)->damage_region,
        /*wait_for_frame_callback=*/true);
    return true;
  }

  if (num_primary_planes) {
    // Mutter has incorrect damage when processing un-cropped buffer commits
    // with viewport.destination == buffer.size. So do not set
    // viewport.destination to primary planes if crop_rect is uniform.
    // TODO(fangzhoug): Refactor some of this logic s.t. the decision of whether
    //   to apply viewport.destination is made at commit time. Right now PIP
    //   would have incorrect size b/c it is fullscreen overlay scheduled at
    //   z_order=0.
    primary_subsurface_->ConfigureAndShowSurface(
        (*split)->transform, (*split)->bounds_rect,
        root_surface()->buffer_scale(), (*split)->enable_blend, nullptr,
        nullptr);
    primary_subsurface_->wayland_surface()->SetViewportSource(
        (*split)->crop_rect);
    primary_subsurface_->wayland_surface()->SetViewportDestination(
        (*split)->crop_rect == gfx::RectF(1.f, 1.f)
            ? gfx::Size()
            : (*split)->bounds_rect.size());
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

  root_surface_->SetViewportDestination(visual_size_px_);
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

void WaylandWindow::UpdateCursorShape(scoped_refptr<BitmapCursorOzone> cursor) {
  DCHECK(cursor);
  absl::optional<int32_t> shape =
      WaylandZcrCursorShapes::ShapeFromType(cursor->type());

  // Round cursor scale factor to ceil as wl_surface.set_buffer_scale accepts
  // only integers.
  if (cursor->type() == CursorType::kNone) {  // Hide the cursor.
    connection_->SetCursorBitmap(
        {}, gfx::Point(), std::ceil(cursor->cursor_image_scale_factor()));
  } else if (cursor->platform_data()) {  // Check for theme-provided cursor.
    connection_->SetPlatformCursor(
        reinterpret_cast<wl_cursor*>(cursor->platform_data()),
        std::ceil(cursor->cursor_image_scale_factor()));
  } else if (connection_->zcr_cursor_shapes() &&
             shape.has_value()) {  // Check for Wayland server-side cursor
                                   // support (e.g. exo for lacros).
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Lacros should not load image assets for default cursors. See
    // BitmapCursorFactoryOzone::GetDefaultCursor().
    DCHECK(cursor->bitmaps().empty());
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    connection_->zcr_cursor_shapes()->SetCursorShape(shape.value());
  } else {  // Use client-side bitmap cursors as fallback.
    // Translate physical pixels to DIPs.
    gfx::Point hotspot_in_dips =
        gfx::ScaleToRoundedPoint(cursor->hotspot(), 1.0f / ui_scale_);
    connection_->SetCursorBitmap(
        cursor->bitmaps(), hotspot_in_dips,
        std::ceil(cursor->cursor_image_scale_factor()));
  }
  // The new cursor needs to be stored last to avoid deleting the old cursor
  // while it's still in use.
  cursor_ = cursor;
}

}  // namespace ui
