// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_window_drag_controller.h"

#include <extended-drag-unstable-v1-client-protocol.h>
#include <wayland-client-protocol.h>

#include <cstdint>
#include <memory>
#include <ostream>
#include <utility>

#include "base/callback.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/scoped_event_dispatcher.h"
#include "ui/events/platform_event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor_position.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_data_offer.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_screen.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"
#include "ui/ozone/platform/wayland/host/wayland_surface.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

namespace {

// Custom mime type used for window dragging DND sessions.
constexpr char kMimeTypeChromiumWindow[] = "chromium/x-window";

// DND action used in window dragging DND sessions.
constexpr uint32_t kDndActionWindowDrag =
    WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;

// Value intentionally high to exit the horizontal rail threshold in
// SnapScrollController, in case of an upwards tab dragging detach with touch.
constexpr int kHorizontalRailExitThreshold = -1000;

}  // namespace

class WaylandWindowDragController::ExtendedDragSource {
 public:
  ExtendedDragSource(WaylandConnection& connection, wl_data_source* source)
      : connection_(connection) {
    DCHECK(connection.extended_drag_v1());
    uint32_t options = ZCR_EXTENDED_DRAG_V1_OPTIONS_ALLOW_SWALLOW |
                       ZCR_EXTENDED_DRAG_V1_OPTIONS_ALLOW_DROP_NO_TARGET |
                       ZCR_EXTENDED_DRAG_V1_OPTIONS_LOCK_CURSOR;
    source_.reset(zcr_extended_drag_v1_get_extended_drag_source(
        connection.extended_drag_v1(), source, options));
    DCHECK(source_);
  }

  void SetDraggedWindow(WaylandToplevelWindow* window,
                        const gfx::Vector2d& offset) {
    auto* surface = window ? window->root_surface()->surface() : nullptr;
    zcr_extended_drag_source_v1_drag(source_.get(), surface, offset.x(),
                                     offset.y());
    connection_->Flush();
  }

 private:
  wl::Object<zcr_extended_drag_source_v1> source_;
  const raw_ref<WaylandConnection> connection_;
};

WaylandWindowDragController::WaylandWindowDragController(
    WaylandConnection* connection,
    WaylandDataDeviceManager* device_manager,
    WaylandPointer::Delegate* pointer_delegate,
    WaylandTouch::Delegate* touch_delegate)
    : connection_(connection),
      data_device_manager_(device_manager),
      data_device_(device_manager->GetDevice()),
      window_manager_(connection_->window_manager()),
      pointer_delegate_(pointer_delegate),
      touch_delegate_(touch_delegate) {
  DCHECK(data_device_);
  DCHECK(pointer_delegate_);
  DCHECK(touch_delegate_);
}

WaylandWindowDragController::~WaylandWindowDragController() = default;

bool WaylandWindowDragController::StartDragSession() {
  if (state_ != State::kIdle)
    return true;

  // TODO(crbug.com/1246529): Drop the heuristic below which detects the "drag
  // source" info in favor of having it injected by the upper level layers.
  auto [serial, origin] = GetSerialAndOrigin();
  if (!serial || !origin) {
    LOG(ERROR) << "Failed to retrieve dnd serial / origin window.";
    return false;
  }

  DVLOG(1) << "Starting DND session.";
  state_ = State::kAttached;
  origin_window_ = origin;
  drag_source_ = serial->type == wl::SerialType::kTouchPress
                     ? DragSource::kTouch
                     : DragSource::kMouse;

  DCHECK(!data_source_);
  data_source_ = data_device_manager_->CreateSource(this);
  data_source_->Offer({kMimeTypeChromiumWindow});
  data_source_->SetDndActions(kDndActionWindowDrag);

  if (IsExtendedDragAvailableInternal()) {
    extended_drag_source_ = std::make_unique<ExtendedDragSource>(
        *connection_, data_source_->data_source());
  } else {
    LOG(ERROR) << "zcr_extended_drag_v1 extension not available! "
               << "Window/Tab dragging won't be fully functional.";
  }

  data_device_->StartDrag(*data_source_, *origin_window_, serial->value,
                          /*icon_surface=*/nullptr, this);
  pointer_grab_owner_ = origin_window_;
  should_process_drag_event_ = false;

  // Observe window so we can take ownership of the origin surface in case it
  // is destroyed during the DND session.
  window_manager_->AddObserver(this);
  return true;
}

bool WaylandWindowDragController::Drag(WaylandToplevelWindow* window,
                                       const gfx::Vector2d& offset) {
  DCHECK_GE(state_, State::kAttached);
  DCHECK(window);

  SetDraggedWindow(window, offset);
  state_ = State::kDetached;
  RunLoop();
  SetDraggedWindow(nullptr, {});

  DCHECK(state_ == State::kAttaching || state_ == State::kDropped ||
         state_ == State::kCancelled);
  if (state_ == State::kAttaching) {
    state_ = State::kAttached;
    return false;
  }

  auto state = state_;
  HandleDropAndResetState();

  return state != State::kCancelled;
}

void WaylandWindowDragController::StopDragging() {
  if (state_ != State::kDetached)
    return;

  DVLOG(1) << "End drag loop requested. state=" << state_;

  // This function is supposed to be called to indicate that the window was just
  // snapped into a tab strip. So switch to |kAttached| state, store the focused
  // window as the pointer grabber and ask to quit the nested loop.
  state_ = State::kAttaching;
  pointer_grab_owner_ =
      window_manager_->GetCurrentPointerOrTouchFocusedWindow();
  QuitLoop();
}

bool WaylandWindowDragController::IsDragSource() const {
  DCHECK(data_source_);
  return true;
}

// Icon drawing and update for window/tab dragging is handled by buffer manager.
void WaylandWindowDragController::DrawIcon() {}

void WaylandWindowDragController::OnDragOffer(
    std::unique_ptr<WaylandDataOffer> offer) {
  DCHECK_GE(state_, State::kAttached);
  DCHECK(offer);
  DCHECK(!data_offer_);

  DVLOG(1) << "OnOffer. mime_types=" << offer->mime_types().size();
  data_offer_ = std::move(offer);
}

void WaylandWindowDragController::OnDragEnter(WaylandWindow* window,
                                              const gfx::PointF& location,
                                              uint32_t serial) {
  DCHECK_GE(state_, State::kAttached);
  DCHECK(window);
  DCHECK(data_source_);
  DCHECK(data_offer_);

  drag_target_window_ = window;

  // Forward focus change event to the input delegate, so other components, such
  // as WaylandScreen, are able to properly retrieve focus related info during
  // window dragging sesstions.
  pointer_location_ = location;

  DCHECK(drag_source_.has_value());
  // Check if this is necessary.
  if (*drag_source_ == DragSource::kMouse) {
    pointer_delegate_->OnPointerFocusChanged(
        window, location, wl::EventDispatchPolicy::kImmediate);
  } else {
    touch_delegate_->OnTouchFocusChanged(window);
  }

  DVLOG(1) << "OnEnter. widget=" << window->GetWidget();

  // TODO(crbug.com/1102946): Exo does not support custom mime types. In this
  // case, |data_offer_| will hold an empty mime_types list and, at this point,
  // it's safe just to skip the offer checks and requests here.
  if (!base::Contains(data_offer_->mime_types(), kMimeTypeChromiumWindow)) {
    DVLOG(1) << "OnEnter. No valid mime type found.";
    return;
  }

  // Accept the offer and set the dnd action.
  data_offer_->SetDndActions(kDndActionWindowDrag);
  data_offer_->Accept(serial, kMimeTypeChromiumWindow);
}

void WaylandWindowDragController::OnDragMotion(const gfx::PointF& location) {
  DCHECK(drag_target_window_);

  DCHECK_GE(state_, State::kAttached);
  DVLOG(2) << "OnMotion. location=" << location.ToString();

  // Motion events are not expected to be dispatched while waiting for the drag
  // loop to exit, ie: kAttaching transitional state. See crbug.com/1169446.
  if (state_ == State::kAttaching)
    return;

  // Forward cursor location update info to the input handling delegate.
  should_process_drag_event_ = true;
  pointer_location_ = location;

  if (*drag_source_ == DragSource::kMouse) {
    pointer_delegate_->OnPointerMotionEvent(
        location, wl::EventDispatchPolicy::kImmediate);
  } else {
    const auto touch_pointer_ids = touch_delegate_->GetActiveTouchPointIds();
    LOG_IF(WARNING, touch_pointer_ids.size() != 1u)
        << "Unexpected touch drag motion. Active touch_points: "
        << touch_pointer_ids.size();
    if (!touch_pointer_ids.empty()) {
      touch_delegate_->OnTouchMotionEvent(location, base::TimeTicks::Now(),
                                          touch_pointer_ids[0],
                                          wl::EventDispatchPolicy::kImmediate);
    }
  }
}

void WaylandWindowDragController::OnDragLeave() {
  DCHECK_GE(state_, State::kAttached);

  drag_target_window_ = nullptr;

  // In order to guarantee ET_MOUSE_RELEASED event is delivered once the DND
  // session finishes, the focused window is not reset here. This is similar to
  // the "implicit grab" behavior implemented by Wayland compositors for
  // wl_pointer events. Additionally, this makes it possible for the drag
  // controller to overcome deviations in the order that wl_data_source and
  // wl_pointer events arrive when the drop happens. For example, unlike Weston
  // and Sway, Gnome Shell <= 2.26 sends them in the following order:
  //
  // wl_data_device.leave >  wl_pointer.enter > wl_data_source.cancel/finish
  //
  // which would require hacky workarounds in HandleDropAndResetState function
  // to properly detect and handle such cases.

  if (!data_offer_)
    return;

  DVLOG(1) << "OnLeave";
  data_offer_.reset();

  // As wl-shell/xdg-shell clients are only aware of surface-local coordinates
  // and there is no implicit grab during DND sessions, a fake motion event with
  // negative y coordinate is used here to allow higher level UI components to
  // detect when a window should be detached. E.g: On Chrome, dragging a tab all
  // the way up to the top edge of the window won't work without this fake
  // motion event upon wl_data_device::leave events. This is a workaround and
  // should ideally be reworked in the future, at higher level layers such that
  // they properly handle platforms that do not support global screen
  // coordinates, like Wayland.
  //
  // TODO(https://crbug.com/1282186): Find a better solution for upwards tab
  // detaching.
  if (state_ != State::kAttached)
    return;

  if (*drag_source_ == DragSource::kMouse) {
    pointer_delegate_->OnPointerMotionEvent(
        {pointer_location_.x(), -1}, wl::EventDispatchPolicy::kImmediate);
  } else {
    const auto touch_pointer_ids = touch_delegate_->GetActiveTouchPointIds();
    if (!touch_pointer_ids.empty()) {
      // If an user starts dragging a tab horizontally with touch, Chrome enters
      // in "horizontal snapping" mode (see ScrollSnapController for details).
      // Hence, in case of touch driven dragging, use a higher negative dy
      // to work around the threshold in ScrollSnapController otherwise,
      // the drag event is discarded.
      touch_delegate_->OnTouchMotionEvent(
          {pointer_location_.x(), kHorizontalRailExitThreshold},
          base::TimeTicks::Now(), touch_pointer_ids[0],
          wl::EventDispatchPolicy::kImmediate);
    }
  }
}

void WaylandWindowDragController::OnDragDrop() {
  DCHECK_GE(state_, State::kAttached);
  DVLOG(1) << "Dropped. state=" << state_;

  DCHECK(data_offer_);
  data_offer_->FinishOffer();
  data_offer_.reset();

  drag_target_window_ = nullptr;
}

const WaylandWindow* WaylandWindowDragController::GetDragTarget() const {
  return drag_target_window_;
}

// This function is called when either 'cancelled' or 'finished' data source
// events is received during a window dragging session. It is used to detect
// when drop happens, since it is the only event sent by the server regardless
// where it happens, inside or outside toplevel surfaces.
void WaylandWindowDragController::OnDataSourceFinish(bool completed) {
  DCHECK_GE(state_, State::kAttached);
  DCHECK(data_source_);

  DVLOG(1) << "Drop received. state=" << state_;

  // Release DND objects.
  data_offer_.reset();
  data_source_.reset();
  extended_drag_source_.reset();
  origin_surface_.reset();
  origin_window_ = nullptr;

  // When extended-drag is available and the drop happens while a non-null
  // surface was being dragged (i.e: detached mode) which had pointer focus
  // before the drag session, we must reset focus to it, otherwise it would be
  // wrongly kept to the latest surface received through wl_data_device::enter
  // (see OnDragEnter function).
  // In case of touch, though, we simply reset the focus altogether.
  if (IsExtendedDragAvailableInternal() && dragged_window_) {
    if (*drag_source_ == DragSource::kMouse) {
      // TODO: check if this usage is correct.

      pointer_delegate_->OnPointerFocusChanged(
          dragged_window_, pointer_location_,
          wl::EventDispatchPolicy::kImmediate);
    } else {
      touch_delegate_->OnTouchFocusChanged(dragged_window_);
    }
  }
  dragged_window_ = nullptr;

  // Transition to |kDropped| state and determine the next action to take. If
  // drop happened while the move loop was running (i.e: kDetached), ask to quit
  // the loop, otherwise notify session end and reset state right away.
  State state_when_dropped = std::exchange(
      state_, completed || !IsExtendedDragAvailable() ? State::kDropped
                                                      : State::kCancelled);
  if (state_when_dropped == State::kDetached)
    QuitLoop();
  else
    HandleDropAndResetState();

  data_device_->ResetDragDelegate();
  window_manager_->RemoveObserver(this);
}

void WaylandWindowDragController::OnDataSourceSend(const std::string& mime_type,
                                                   std::string* contents) {
  // There is no actual data exchange in DnD window dragging sessions. Window
  // snapping, for example, is supposed to be handled at higher level UI layers.
}

bool WaylandWindowDragController::CanDispatchEvent(const PlatformEvent& event) {
  return state_ == State::kDetached;
}

uint32_t WaylandWindowDragController::DispatchEvent(
    const PlatformEvent& event) {
  DCHECK_EQ(state_, State::kDetached);
  DCHECK(base::CurrentUIThread::IsSet());

  if (event->type() == ET_MOUSE_MOVED || event->type() == ET_MOUSE_DRAGGED ||
      event->type() == ET_TOUCH_MOVED) {
    HandleMotionEvent(event->AsLocatedEvent());
    return POST_DISPATCH_STOP_PROPAGATION;
  }
  return POST_DISPATCH_PERFORM_DEFAULT;
}

void WaylandWindowDragController::OnToplevelWindowCreated(
    WaylandToplevelWindow* window) {
  // Skip unless a toplevel window is getting visible while in attached mode.
  // E.g: A window/tab is being detached in a tab dragging session.
  if (state_ != State::kAttached)
    return;

  DCHECK(window);
  auto origin = window->GetBoundsInDIP().origin();
  gfx::Vector2d offset = gfx::ToFlooredPoint(pointer_location_) - origin;
  DVLOG(1) << "Toplevel window created (detached)."
           << " widget=" << window->GetWidget()
           << " calculated_offset=" << offset.ToString();

  SetDraggedWindow(window, offset);
}

void WaylandWindowDragController::OnWindowRemoved(WaylandWindow* window) {
  DCHECK_NE(state_, State::kIdle);
  DVLOG(1) << "Window being destroyed. widget=" << window->GetWidget();

  if (window == pointer_grab_owner_)
    pointer_grab_owner_ = nullptr;

  if (window == origin_window_)
    origin_surface_ = origin_window_->TakeWaylandSurface();

  if (window == dragged_window_)
    SetDraggedWindow(nullptr, {});
}

void WaylandWindowDragController::HandleMotionEvent(LocatedEvent* event) {
  DCHECK_EQ(state_, State::kDetached);
  DCHECK(event);

  if (!should_process_drag_event_)
    return;

  // Notify listeners about window bounds change (i.e: re-positioning) event.
  // To do so, set the new bounds as per the motion event location and the drag
  // offset. Note that setting a new location (i.e: bounds.origin()) for a
  // surface has no visual effect in ozone/wayland backend. Actual window
  // re-positioning during dragging session is done through the drag icon.
  if (dragged_window_) {
    dragged_window_->SetOrigin(event->location() - drag_offset_);
  }

  should_process_drag_event_ = false;
}

// Dispatch mouse release event (to tell clients that the drop just happened)
// clear focus and reset internal state. Must be called when the session is
// about to finish.
void WaylandWindowDragController::HandleDropAndResetState() {
  DCHECK(state_ == State::kDropped || state_ == State::kCancelled);
  DVLOG(1) << "Notifying drop. window=" << pointer_grab_owner_;

  // StopDragging() may get called in response to bogus input events, eg:
  // wl_pointer.button release, which would imply in multiple calls to this
  // function for a single drop event. That results in ILL_ILLOPN crashes in
  // below code, because |drag_source_| is null after the first call to this
  // function. So, early out here in that case.
  // TODO(crbug.com/1280981): Revert this once Exo-side issue gets solved.
  if (!drag_source_.has_value())
    return;

  if (*drag_source_ == DragSource::kMouse) {
    if (pointer_grab_owner_) {
      pointer_delegate_->OnPointerButtonEvent(
          ET_MOUSE_RELEASED, EF_LEFT_MOUSE_BUTTON, pointer_grab_owner_,
          wl::EventDispatchPolicy::kImmediate);
    }
  } else {
    const auto touch_pointer_ids = touch_delegate_->GetActiveTouchPointIds();
    if (!touch_pointer_ids.empty()) {
      touch_delegate_->OnTouchReleaseEvent(base::TimeTicks::Now(),
                                           touch_pointer_ids[0],
                                           wl::EventDispatchPolicy::kImmediate);
    }
  }

  pointer_grab_owner_ = nullptr;
  state_ = State::kIdle;
  drag_source_.reset();
}

void WaylandWindowDragController::RunLoop() {
  DCHECK_EQ(state_, State::kDetached);
  DCHECK(dragged_window_);

  DVLOG(1) << "Starting drag loop. widget=" << dragged_window_->GetWidget()
           << " offset=" << drag_offset_.ToString();

  auto old_dispatcher = std::move(nested_dispatcher_);
  nested_dispatcher_ =
      PlatformEventSource::GetInstance()->OverrideDispatcher(this);

  base::WeakPtr<WaylandWindowDragController> alive(weak_factory_.GetWeakPtr());

  base::RunLoop loop(base::RunLoop::Type::kNestableTasksAllowed);
  quit_loop_closure_ = loop.QuitClosure();
  loop.Run();

  if (!alive)
    return;

  nested_dispatcher_ = std::move(old_dispatcher);

  DVLOG(1) << "Quitting drag loop " << state_;
}

void WaylandWindowDragController::QuitLoop() {
  DCHECK(!quit_loop_closure_.is_null());

  nested_dispatcher_.reset();
  std::move(quit_loop_closure_).Run();
}

void WaylandWindowDragController::SetDraggedWindow(
    WaylandToplevelWindow* window,
    const gfx::Vector2d& offset) {
  if (dragged_window_ == window && offset == drag_offset_)
    return;

  dragged_window_ = window;
  drag_offset_ = offset;

  // TODO(crbug.com/896640): Fallback when extended-drag is not available.
  if (extended_drag_source_)
    extended_drag_source_->SetDraggedWindow(dragged_window_, drag_offset_);
}

bool WaylandWindowDragController::IsExtendedDragAvailable() const {
  return extended_drag_available_for_testing_ ||
         IsExtendedDragAvailableInternal();
}

bool WaylandWindowDragController::IsExtendedDragAvailableInternal() const {
  return !!connection_->extended_drag_v1();
}

std::ostream& operator<<(std::ostream& out,
                         WaylandWindowDragController::State state) {
  return out << static_cast<int>(state);
}

std::pair<absl::optional<wl::Serial>, WaylandWindow*>
WaylandWindowDragController::GetSerialAndOrigin() {
  std::pair<absl::optional<wl::Serial>, WaylandWindow*> result{};
  for (auto type : {wl::SerialType::kTouchPress, wl::SerialType::kMousePress}) {
    auto serial = connection_->serial_tracker().GetSerial(type);
    auto* window = type == wl::SerialType::kTouchPress
                       ? window_manager_->GetCurrentTouchFocusedWindow()
                       : window_manager_->GetCurrentPointerFocusedWindow();
    if (window && serial &&
        (!result.first || serial->timestamp > result.first->timestamp)) {
      result = {serial, window};
    }
  }
  return result;
}

}  // namespace ui
