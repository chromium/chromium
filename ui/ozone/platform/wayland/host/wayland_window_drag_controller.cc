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

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/scoped_event_dispatcher.h"
#include "ui/events/platform_event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/dump_util.h"
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

using mojom::DragEventSource;

// Custom mime type used for window dragging DND sessions.
constexpr char kMimeTypeChromiumWindow[] = "chromium/x-window";

// DND action used in window dragging DND sessions.
constexpr uint32_t kDndActionWindowDrag =
    WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;

// Value minimally higher than `GestureDetector::Config::touch_slop` (see [1])
// to exit the horizontal rail threshold in SnapScrollController, in case of
// an upwards tab dragging detach with touch.
//
// [1] //ui/events/gesture_detection/gesture_detector.h
constexpr int kHorizontalRailExitThreshold = -10;

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

bool WaylandWindowDragController::StartDragSession(
    WaylandToplevelWindow* origin,
    DragEventSource drag_source) {
  if (state_ != State::kIdle)
    return true;

  auto serial = GetSerial(drag_source, origin);
  if (!serial) {
    LOG(ERROR) << "Failed to retrieve dnd serial. origin=" << origin
               << " drag_source=" << drag_source;
    return false;
  }

  VLOG(1) << "Starting DND session.";
  state_ = State::kAttached;
  origin_window_ = origin;
  drag_source_ = drag_source;

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
  should_process_drag_motion_events_ = false;

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

  VLOG(1) << "End drag loop requested. state=" << state_;

  // This function is supposed to be called to indicate that the window was just
  // snapped into a tab strip. So switch to |kAttached| state, store the focused
  // window as the pointer grabber and ask to quit the nested loop.
  state_ = State::kAttaching;
  pointer_grab_owner_ =
      window_manager_->GetCurrentPointerOrTouchFocusedWindow();
  VLOG(1) << "Quiting Loop : StopDragging";
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
  // Drag-and-drop sessions may be terminated by the client while drag-and-drop
  // server events are still in-flight. No-op if this is the case.
  if (!IsActiveDragAndDropSession()) {
    return;
  }

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
  if (*drag_source_ == DragEventSource::kMouse) {
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
  // Drag-and-drop sessions may be terminated by the client while drag-and-drop
  // server events are still in-flight. No-op if this is the case.
  if (!IsActiveDragAndDropSession()) {
    return;
  }

  DCHECK_GE(state_, State::kAttached);
  DVLOG(2) << "OnMotion. location=" << location.ToString();

  // Motion events are not expected to be dispatched while waiting for the drag
  // loop to exit, ie: kAttaching transitional state. See crbug.com/1169446.
  if (state_ == State::kAttaching)
    return;

  // Forward cursor location update info to the input handling delegate.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  should_process_drag_motion_events_ =
      !(static_cast<WaylandToplevelWindow*>(drag_target_window_)
            ->IsScreenCoordinatesEnabled());
#else
  // non-lacros platforms never use global coordinates so they always process
  // drag events.
  should_process_drag_motion_events_ = true;
#endif

  pointer_location_ = location;

  if (*drag_source_ == DragEventSource::kMouse) {
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
  // Drag and drop sessions may be terminated by the client while drag-and-drop
  // server events may still be in-flight. No-op if this is the case.
  if (!IsActiveDragAndDropSession()) {
    return;
  }

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

  if (*drag_source_ == DragEventSource::kMouse) {
    pointer_delegate_->OnPointerMotionEvent(
        {pointer_location_.x(), -1}, wl::EventDispatchPolicy::kImmediate);
  } else {
    const auto touch_pointer_ids = touch_delegate_->GetActiveTouchPointIds();
    if (!touch_pointer_ids.empty()) {
      // If an user starts dragging a tab horizontally with touch, Chrome enters
      // in "horizontal snapping" mode (see SnapScrollController for details).
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
  // Drag-and-drop sessions may be terminated by the client while drag-and-drop
  // server events are still in-flight. No-op if this is the case.
  if (!IsActiveDragAndDropSession()) {
    return;
  }

  DCHECK_GE(state_, State::kAttached);
  VLOG(1) << "Dropped. state=" << state_;

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

  VLOG(1) << "DataSourceFinish received. completed=" << completed
          << ", state=" << state_;
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
    if (*drag_source_ == DragEventSource::kMouse) {
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
  if (state_when_dropped == State::kDetached) {
    VLOG(1) << "Quiting Loop : Detached";
    QuitLoop();
  } else {
    HandleDropAndResetState();
  }

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

  // The drag should only be cancelled after all window pointers have had a
  // chance invalidate. This is necessary as a single aura::Window can serve
  // multiple roles (e.g target window can also be the origin window). Cancel
  // The drag if either `drag_target_window_` or `dragged_window_` have been
  // removed.
  bool should_cancel_drag = false;

  if (window == drag_target_window_) {
    drag_target_window_ = nullptr;
    should_cancel_drag = true;
  }

  if (window == pointer_grab_owner_) {
    pointer_grab_owner_ = nullptr;
  }

  if (window == origin_window_) {
    origin_surface_ = origin_window_->TakeWaylandSurface();
    origin_window_ = nullptr;
  }

  if (window == dragged_window_) {
    SetDraggedWindow(nullptr, {});
    should_cancel_drag = true;
  }

  if (should_cancel_drag) {
    LOG(ERROR) << "OnDataSourceFinish";
    OnDataSourceFinish(/*completed=*/false);
  }
}

void WaylandWindowDragController::HandleMotionEvent(LocatedEvent* event) {
  DCHECK_EQ(state_, State::kDetached);
  DCHECK(event);

  if (!should_process_drag_motion_events_) {
    return;
  }

  // Notify listeners about window bounds change (i.e: re-positioning) event.
  // To do so, set the new bounds as per the motion event location and the drag
  // offset. Note that setting a new location (i.e: bounds.origin()) for a
  // surface has no visual effect in ozone/wayland backend. Actual window
  // re-positioning during dragging session is done through the drag icon.
  if (dragged_window_) {
    dragged_window_->SetOrigin(event->location() - drag_offset_);
  }

  should_process_drag_motion_events_ = false;
}

// Dispatch mouse release event (to tell clients that the drop just happened)
// clear focus and reset internal state. Must be called when the session is
// about to finish.
void WaylandWindowDragController::HandleDropAndResetState() {
  DCHECK(state_ == State::kDropped || state_ == State::kCancelled);
  VLOG(1) << "Notifying drop. window=" << pointer_grab_owner_;

  // StopDragging() may get called in response to bogus input events, eg:
  // wl_pointer.button release, which would imply in multiple calls to this
  // function for a single drop event. That results in ILL_ILLOPN crashes in
  // below code, because |drag_source_| is null after the first call to this
  // function. So, early out here in that case.
  // TODO(crbug.com/1280981): Revert this once Exo-side issue gets solved.
  if (!drag_source_.has_value())
    return;

  if (*drag_source_ == DragEventSource::kMouse) {
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
  auto old_dispatcher = std::move(nested_dispatcher_);

  VLOG(1) << "Starting drag loop. widget=" << dragged_window_->GetWidget()
          << " offset=" << drag_offset_.ToString()
          << ", has old dispatcher=" << !!old_dispatcher;

  nested_dispatcher_ =
      PlatformEventSource::GetInstance()->OverrideDispatcher(this);

  base::WeakPtr<WaylandWindowDragController> alive(weak_factory_.GetWeakPtr());

  base::RunLoop loop(base::RunLoop::Type::kNestableTasksAllowed);
  quit_loop_closure_ = loop.QuitClosure();
  loop.Run();

  if (!alive) {
    VLOG(1) << "Exited drag loop: destroyed";
    return;
  }

  nested_dispatcher_ = std::move(old_dispatcher);

  VLOG(1) << "Exited drag loop: state=" << state_;
}

void WaylandWindowDragController::QuitLoop() {
  DCHECK(!quit_loop_closure_.is_null());
  VLOG(1) << "Quit Loop: resetting nested dispatcher=" << !!nested_dispatcher_;
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

bool WaylandWindowDragController::IsActiveDragAndDropSession() const {
  return !!data_source_;
}

void WaylandWindowDragController::DumpState(std::ostream& out) const {
  constexpr auto kStateToString = base::MakeFixedFlatMap<State, const char*>(
      {{State::kIdle, "idle"},
       {State::kAttached, "attached"},
       {State::kDetached, "detached"},
       {State::kDropped, "dropped"},
       {State::kCancelled, "canceled"},
       {State::kAttaching, "attaching"}});

  out << "WaylandWindowDragController:"
      << " state=" << GetMapValueOrDefault(kStateToString, state_)
      << ", drag_offset=" << drag_offset_.ToString()
      << ", pointer_position=" << pointer_location_.ToString()
      << ", data_source=" << !!data_source_
      << ", dragged_window=" << GetWindowName(dragged_window_.get())
      << ", pointer_grab_owner=" << GetWindowName(pointer_grab_owner_.get())
      << ", origin_window=" << GetWindowName(origin_window_.get())
      << ", drag_target_window=" << GetWindowName(drag_target_window_.get())
      << ", nested_dispatcher=" << !!nested_dispatcher_;
}

bool WaylandWindowDragController::IsExtendedDragAvailableInternal() const {
  return !!connection_->extended_drag_v1();
}

std::ostream& operator<<(std::ostream& out,
                         WaylandWindowDragController::State state) {
  return out << static_cast<int>(state);
}

absl::optional<wl::Serial> WaylandWindowDragController::GetSerial(
    DragEventSource drag_source,
    WaylandToplevelWindow* origin) {
  auto* focused = drag_source == DragEventSource::kMouse
                      ? window_manager_->GetCurrentPointerFocusedWindow()
                      : window_manager_->GetCurrentTouchFocusedWindow();
  if (!origin || focused != origin) {
    return absl::nullopt;
  }
  return connection_->serial_tracker().GetSerial(
      drag_source == DragEventSource::kMouse ? wl::SerialType::kMousePress
                                             : wl::SerialType::kTouchPress);
}

}  // namespace ui
