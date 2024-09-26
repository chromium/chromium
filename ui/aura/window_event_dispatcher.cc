// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_event_dispatcher.h"

#include <stddef.h>

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/metrics/custom_metrics_recorder.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/event_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/env_input_state_controller.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher_observer.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tracker.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/base/ime/input_method.h"
#include "ui/compositor/compositor.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/geometry/transform.h"

typedef ui::EventDispatchDetails DispatchDetails;

namespace aura {

namespace {

// Returns true if |target| has a non-client (frame) component at |location|,
// in window coordinates.
bool IsNonClientLocation(Window* target, const gfx::Point& location) {
  if (!target->delegate())
    return false;
  int hit_test_code = target->delegate()->GetNonClientComponent(location);
  return hit_test_code != HTCLIENT && hit_test_code != HTNOWHERE;
}

Window* ConsumerToWindow(ui::GestureConsumer* consumer) {
  return consumer ? static_cast<Window*>(consumer) : nullptr;
}

bool IsEventCandidateForHold(const ui::Event& event) {
  if (event.type() == ui::EventType::kTouchMoved) {
    return true;
  }
  if (event.type() == ui::EventType::kMouseDragged) {
    return true;
  }
  if (event.type() == ui::EventType::kMouseExited) {
    return false;
  }
  if (event.IsMouseEvent() && (event.flags() & ui::EF_IS_SYNTHESIZED))
    return true;
  return false;
}

}  // namespace

WindowEventDispatcher::ObserverNotifier::ObserverNotifier(
    WindowEventDispatcher* dispatcher,
    const ui::Event& event)
    : dispatcher_(dispatcher) {
  Env::GetInstance()->window_event_dispatcher_observers().Notify(
      &WindowEventDispatcherObserver::OnWindowEventDispatcherStartedProcessing,
      dispatcher, event);
}

WindowEventDispatcher::ObserverNotifier::~ObserverNotifier() {
  Env::GetInstance()->window_event_dispatcher_observers().Notify(
      &WindowEventDispatcherObserver::
          OnWindowEventDispatcherFinishedProcessingEvent,
      dispatcher_);
}

////////////////////////////////////////////////////////////////////////////////
// WindowEventDispatcher, public:

WindowEventDispatcher::WindowEventDispatcher(WindowTreeHost* host)
    : host_(host), event_targeter_(std::make_unique<WindowTargeter>()) {
  Env::GetInstance()->gesture_recognizer()->AddGestureEventHelper(this);
  Env::GetInstance()->AddObserver(this);
}

WindowEventDispatcher::~WindowEventDispatcher() {
  TRACE_EVENT0("shutdown", "WindowEventDispatcher::Destructor");
  Env::GetInstance()->gesture_recognizer()->RemoveGestureEventHelper(this);
  Env::GetInstance()->RemoveObserver(this);
}

void WindowEventDispatcher::Shutdown() {
  in_shutdown_ = true;
}

ui::EventTargeter* WindowEventDispatcher::GetDefaultEventTargeter() {
  return event_targeter_.get();
}

void WindowEventDispatcher::RepostEvent(const ui::LocatedEvent* event) {
  DCHECK(event->type() == ui::EventType::kMousePressed ||
         event->type() == ui::EventType::kGestureTapDown ||
         event->type() == ui::EventType::kTouchPressed);
  // We allow for only one outstanding repostable event. This is used
  // in exiting context menus.  A dropped repost request is allowed.
  if (event->type() == ui::EventType::kMousePressed) {
    held_repostable_event_ = std::make_unique<ui::MouseEvent>(
        *event->AsMouseEvent(), static_cast<aura::Window*>(event->target()),
        window());
  } else if (event->type() == ui::EventType::kTouchPressed) {
    held_repostable_event_ =
        std::make_unique<ui::TouchEvent>(*event->AsTouchEvent());
  } else {
    DCHECK(event->type() == ui::EventType::kGestureTapDown);
    held_repostable_event_.reset();
    // TODO(rbyers): Reposing of gestures is tricky to get
    // right, so it's not yet supported.  crbug.com/170987.
  }

  if (held_repostable_event_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostNonNestableTask(
        FROM_HERE,
        base::BindOnce(
            base::IgnoreResult(&WindowEventDispatcher::DispatchHeldEvents),
            repost_event_factory_.GetWeakPtr()));
  }
}

void WindowEventDispatcher::OnMouseEventsEnableStateChanged(bool enabled) {
  // Send entered / exited so that visual state can be updated to match
  // mouse events state.
  PostSynthesizeMouseMove(window());
  // TODO(mazda): Add code to disable mouse events when |enabled| == false.
}

void WindowEventDispatcher::DispatchCancelModeEvent() {
  ui::CancelModeEvent event;
  Window* focused_window = client::GetFocusClient(window())->GetFocusedWindow();
  if (focused_window && !window()->Contains(focused_window))
    focused_window = nullptr;
  DispatchDetails details =
      DispatchEvent(focused_window ? focused_window : window(), &event);
  if (details.dispatcher_destroyed)
    return;
}

void WindowEventDispatcher::DispatchGestureEvent(
    ui::GestureConsumer* raw_input_consumer,
    ui::GestureEvent* event) {
  DispatchDetails details = DispatchHeldEvents();
  if (details.dispatcher_destroyed)
    return;
  Window* target = ConsumerToWindow(raw_input_consumer);
  if (target) {
    event->ConvertLocationToTarget(window(), target);
    details = DispatchEvent(target, event);
    if (details.dispatcher_destroyed)
      return;
  }
}

DispatchDetails WindowEventDispatcher::DispatchMouseExitAtPoint(
    Window* window,
    const gfx::Point& point,
    int event_flags) {
  ui::MouseEvent event(ui::EventType::kMouseExited, point, point,
                       ui::EventTimeForNow(), event_flags, ui::EF_NONE);
  return DispatchMouseEnterOrExit(window, event, ui::EventType::kMouseExited);
}

void WindowEventDispatcher::ProcessedTouchEvent(
    uint32_t unique_event_id,
    Window* window,
    ui::EventResult result,
    bool is_source_touch_event_set_blocking) {
  ui::GestureRecognizer::Gestures gestures =
      Env::GetInstance()->gesture_recognizer()->AckTouchEvent(
          unique_event_id, result, is_source_touch_event_set_blocking, window);
  DispatchDetails details = ProcessGestures(window, std::move(gestures));
  if (details.dispatcher_destroyed)
    return;
}

void WindowEventDispatcher::HoldPointerMoves() {
  if (!move_hold_count_) {
    // |synthesize_mouse_events_| is explicitly not changed. It is handled and
    // reset in ReleasePointerMoves.
    held_event_factory_.InvalidateWeakPtrs();
  }
  ++move_hold_count_;
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      "ui", "WindowEventDispatcher::HoldPointerMoves", TRACE_ID_LOCAL(this));
}

void WindowEventDispatcher::ReleasePointerMoves() {
  --move_hold_count_;
  DCHECK_GE(move_hold_count_, 0);
  if (!move_hold_count_) {
    // HoldPointerMoves cancels the pending synthesized mouse move if any.
    // So ReleasePointerMoves should ensure that |synthesize_mouse_move_|
    // resets. Otherwise, PostSynthesizeMouseMove is blocked indefintely.
    const bool pending_synthesize_mouse_move = synthesize_mouse_move_;
    synthesize_mouse_move_ = false;
    if (held_move_event_) {
      // We don't want to call DispatchHeldEvents directly, because this might
      // be called from a deep stack while another event, in which case
      // dispatching another one may not be safe/expected.  Instead we post a
      // task, that we may cancel if HoldPointerMoves is called again before it
      // executes.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostNonNestableTask(
          FROM_HERE,
          base::BindOnce(
              base::IgnoreResult(&WindowEventDispatcher::DispatchHeldEvents),
              held_event_factory_.GetWeakPtr()));
    } else {
      if (did_dispatch_held_move_event_callback_)
        std::move(did_dispatch_held_move_event_callback_).Run();
      if (pending_synthesize_mouse_move) {
        // Schedule a synthesized mouse move event when there is no held mouse
        // move and we should generate one.
        PostSynthesizeMouseMove(window());
      }
    }
  }
  TRACE_EVENT_NESTABLE_ASYNC_END0(
      "ui", "WindowEventDispatcher::HoldPointerMoves", TRACE_ID_LOCAL(this));
}

gfx::Point WindowEventDispatcher::GetLastMouseLocationInRoot() const {
  gfx::Point location = Env::GetInstance()->last_mouse_location();
  ConvertPointFromScreen(&location);
  return location;
}

void WindowEventDispatcher::OnHostLostMouseGrab() {
  mouse_pressed_handler_ = nullptr;
  mouse_moved_handler_ = nullptr;
}

void WindowEventDispatcher::OnCursorMovedToRootLocation(
    const gfx::Point& root_location) {
  Env::GetInstance()->env_controller()->SetLastMouseLocation(window(),
                                                             root_location);

  // Synthesize a mouse move in case the cursor's location in root coordinates
  // changed but its position in WindowTreeHost coordinates did not.
  PostSynthesizeMouseMove(window());
}

void WindowEventDispatcher::OnPostNotifiedWindowDestroying(Window* window) {
  OnWindowHidden(window, WINDOW_DESTROYED);
}

////////////////////////////////////////////////////////////////////////////////
// WindowEventDispatcher, private:

Window* WindowEventDispatcher::window() {
  return host_->window();
}

const Window* WindowEventDispatcher::window() const {
  return host_->window();
}

void WindowEventDispatcher::ConvertPointFromScreen(gfx::Point* point) const {
  client::ScreenPositionClient* client =
      client::GetScreenPositionClient(window());
  if (client)
    client->ConvertPointFromScreen(window(), point);
}

void WindowEventDispatcher::TransformEventForDeviceScaleFactor(
    ui::LocatedEvent* event) {
  event->UpdateForRootTransform(
      host_->GetInverseRootTransform(),
      host_->GetInverseRootTransformForLocalEventCoordinates());
}

void WindowEventDispatcher::DispatchMouseExitToHidingWindow(Window* window) {
  // Dispatching events during shutdown can cause crashes (e.g. in Chrome OS
  // system tray cleanup). https://crbug.com/874156
  if (in_shutdown_)
    return;

  // The mouse capture is intentionally ignored. Think that a mouse enters
  // to a window, the window sets the capture, the mouse exits the window,
  // and then it releases the capture. In that case OnMouseExited won't
  // be called. So it is natural not to emit OnMouseExited even though
  // |window| is the capture window.
  gfx::Point last_mouse_location = GetLastMouseLocationInRoot();
  if (window->Contains(mouse_moved_handler_) &&
      window->ContainsPointInRoot(last_mouse_location)) {
    DispatchDetails details =
        DispatchMouseExitAtPoint(this->window(), last_mouse_location);
    if (details.dispatcher_destroyed)
      return;
  }
}

ui::EventDispatchDetails WindowEventDispatcher::DispatchMouseEnterOrExit(
    Window* target,
    const ui::MouseEvent& event,
    ui::EventType type) {
  Env::GetInstance()->env_controller()->UpdateStateForMouseEvent(window(),
                                                                 event);
  if (!mouse_moved_handler_ || !mouse_moved_handler_->HasTargetHandler() ||
      !window()->Contains(mouse_moved_handler_))
    return DispatchDetails();

  // |event| may be an event in the process of being dispatched to a target (in
  // which case its locations will be in the event's target's coordinate
  // system), or a synthetic event created in root-window (in which case, the
  // event's target will be NULL, and the event will be in the root-window's
  // coordinate system.
  if (!target)
    target = window();
  ui::MouseEvent translated_event(event, target, mouse_moved_handler_.get(),
                                  type, event.flags() | ui::EF_IS_SYNTHESIZED);
  return DispatchEvent(mouse_moved_handler_, &translated_event);
}

ui::EventDispatchDetails WindowEventDispatcher::ProcessGestures(
    Window* target,
    ui::GestureRecognizer::Gestures gestures) {
  DispatchDetails details;
  if (gestures.empty())
    return details;

  // If a window has been hidden between the touch event and now, the associated
  // gestures may not have a valid target.
  if (!target)
    return details;

  for (const auto& event : gestures) {
    event->ConvertLocationToTarget(window(), target);
    details = DispatchEvent(target, event.get());
    if (details.dispatcher_destroyed || details.target_destroyed)
      break;
  }
  return details;
}

void WindowEventDispatcher::OnWindowHidden(Window* invisible,
                                           WindowHiddenReason reason) {
  // If the window the mouse was pressed in becomes invisible, it should no
  // longer receive mouse events.
  if (invisible->Contains(mouse_pressed_handler_))
    mouse_pressed_handler_ = nullptr;
  if (invisible->Contains(mouse_moved_handler_))
    mouse_moved_handler_ = nullptr;
  if (invisible->Contains(touchpad_pinch_handler_))
    touchpad_pinch_handler_ = nullptr;

  // If events are being dispatched from a nested message-loop, and the target
  // of the outer loop is hidden or moved to another dispatcher during
  // dispatching events in the inner loop, then reset the target for the outer
  // loop.
  if (invisible->Contains(old_dispatch_target_))
    old_dispatch_target_ = nullptr;

  // Cleaning up gesture state may end up destroying the hidden window. We use a
  // tracker to detect this.
  WindowTracker invisible_tracker({invisible});
  invisible->CleanupGestureState();

  // Do not clear the capture, and the |event_dispatch_target_| if the
  // window is moving across hosts, because the target itself is actually still
  // visible and clearing them stops further event processing, which can cause
  // unexpected behaviors. See crbug.com/157583
  if (reason != WINDOW_MOVING) {
    // We don't ask |invisible| here, because invisible may have been removed
    // from the window hierarchy already by the time this function is called
    // (OnWindowDestroyed).
    client::CaptureClient* capture_client =
        client::GetCaptureClient(host_->window());
    Window* capture_window =
        capture_client ? capture_client->GetCaptureWindow() : nullptr;

    if (!invisible_tracker.Contains(invisible) ||
        invisible->Contains(event_dispatch_target_)) {
      event_dispatch_target_ = nullptr;
    }

    // If the ancestor of the capture window is hidden, release the capture.
    // Note that this may delete the window so do not use capture_window
    // after this.
    if (invisible_tracker.Contains(invisible) &&
        invisible->Contains(capture_window) && invisible != window()) {
      capture_window->ReleaseCapture();
    }
  }
}

bool WindowEventDispatcher::is_dispatched_held_event(
    const ui::Event& event) const {
  return dispatching_held_event_ == &event;
}

////////////////////////////////////////////////////////////////////////////////
// WindowEventDispatcher, aura::client::CaptureDelegate implementation:

void WindowEventDispatcher::UpdateCapture(Window* old_capture,
                                          Window* new_capture) {
  // |mouse_moved_handler_| may have been set to a Window in a different root
  // (see below). Clear it here to ensure we don't end up referencing a stale
  // Window.
  if (mouse_moved_handler_ && !window()->Contains(mouse_moved_handler_))
    mouse_moved_handler_ = nullptr;

  if (old_capture && old_capture->GetRootWindow() == window() &&
      old_capture->delegate()) {
    // Send a capture changed event with the most recent mouse screen location.
    const gfx::Point location = Env::GetInstance()->last_mouse_location();
    ui::MouseEvent event(ui::EventType::kMouseCaptureChanged, location,
                         location, ui::EventTimeForNow(), 0, 0);

    DispatchDetails details = DispatchEvent(old_capture, &event);
    if (details.dispatcher_destroyed)
      return;

    if (!details.target_destroyed)
      old_capture->delegate()->OnCaptureLost();
  }

  if (new_capture) {
    // Make all subsequent mouse events go to the capture window. We shouldn't
    // need to send an event here as OnCaptureLost() should take care of that.
    if (mouse_moved_handler_ || Env::GetInstance()->IsMouseButtonDown())
      mouse_moved_handler_ = new_capture;
  } else {
    // Make sure mouse_moved_handler gets updated.
    DispatchDetails details = SynthesizeMouseMoveEvent();
    if (details.dispatcher_destroyed)
      return;
  }
  mouse_pressed_handler_ = nullptr;
}

void WindowEventDispatcher::OnOtherRootGotCapture() {
  // Windows provides the TrackMouseEvents API which allows us to rely on the
  // OS to send us the mouse exit events (WM_MOUSELEAVE). Additionally on
  // desktop Windows, every top level window could potentially have its own
  // root window, in which case this function will get called whenever those
  // windows grab mouse capture. Sending mouse exit messages in these cases
  // causes subtle bugs like (crbug.com/394672).
#if !BUILDFLAG(IS_WIN)
  if (mouse_moved_handler_) {
    // Dispatch a mouse exit to reset any state associated with hover. This is
    // important when going from no window having capture to a window having
    // capture because we do not dispatch EventType::kMouseCaptureChanged in
    // this case.
    DispatchDetails details =
        DispatchMouseExitAtPoint(nullptr, GetLastMouseLocationInRoot());
    if (details.dispatcher_destroyed)
      return;
  }
#endif

  mouse_moved_handler_ = nullptr;
  mouse_pressed_handler_ = nullptr;
}

void WindowEventDispatcher::SetNativeCapture() {
  host_->SetCapture();
}

void WindowEventDispatcher::ReleaseNativeCapture() {
  host_->ReleaseCapture();
}

////////////////////////////////////////////////////////////////////////////////
// WindowEventDispatcher, ui::EventProcessor implementation:

ui::EventTarget* WindowEventDispatcher::GetRootForEvent(ui::Event* event) {
  return window();
}

void WindowEventDispatcher::OnEventProcessingStarted(ui::Event* event) {
  // Don't dispatch events during shutdown.
  if (in_shutdown_) {
    event->SetHandled();
    return;
  }

  // The held events are already in |window()|'s coordinate system. So it is
  // not necessary to apply the transform to convert from the host's
  // coordinate system to |window()|'s coordinate system.
  if (event->IsLocatedEvent() && !is_dispatched_held_event(*event))
    TransformEventForDeviceScaleFactor(static_cast<ui::LocatedEvent*>(event));

  observer_notifiers_.push(std::make_unique<ObserverNotifier>(this, *event));
}

void WindowEventDispatcher::OnEventProcessingFinished(ui::Event* event) {
  if (in_shutdown_)
    return;

  observer_notifiers_.pop();
}

////////////////////////////////////////////////////////////////////////////////
// WindowEventDispatcher, ui::EventDispatcherDelegate implementation:

bool WindowEventDispatcher::CanDispatchToTarget(ui::EventTarget* target) {
  return event_dispatch_target_ == target;
}

ui::EventDispatchDetails WindowEventDispatcher::PreDispatchEvent(
    ui::EventTarget* target,
    ui::Event* event) {
  if (host_->compositor() && cc::CustomMetricRecorder::Get()) {
    // Must destroy existing monitor before creating the new one since the
    // monitors are expected to be added and removed in stack order (LIFO).
    event_metrics_monitor_.reset();
    event_metrics_monitor_ = CreateScropedMetricsMonitorForEvent(*event);
  }

  Window* target_window = static_cast<Window*>(target);
  CHECK(window()->Contains(target_window));

  WindowTracker target_window_tracker;
  target_window_tracker.Add(target_window);
  if (!dispatching_held_event_) {
    bool can_be_held = IsEventCandidateForHold(*event);
    if (!move_hold_count_ || !can_be_held) {
      if (can_be_held)
        held_move_event_.reset();
      DispatchDetails details = DispatchHeldEvents();
      if (details.dispatcher_destroyed || details.target_destroyed)
        return details;
    }
  }
  if (target_window_tracker.windows().empty()) {
    // The event target is destroyed while processing the held event.
    DispatchDetails details;
    details.target_destroyed = true;
    return details;
  }

  DispatchDetails details;
  if (event->IsMouseEvent()) {
    details = PreDispatchMouseEvent(target_window, event->AsMouseEvent());
  } else if (event->IsScrollEvent()) {
    details = PreDispatchLocatedEvent(target_window, event->AsScrollEvent());
  } else if (event->IsTouchEvent()) {
    details = PreDispatchTouchEvent(target_window, event->AsTouchEvent());
  } else if (event->IsKeyEvent()) {
    details = PreDispatchKeyEvent(target_window, event->AsKeyEvent());
  } else if (event->IsPinchEvent()) {
    details = PreDispatchPinchEvent(target_window, event->AsGestureEvent());
  }
  if (details.dispatcher_destroyed || details.target_destroyed)
    return details;

  old_dispatch_target_ = event_dispatch_target_;
  event_dispatch_target_ = target_window;
  return DispatchDetails();
}

ui::EventDispatchDetails WindowEventDispatcher::PostDispatchEvent(
    ui::EventTarget* target,
    const ui::Event& event) {
  DispatchDetails details;
  if (!target || target != event_dispatch_target_)
    details.target_destroyed = true;
  event_dispatch_target_ = old_dispatch_target_;
  old_dispatch_target_ = nullptr;
#ifndef NDEBUG
  DCHECK(!event_dispatch_target_ || window()->Contains(event_dispatch_target_));
#endif

  if (event.IsTouchEvent() && !details.target_destroyed) {
    // Do not let 'held' touch events contribute to any gestures unless it is
    // being dispatched.
    if (is_dispatched_held_event(event) || !held_move_event_ ||
        !held_move_event_->IsTouchEvent()) {
      const ui::TouchEvent& touchevent = *event.AsTouchEvent();

      if (!touchevent.synchronous_handling_disabled()) {
        Window* window = static_cast<Window*>(target);
        auto event_result = touchevent.force_process_gesture()
                                ? ui::ER_UNHANDLED
                                : event.result();
        ui::GestureRecognizer::Gestures gestures =
            Env::GetInstance()->gesture_recognizer()->AckTouchEvent(
                touchevent.unique_event_id(), event_result,
                false /* is_source_touch_event_set_blocking */, window);

        details = ProcessGestures(window, std::move(gestures));
      }
    }
  }

  // Note this must run after processing events corresponding to the event
  // monitor creation code in PreDispatchEvent to track latencies properly.
  if (!details.dispatcher_destroyed && host_->compositor() &&
      cc::CustomMetricRecorder::Get()) {
    event_metrics_monitor_.reset();
  }

  return details;
}

////////////////////////////////////////////////////////////////////////////////
// WindowEventDispatcher, ui::GestureEventHelper implementation:

bool WindowEventDispatcher::CanDispatchToConsumer(
    ui::GestureConsumer* consumer) {
  Window* consumer_window = ConsumerToWindow(consumer);
  return (consumer_window && consumer_window->GetRootWindow() == window());
}

void WindowEventDispatcher::DispatchSyntheticTouchEvent(ui::TouchEvent* event) {
  // The synthetic event's location is based on the last known location of
  // the pointer, in dips. OnEventFromSource expects events with co-ordinates
  // in raw pixels, so we convert back to raw pixels here.
  DCHECK(event->type() == ui::EventType::kTouchCancelled ||
         event->type() == ui::EventType::kTouchPressed);
  event->UpdateForRootTransform(
      host_->GetRootTransform(),
      host_->GetRootTransformForLocalEventCoordinates());
  DispatchDetails details = OnEventFromSource(event);
  if (details.dispatcher_destroyed)
    return;
}

////////////////////////////////////////////////////////////////////////////////
// WindowEventDispatcher, WindowObserver implementation:

void WindowEventDispatcher::OnWindowDestroying(Window* window) {
  if (!host_->window()->Contains(window))
    return;

  SynthesizeMouseMoveAfterChangeToWindow(window);
}

void WindowEventDispatcher::OnWindowDestroyed(Window* window) {
  // We observe all windows regardless of what root Window (if any) they're
  // attached to.
  observation_manager_.RemoveObservation(window);

  // In theory this should be cleaned up by other checks, but we are getting
  // crashes that seem to indicate otherwise. See https://crbug.com/942552 for
  // one case.
  if (window == mouse_moved_handler_)
    mouse_moved_handler_ = nullptr;
}

void WindowEventDispatcher::OnWindowAddedToRootWindow(Window* attached) {
  if (!observation_manager_.IsObservingSource(attached))
    observation_manager_.AddObservation(attached);

  if (!host_->window()->Contains(attached))
    return;

  SynthesizeMouseMoveAfterChangeToWindow(attached);
}

void WindowEventDispatcher::OnWindowRemovingFromRootWindow(Window* detached,
                                                           Window* new_root) {
  if (!host_->window()->Contains(detached))
    return;

  DCHECK(client::GetCaptureWindow(window()) != window());

  DispatchMouseExitToHidingWindow(detached);
  SynthesizeMouseMoveAfterChangeToWindow(detached);

  // Hiding the window releases capture which can implicitly destroy the window
  // so the window may no longer be valid after this call.
  OnWindowHidden(detached, new_root ? WINDOW_MOVING : WINDOW_HIDDEN);
}

void WindowEventDispatcher::OnWindowVisibilityChanging(Window* window,
                                                       bool visible) {
  if (!host_->window()->Contains(window))
    return;

  DispatchMouseExitToHidingWindow(window);
}

void WindowEventDispatcher::OnWindowVisibilityChanged(Window* window,
                                                      bool visible) {
  if (!host_->window()->Contains(window))
    return;

  if (window->ContainsPointInRoot(GetLastMouseLocationInRoot()))
    PostSynthesizeMouseMove(window);

  // Hiding the window releases capture which can implicitly destroy the window
  // so the window may no longer be valid after this call.
  if (!visible)
    OnWindowHidden(window, WINDOW_HIDDEN);
}

void WindowEventDispatcher::OnWindowBoundsChanged(
    Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  if (!host_->window()->Contains(window))
    return;

  if (window == host_->window()) {
    TRACE_EVENT1("ui", "WindowEventDispatcher::OnWindowBoundsChanged(root)",
                 "size", new_bounds.size().ToString());

    DispatchDetails details = DispatchHeldEvents();
    if (details.dispatcher_destroyed)
      return;

    synthesize_mouse_move_ = false;
  }

  if (window->IsVisible() &&
      window->event_targeting_policy() != EventTargetingPolicy::kNone) {
    gfx::Rect old_bounds_in_root = old_bounds, new_bounds_in_root = new_bounds;
    Window::ConvertRectToTarget(window->parent(), host_->window(),
                                &old_bounds_in_root);
    Window::ConvertRectToTarget(window->parent(), host_->window(),
                                &new_bounds_in_root);
    gfx::Point last_mouse_location = GetLastMouseLocationInRoot();
    if ((old_bounds_in_root.Contains(last_mouse_location) !=
         new_bounds_in_root.Contains(last_mouse_location)) ||
        (new_bounds_in_root.Contains(last_mouse_location) &&
         new_bounds_in_root.origin() != old_bounds_in_root.origin())) {
      PostSynthesizeMouseMove(window);
    }
  }
}

void WindowEventDispatcher::OnWindowTargetTransformChanging(
    Window* window,
    const gfx::Transform& new_transform) {
  window_transforming_ = true;
  if (!synthesize_mouse_move_ && host_->window()->Contains(window))
    SynthesizeMouseMoveAfterChangeToWindow(window);
}

void WindowEventDispatcher::OnWindowTransformed(
    Window* window,
    ui::PropertyChangeReason reason) {
  // Call SynthesizeMouseMoveAfterChangeToWindow() only if it's the first time
  // that OnWindowTransformed() is called after
  // OnWindowTargetTransformChanging() (to avoid generating multiple mouse
  // events during animation).
  if (window_transforming_ && !synthesize_mouse_move_ &&
      host_->window()->Contains(window)) {
    SynthesizeMouseMoveAfterChangeToWindow(window);
  }
  window_transforming_ = false;
}

///////////////////////////////////////////////////////////////////////////////
// WindowEventDispatcher, EnvObserver implementation:

void WindowEventDispatcher::OnWindowInitialized(Window* window) {
  observation_manager_.AddObservation(window);
}

////////////////////////////////////////////////////////////////////////////////
// WindowEventDispatcher, private:

ui::EventDispatchDetails WindowEventDispatcher::DispatchHeldEvents() {
  if (!held_repostable_event_ && !held_move_event_) {
    if (did_dispatch_held_move_event_callback_)
      std::move(did_dispatch_held_move_event_callback_).Run();
    return DispatchDetails();
  }

  CHECK(!dispatching_held_event_);

  DispatchDetails dispatch_details;
  if (held_repostable_event_) {
    if (held_repostable_event_->type() == ui::EventType::kMousePressed ||
        held_repostable_event_->type() == ui::EventType::kTouchPressed) {
      std::unique_ptr<ui::LocatedEvent> event =
          std::move(held_repostable_event_);
      dispatching_held_event_ = event.get();
      dispatch_details = OnEventFromSource(event.get());
    } else {
      // TODO(rbyers): GESTURE_TAP_DOWN not yet supported: crbug.com/170987.
      NOTREACHED_IN_MIGRATION();
    }
    if (dispatch_details.dispatcher_destroyed)
      return dispatch_details;
  }

  if (held_move_event_) {
    // |held_move_event_| should be cleared here. Some event handler can
    // create its own run loop on an event (e.g. WindowMove loop for
    // tab-dragging), which means the other move events need to be processed
    // before this OnEventFromSource() finishes. See also b/119260190.
    std::unique_ptr<ui::LocatedEvent> event = std::move(held_move_event_);

    // If a mouse move has been synthesized, the target location is suspect,
    // so drop the held mouse event.
    if (event->IsTouchEvent() ||
        (event->IsMouseEvent() && !synthesize_mouse_move_)) {
      dispatching_held_event_ = event.get();
      dispatch_details = OnEventFromSource(event.get());
    }
  }

  if (!dispatch_details.dispatcher_destroyed) {
    dispatching_held_event_ = nullptr;
    Env::GetInstance()->window_event_dispatcher_observers().Notify(
        &WindowEventDispatcherObserver::
            OnWindowEventDispatcherDispatchedHeldEvents,
        this);
    if (did_dispatch_held_move_event_callback_)
      std::move(did_dispatch_held_move_event_callback_).Run();
  }

  return dispatch_details;
}

void WindowEventDispatcher::PostSynthesizeMouseMove(Window* window) {
  // No one should care where the real mouse is when this flag is on. So there
  // is no need to send a synthetic mouse move here.
  if (ui::PlatformEventSource::ShouldIgnoreNativePlatformEvents())
    return;

  if (synthesize_mouse_move_ || in_shutdown_)
    return;

#if BUILDFLAG(IS_WIN)
  // Gets the window at the current cursor point.
  gfx::Point cursor_point =
      display::Screen::GetScreen()->GetCursorScreenPoint();
  gfx::NativeWindow window_under_cursor =
      display::Screen::GetScreen()->GetWindowAtScreenPoint(cursor_point);

  ConvertPointFromScreen(&cursor_point);
  // If the mouse cursor is within the |window|, but |window_under_cursor| is
  // null, it means another program's window is occluding ours. And also, if
  // |window_under_cursor| doesn't belong to ours then we do not synthesize a
  // mouse move event.
  if (window->ContainsPointInRoot(cursor_point) &&
      (!window_under_cursor ||
       !host_->window()->Contains(window_under_cursor))) {
    return;
  }
#endif

  synthesize_mouse_move_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostNonNestableTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&WindowEventDispatcher::SynthesizeMouseMoveEvent),
          held_event_factory_.GetWeakPtr()));
}

void WindowEventDispatcher::SynthesizeMouseMoveAfterChangeToWindow(
    Window* window) {
  if (in_shutdown_)
    return;
  if (window->IsVisible() &&
      window->ContainsPointInRoot(GetLastMouseLocationInRoot())) {
    PostSynthesizeMouseMove(window);
  }
}

ui::EventDispatchDetails WindowEventDispatcher::SynthesizeMouseMoveEvent() {
  DispatchDetails details;
  if (!synthesize_mouse_move_ || in_shutdown_)
    return details;
  synthesize_mouse_move_ = false;

  // No need to generate mouse event if the cursor is invisible and not locked.
  client::CursorClient* cursor_client =
      client::GetCursorClient(host_->window());
  if (cursor_client && (!cursor_client->IsMouseEventsEnabled() ||
                        (!cursor_client->IsCursorVisible() &&
                         !cursor_client->IsCursorLocked()))) {
    return details;
  }

  // If one of the mouse buttons is currently down, then do not synthesize a
  // mouse-move event. In such cases, aura could synthesize a DRAGGED event
  // instead of a MOVED event, but in multi-display/multi-host scenarios, the
  // DRAGGED event can be synthesized in the incorrect host. So avoid
  // synthesizing any events at all.
  if (Env::GetInstance()->mouse_button_flags())
    return details;

  // Do not use GetLastMouseLocationInRoot here because it's not updated when
  // the mouse is not over the window or when the window is minimized.
  gfx::Point mouse_location =
      display::Screen::GetScreen()->GetCursorScreenPoint();
  ConvertPointFromScreen(&mouse_location);
  if (!window()->bounds().Contains(mouse_location))
    return details;
  gfx::Point host_mouse_location = mouse_location;
  host_->ConvertDIPToPixels(&host_mouse_location);
  ui::MouseEvent event(ui::EventType::kMouseMoved, host_mouse_location,
                       host_mouse_location, ui::EventTimeForNow(),
                       ui::EF_IS_SYNTHESIZED, 0);
  return OnEventFromSource(&event);
}

DispatchDetails WindowEventDispatcher::PreDispatchLocatedEvent(
    Window* target,
    ui::LocatedEvent* event) {
  int flags = event->flags();
  if (IsNonClientLocation(target, event->location()))
    flags |= ui::EF_IS_NON_CLIENT;
  event->SetFlags(flags);

  if (!is_dispatched_held_event(*event) &&
      (event->IsMouseEvent() || event->IsScrollEvent()) &&
      !(event->flags() & ui::EF_IS_SYNTHESIZED)) {
    synthesize_mouse_move_ = false;
  }

  return DispatchDetails();
}

DispatchDetails WindowEventDispatcher::PreDispatchMouseEvent(
    Window* target,
    ui::MouseEvent* event) {
  client::CursorClient* cursor_client = client::GetCursorClient(window());
  // We allow synthesized mouse exit events through even if mouse events are
  // disabled. This ensures that hover state, etc on controls like buttons is
  // cleared.
  if (cursor_client && !cursor_client->IsMouseEventsEnabled() &&
      (event->flags() & ui::EF_IS_SYNTHESIZED) &&
      (event->type() != ui::EventType::kMouseExited)) {
    event->SetHandled();
    return DispatchDetails();
  }

  Env::GetInstance()->env_controller()->UpdateStateForMouseEvent(window(),
                                                                 *event);

  if (IsEventCandidateForHold(*event) && !dispatching_held_event_) {
    if (move_hold_count_) {
      held_move_event_ =
          std::make_unique<ui::MouseEvent>(*event, target, window());
      event->SetHandled();
      return DispatchDetails();
    } else {
      // We may have a held event for a period between the time move_hold_count_
      // fell to 0 and the DispatchHeldEvents executes. Since we're going to
      // dispatch the new event directly below, we can reset the old one.
      held_move_event_.reset();
    }
  }

  switch (event->type()) {
    case ui::EventType::kMouseExited:
      if (!target || target == window()) {
        DispatchDetails details = DispatchMouseEnterOrExit(
            target, *event, ui::EventType::kMouseExited);
        if (details.dispatcher_destroyed) {
          event->SetHandled();
          return details;
        }
        mouse_moved_handler_ = nullptr;
      }
      break;
    case ui::EventType::kMouseMoved:
      // Send an exit to the current |mouse_moved_handler_| and an enter to
      // |target|. Take care that both us and |target| aren't destroyed during
      // dispatch.
      if (target != mouse_moved_handler_) {
        aura::Window* old_mouse_moved_handler = mouse_moved_handler_;
        WindowTracker live_window;
        live_window.Add(target);
        DispatchDetails details = DispatchMouseEnterOrExit(
            target, *event, ui::EventType::kMouseExited);
        // |details| contains information about |mouse_moved_handler_| being
        // destroyed which is not our |target|. Return value of this function
        // should be about our |target|.
        DispatchDetails target_details = details;
        target_details.target_destroyed = !live_window.Contains(target);
        if (details.dispatcher_destroyed) {
          event->SetHandled();
          return target_details;
        }
        // If the |mouse_moved_handler_| changes out from under us, assume a
        // nested run loop ran and we don't need to do anything.
        if (mouse_moved_handler_ != old_mouse_moved_handler) {
          event->SetHandled();
          return target_details;
        }
        if (details.target_destroyed || target_details.target_destroyed) {
          mouse_moved_handler_ = nullptr;
          event->SetHandled();
          return target_details;
        }
        live_window.Remove(target);

        mouse_moved_handler_ = target;
        details = DispatchMouseEnterOrExit(target, *event,
                                           ui::EventType::kMouseEntered);
        if (details.dispatcher_destroyed || details.target_destroyed) {
          event->SetHandled();
          return details;
        }
      }
      break;
    case ui::EventType::kMousePressed:
      // Don't set the mouse pressed handler for non client mouse down events.
      // These are only sent by Windows and are not always followed with non
      // client mouse up events which causes subsequent mouse events to be
      // sent to the wrong target.
      if (!(event->flags() & ui::EF_IS_NON_CLIENT) && !mouse_pressed_handler_)
        mouse_pressed_handler_ = target;
      break;
    case ui::EventType::kMouseReleased:
      mouse_pressed_handler_ = nullptr;
      break;
    default:
      break;
  }

  return PreDispatchLocatedEvent(target, event);
}

DispatchDetails WindowEventDispatcher::PreDispatchPinchEvent(
    Window* target,
    ui::GestureEvent* event) {
  if (event->details().device_type() != ui::GestureDeviceType::DEVICE_TOUCHPAD)
    return PreDispatchLocatedEvent(target, event);
  switch (event->type()) {
    case ui::EventType::kGesturePinchBegin:
      touchpad_pinch_handler_ = target;
      break;
    case ui::EventType::kGesturePinchEnd:
      touchpad_pinch_handler_ = nullptr;
      break;
    default:
      break;
  }

  return PreDispatchLocatedEvent(target, event);
}

DispatchDetails WindowEventDispatcher::PreDispatchTouchEvent(
    Window* target,
    ui::TouchEvent* event) {
  if (event->type() == ui::EventType::kTouchMoved && move_hold_count_ &&
      !dispatching_held_event_) {
    held_move_event_ =
        std::make_unique<ui::TouchEvent>(*event, target, window());
    event->SetHandled();
    return DispatchDetails();
  }

  Env::GetInstance()->env_controller()->UpdateStateForTouchEvent(target,
                                                                 *event);

  ui::TouchEvent root_relative_event(*event);
  root_relative_event.set_location_f(event->root_location_f());
  Env* env = Env::GetInstance();
  if (!env->gesture_recognizer()->ProcessTouchEventPreDispatch(
          &root_relative_event, target)) {
    // The event is invalid - ignore it.
    event->StopPropagation();
    event->DisableSynchronousHandling();
    env->window_event_dispatcher_observers().Notify(
        &WindowEventDispatcherObserver::OnWindowEventDispatcherIgnoredEvent,
        this);
    return DispatchDetails();
  }

  // This flag is set depending on the gestures recognized in the call above,
  // and needs to propagate with the forwarded event.
  event->set_may_cause_scrolling(root_relative_event.may_cause_scrolling());

  return PreDispatchLocatedEvent(target, event);
}

DispatchDetails WindowEventDispatcher::PreDispatchKeyEvent(
    Window* target,
    ui::KeyEvent* event) {
  if (skip_ime_ || !host_->has_input_method() ||
      (event->flags() & ui::EF_IS_SYNTHESIZED) ||
      !host_->ShouldSendKeyEventToIme() ||
      target->GetProperty(aura::client::kSkipImeProcessing)) {
    return DispatchDetails();
  }

  // At this point (i.e: EP_PREDISPATCH), event target is still not set, so do
  // it explicitly here thus making it possible for InputMethodContext
  // implementation to retrieve target window through KeyEvent::target().
  // Event::target is reset at WindowTreeHost::DispatchKeyEventPostIME(), just
  // after key is processed by InputMethodContext.
  ui::Event::DispatcherApi(event).set_target(window());

  DispatchDetails details = host_->GetInputMethod()->DispatchKeyEvent(event);
  event->StopPropagation();
  return details;
}

std::unique_ptr<cc::EventsMetricsManager::ScopedMonitor>
WindowEventDispatcher::CreateScropedMetricsMonitorForEvent(
    const ui::Event& event) {
  std::unique_ptr<cc::EventMetrics> metrics;
  if (event.IsScrollGestureEvent() || event.IsPinchEvent()) {
    const auto* gesture = event.AsGestureEvent();
    // There are many tests that don't set the device type properly, so if the
    // device type is not set, we'll consider it as touchpad/wheel.
    ui::ScrollInputType input_type =
        gesture->details().device_type() ==
                ui::GestureDeviceType::DEVICE_TOUCHSCREEN
            ? ui::ScrollInputType::kTouchscreen
            : ui::ScrollInputType::kWheel;
    if (gesture->type() == ui::EventType::kGestureScrollUpdate) {
      metrics = cc::ScrollUpdateEventMetrics::CreateForBrowser(
          ui::EventType::kGestureScrollUpdate, input_type,
          /*is_inertial=*/false,
          has_seen_gesture_scroll_update_after_begin_
              ? cc::ScrollUpdateEventMetrics::ScrollUpdateType::kContinued
              : cc::ScrollUpdateEventMetrics::ScrollUpdateType::kStarted,
          gesture->details().scroll_y(), gesture->time_stamp(),
          base::IdType64<class ui::LatencyInfo>(event.latency()->trace_id()));
      has_seen_gesture_scroll_update_after_begin_ = true;
    } else if (gesture->IsScrollGestureEvent()) {
      metrics = cc::ScrollEventMetrics::CreateForBrowser(
          gesture->type(), input_type,
          /*is_inertial=*/false, gesture->time_stamp(),
          base::IdType64<class ui::LatencyInfo>(event.latency()->trace_id()));
      if (gesture->type() == ui::EventType::kGestureScrollBegin) {
        has_seen_gesture_scroll_update_after_begin_ = false;
      }
    } else {
      DCHECK(gesture->IsPinchEvent());
      metrics = cc::PinchEventMetrics::Create(
          gesture->type(), input_type, gesture->time_stamp(),
          base::IdType64<class ui::LatencyInfo>(event.latency()->trace_id()));
    }
  } else {
    metrics = cc::EventMetrics::Create(
        event.type(), event.time_stamp(),
        base::IdType64<class ui::LatencyInfo>(event.latency()->trace_id()));
  }
  cc::EventsMetricsManager::ScopedMonitor::DoneCallback done_callback;
  if (metrics) {
    // TODO(crbug.com/40208152): The following breakdown has the renderer word
    // in its name, so not the best breakdown to use in the browser. Introduce
    // and use breakdowns specific to the browser.
    metrics->SetDispatchStageTimestamp(
        cc::EventMetrics::DispatchStage::kRendererMainStarted);
    done_callback = base::BindOnce(
        [](std::unique_ptr<cc::EventMetrics> metrics, bool handled) {
          // TODO(crbug.com/40208152): The following breakdown has the renderer
          // word in its name, so not the best breakdown to use in the
          // browser. Introduce and use breakdowns specific to the browser.
          metrics->SetDispatchStageTimestamp(
              cc::EventMetrics::DispatchStage::kRendererMainFinished);
          return handled ? std::move(metrics) : nullptr;
        },
        std::move(metrics));
  }
  return host_->compositor()->GetScopedEventMetricsMonitor(
      std::move(done_callback));
}

}  // namespace aura
