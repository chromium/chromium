// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/input_handler_proxy.h"

#include <stddef.h>

#include <algorithm>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/profiler/sample_metadata.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_mouse_wheel_event.h"
#include "third_party/blink/public/platform/web_touch_event.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/compositor_thread_event_queue.h"
#include "ui/events/blink/did_overscroll_params.h"
#include "ui/events/blink/event_with_callback.h"
#include "ui/events/blink/input_handler_proxy_client.h"
#include "ui/events/blink/input_scroll_elasticity_controller.h"
#include "ui/events/blink/momentum_scroll_jank_tracker.h"
#include "ui/events/blink/scroll_predictor.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/latency/latency_info.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace {

const int32_t kEventDispositionUndefined = -1;

cc::ScrollState CreateScrollStateForGesture(const WebGestureEvent& event) {
  cc::ScrollStateData scroll_state_data;
  switch (event.GetType()) {
    case WebInputEvent::kGestureScrollBegin:
      scroll_state_data.position_x = event.PositionInWidget().x;
      scroll_state_data.position_y = event.PositionInWidget().y;
      scroll_state_data.delta_x_hint = -event.data.scroll_begin.delta_x_hint;
      scroll_state_data.delta_y_hint = -event.data.scroll_begin.delta_y_hint;
      scroll_state_data.is_beginning = true;
      // On Mac, a GestureScrollBegin in the inertial phase indicates a fling
      // start.
      scroll_state_data.is_in_inertial_phase =
          (event.data.scroll_begin.inertial_phase ==
           WebGestureEvent::InertialPhaseState::kMomentum);
      scroll_state_data.delta_granularity =
          static_cast<double>(event.data.scroll_begin.delta_hint_units);
      break;
    case WebInputEvent::kGestureScrollUpdate:
      scroll_state_data.delta_x = -event.data.scroll_update.delta_x;
      scroll_state_data.delta_y = -event.data.scroll_update.delta_y;
      scroll_state_data.velocity_x = event.data.scroll_update.velocity_x;
      scroll_state_data.velocity_y = event.data.scroll_update.velocity_y;
      scroll_state_data.is_in_inertial_phase =
          event.data.scroll_update.inertial_phase ==
          WebGestureEvent::InertialPhaseState::kMomentum;
      scroll_state_data.delta_granularity =
          static_cast<double>(event.data.scroll_update.delta_units);
      break;
    case WebInputEvent::kGestureScrollEnd:
      scroll_state_data.is_ending = true;
      break;
    default:
      NOTREACHED();
      break;
  }
  return cc::ScrollState(scroll_state_data);
}

cc::ScrollState CreateScrollStateForInertialEnd() {
  cc::ScrollStateData scroll_state_data;
  scroll_state_data.is_ending = true;
  return cc::ScrollState(scroll_state_data);
}

cc::ScrollState CreateScrollStateForInertialUpdate(
    const gfx::Vector2dF& delta) {
  cc::ScrollStateData scroll_state_data;
  scroll_state_data.delta_x = delta.x();
  scroll_state_data.delta_y = delta.y();
  scroll_state_data.is_in_inertial_phase = true;
  return cc::ScrollState(scroll_state_data);
}

cc::InputHandler::ScrollInputType GestureScrollInputType(
    blink::WebGestureDevice device) {
  switch (device) {
    case blink::WebGestureDevice::kTouchpad:
      return cc::InputHandler::WHEEL;
    case blink::WebGestureDevice::kTouchscreen:
      return cc::InputHandler::TOUCHSCREEN;
    case blink::WebGestureDevice::kSyntheticAutoscroll:
      return cc::InputHandler::AUTOSCROLL;
    case blink::WebGestureDevice::kScrollbar:
      return cc::InputHandler::SCROLLBAR;
    case blink::WebGestureDevice::kUninitialized:
      break;
  }
  NOTREACHED();
  return cc::InputHandler::SCROLL_INPUT_UNKNOWN;
}

cc::SnapFlingController::GestureScrollType GestureScrollEventType(
    WebInputEvent::Type web_event_type) {
  switch (web_event_type) {
    case WebInputEvent::kGestureScrollBegin:
      return cc::SnapFlingController::GestureScrollType::kBegin;
    case WebInputEvent::kGestureScrollUpdate:
      return cc::SnapFlingController::GestureScrollType::kUpdate;
    case WebInputEvent::kGestureScrollEnd:
      return cc::SnapFlingController::GestureScrollType::kEnd;
    default:
      NOTREACHED();
      return cc::SnapFlingController::GestureScrollType::kBegin;
  }
}

cc::SnapFlingController::GestureScrollUpdateInfo GetGestureScrollUpdateInfo(
    const WebGestureEvent& event) {
  cc::SnapFlingController::GestureScrollUpdateInfo info;
  info.delta = gfx::Vector2dF(-event.data.scroll_update.delta_x,
                              -event.data.scroll_update.delta_y);
  info.is_in_inertial_phase =
      event.data.scroll_update.inertial_phase ==
      blink::WebGestureEvent::InertialPhaseState::kMomentum;
  info.event_time = event.TimeStamp();
  return info;
}

enum ScrollingThreadStatus {
  SCROLLING_ON_COMPOSITOR,
  SCROLLING_ON_COMPOSITOR_BLOCKED_ON_MAIN,
  SCROLLING_ON_MAIN,
  LAST_SCROLLING_THREAD_STATUS_VALUE = SCROLLING_ON_MAIN,
};

}  // namespace

namespace ui {

InputHandlerProxy::InputHandlerProxy(cc::InputHandler* input_handler,
                                     InputHandlerProxyClient* client,
                                     bool force_input_to_main_thread)
    : client_(client),
      input_handler_(input_handler),
      synchronous_input_handler_(nullptr),
      allow_root_animate_(true),
#if DCHECK_IS_ON()
      expect_scroll_update_end_(false),
#endif
      gesture_scroll_on_impl_thread_(false),
      scroll_sequence_ignored_(false),
      smooth_scroll_enabled_(false),
      touch_result_(kEventDispositionUndefined),
      mouse_wheel_result_(kEventDispositionUndefined),
      current_overscroll_params_(nullptr),
      has_ongoing_compositor_scroll_or_pinch_(false),
      has_seen_first_gesture_scroll_update_after_begin_(false),
      last_injected_gesture_was_begin_(false),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      snap_fling_controller_(std::make_unique<cc::SnapFlingController>(this)),
      compositor_touch_action_enabled_(
          base::FeatureList::IsEnabled(features::kCompositorTouchAction)),
      force_input_to_main_thread_(force_input_to_main_thread) {
  DCHECK(client);
  input_handler_->BindToClient(this);
  cc::ScrollElasticityHelper* scroll_elasticity_helper =
      input_handler_->CreateScrollElasticityHelper();
  if (scroll_elasticity_helper) {
    scroll_elasticity_controller_.reset(
        new InputScrollElasticityController(scroll_elasticity_helper));
  }
  compositor_event_queue_ = std::make_unique<CompositorThreadEventQueue>();
  scroll_predictor_ =
      base::FeatureList::IsEnabled(features::kResamplingScrollEvents)
          ? std::make_unique<ScrollPredictor>()
          : nullptr;

  if (base::FeatureList::IsEnabled(features::kSkipTouchEventFilter) &&
      GetFieldTrialParamValueByFeature(
          features::kSkipTouchEventFilter,
          features::kSkipTouchEventFilterFilteringProcessParamName) ==
          features::
              kSkipTouchEventFilterFilteringProcessParamValueBrowserAndRenderer) {
    // Skipping filtering for touch events on renderer process is enabled.
    // Always skip filtering discrete events.
    skip_touch_filter_discrete_ = true;
    if (GetFieldTrialParamValueByFeature(
            features::kSkipTouchEventFilter,
            features::kSkipTouchEventFilterTypeParamName) ==
        features::kSkipTouchEventFilterTypeParamValueAll) {
      // The experiment config also specifies to skip touchmove events.
      skip_touch_filter_all_ = true;
    }
  }
}

InputHandlerProxy::~InputHandlerProxy() {}

void InputHandlerProxy::WillShutdown() {
  scroll_elasticity_controller_.reset();
  input_handler_ = NULL;
  client_->WillShutdown();
}

void InputHandlerProxy::HandleInputEventWithLatencyInfo(
    WebScopedInputEvent event,
    const LatencyInfo& latency_info,
    EventDispositionCallback callback) {
  DCHECK(input_handler_);

  TRACE_EVENT_WITH_FLOW1("input,benchmark", "LatencyInfo.Flow",
                         TRACE_ID_DONT_MANGLE(latency_info.trace_id()),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "step", "HandleInputEventImpl");

  std::unique_ptr<EventWithCallback> event_with_callback =
      std::make_unique<EventWithCallback>(std::move(event), latency_info,
                                          tick_clock_->NowTicks(),
                                          std::move(callback));

  enum {
    NO_SCROLL_PINCH = 0,
    ONGOING_SCROLL_PINCH = 1,
    SCROLL_PINCH = 2,
  };
  // Note: Other input can race ahead of gesture input as they don't have to go
  // through the queue, but we believe it's OK to do so.
  if (!IsGestureScrollOrPinch(event_with_callback->event().GetType())) {
    base::ScopedSampleMetadata metadata("Input.GestureScrollOrPinch",
                                        NO_SCROLL_PINCH);
    DispatchSingleInputEvent(std::move(event_with_callback),
                             tick_clock_->NowTicks());
    return;
  }

  base::ScopedSampleMetadata metadata("Input.GestureScrollOrPinch",
                                      has_ongoing_compositor_scroll_or_pinch_
                                          ? ONGOING_SCROLL_PINCH
                                          : SCROLL_PINCH);
  const auto& gesture_event = ToWebGestureEvent(event_with_callback->event());
  const bool is_first_gesture_scroll_update =
      !has_seen_first_gesture_scroll_update_after_begin_ &&
      gesture_event.GetType() == blink::WebGestureEvent::kGestureScrollUpdate;

  if (gesture_event.GetType() == blink::WebGestureEvent::kGestureScrollBegin) {
    has_seen_first_gesture_scroll_update_after_begin_ = false;
  } else if (gesture_event.GetType() ==
             blink::WebGestureEvent::kGestureScrollUpdate) {
    has_seen_first_gesture_scroll_update_after_begin_ = true;
  }

  if (has_ongoing_compositor_scroll_or_pinch_) {
    bool is_from_set_non_blocking_touch =
        gesture_event.SourceDevice() == blink::WebGestureDevice::kTouchscreen &&
        gesture_event.is_source_touch_event_set_non_blocking;
    bool is_scroll_end_from_wheel =
        gesture_event.SourceDevice() == blink::WebGestureDevice::kTouchpad &&
        gesture_event.GetType() == blink::WebGestureEvent::kGestureScrollEnd;
    bool scroll_update_has_blocking_wheel_source =
        gesture_event.SourceDevice() == blink::WebGestureDevice::kTouchpad &&
        is_first_gesture_scroll_update;

    if (is_from_set_non_blocking_touch || is_scroll_end_from_wheel ||
        scroll_update_has_blocking_wheel_source || synchronous_input_handler_) {
      // 1. Gesture events was already delayed by blocking events in rAF aligned
      // queue. We want to avoid additional one frame delay by flushing the
      // VSync queue immediately.
      // The first GSU latency was tracked by:
      // |smoothness.tough_scrolling_cases:first_gesture_scroll_update_latency|.
      // 2. |synchronous_input_handler_| is WebView only. WebView has different
      // mechanisms and we want to forward all events immediately.
      compositor_event_queue_->Queue(std::move(event_with_callback),
                                     tick_clock_->NowTicks());
      DispatchQueuedInputEvents();
      return;
    }

    bool needs_animate_input = compositor_event_queue_->empty();
    compositor_event_queue_->Queue(std::move(event_with_callback),
                                   tick_clock_->NowTicks());
    if (needs_animate_input)
      input_handler_->SetNeedsAnimateInput();
    return;
  }

  // We have to dispatch the event to know whether the gesture sequence will be
  // handled by the compositor or not.
  DispatchSingleInputEvent(std::move(event_with_callback),
                           tick_clock_->NowTicks());
}

void InputHandlerProxy::DispatchSingleInputEvent(
    std::unique_ptr<EventWithCallback> event_with_callback,
    const base::TimeTicks now) {
  const ui::LatencyInfo& original_latency_info =
      event_with_callback->latency_info();
  ui::LatencyInfo monitored_latency_info = original_latency_info;
  std::unique_ptr<cc::SwapPromiseMonitor> latency_info_swap_promise_monitor =
      input_handler_->CreateLatencyInfoSwapPromiseMonitor(
          &monitored_latency_info);

  current_overscroll_params_.reset();

  InputHandlerProxy::EventDisposition disposition = RouteToTypeSpecificHandler(
      event_with_callback->event(), original_latency_info);

  blink::WebGestureEvent::Type type = event_with_callback->event().GetType();
  switch (type) {
    case blink::WebGestureEvent::kGestureScrollBegin:
    case blink::WebGestureEvent::kGesturePinchBegin:
    case blink::WebGestureEvent::kGestureScrollUpdate:
    case blink::WebGestureEvent::kGesturePinchUpdate:
      has_ongoing_compositor_scroll_or_pinch_ = disposition == DID_HANDLE;
      break;

    case blink::WebGestureEvent::kGestureScrollEnd:
    case blink::WebGestureEvent::kGesturePinchEnd:
      has_ongoing_compositor_scroll_or_pinch_ = false;
      break;
    default:
      break;
  }

  // Handle jank tracking during the momentum phase of a scroll gesture. The
  // class filters non-momentum events internally.
  switch (type) {
    case blink::WebGestureEvent::kGestureScrollBegin:
      momentum_scroll_jank_tracker_ =
          std::make_unique<MomentumScrollJankTracker>();
      break;
    case blink::WebGestureEvent::kGestureScrollUpdate:
      // It's possible to get a scroll update without a begin. Ignore these
      // cases.
      if (momentum_scroll_jank_tracker_) {
        momentum_scroll_jank_tracker_->OnDispatchedInputEvent(
            event_with_callback.get(), now);
      }
      break;
    case blink::WebGestureEvent::kGestureScrollEnd:
      momentum_scroll_jank_tracker_.reset();
      break;
    default:
      break;
  }

  // Will run callback for every original events.
  event_with_callback->RunCallbacks(disposition, monitored_latency_info,
                                    std::move(current_overscroll_params_));
}

void InputHandlerProxy::DispatchQueuedInputEvents() {
  // Calling |NowTicks()| is expensive so we only want to do it once.
  base::TimeTicks now = tick_clock_->NowTicks();
  while (!compositor_event_queue_->empty())
    DispatchSingleInputEvent(compositor_event_queue_->Pop(), now);
}

// This function handles creating synthetic Gesture events. It is currently used
// for creating Gesture event equivalents for mouse events on a composited
// scrollbar. (See InputHandlerProxy::HandleInputEvent)
void InputHandlerProxy::InjectScrollbarGestureScroll(
    const WebInputEvent::Type type,
    const blink::WebFloatPoint& position_in_widget,
    const cc::InputHandlerPointerResult& pointer_result,
    const LatencyInfo& latency_info,
    const base::TimeTicks original_timestamp) {
  gfx::Vector2dF scroll_delta(pointer_result.scroll_offset.x(),
                              pointer_result.scroll_offset.y());

  std::unique_ptr<WebGestureEvent> synthetic_gesture_event =
      GenerateInjectedScrollGesture(
          type, original_timestamp, blink::WebGestureDevice::kScrollbar,
          position_in_widget, scroll_delta,
          pointer_result.scroll_units);

  // This will avoid hit testing and directly scroll the scroller with the
  // provided element_id.
  if (type == WebInputEvent::Type::kGestureScrollBegin)
    synthetic_gesture_event->data.scroll_begin.scrollable_area_element_id =
        pointer_result.target_scroller.GetStableId();

  WebScopedInputEvent web_scoped_gesture_event(
      synthetic_gesture_event.release());

  // Send in a LatencyInfo with SCROLLBAR type so that the end to end latency
  // is calculated specifically for scrollbars.
  LatencyInfo scrollbar_latency_info(latency_info);
  scrollbar_latency_info.set_source_event_type(ui::SourceEventType::SCROLLBAR);

  // This latency_info should not have already been scheduled for rendering -
  // i.e. it should be the original latency_info that was associated with the
  // input event that caused this scroll injection. If it has already been
  // scheduled it won't get queued to be shipped off with the CompositorFrame
  // when the gesture is handled.
  DCHECK(!scrollbar_latency_info.FindLatency(
      ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL_COMPONENT, nullptr));

  if (type == WebInputEvent::Type::kGestureScrollBegin) {
    last_injected_gesture_was_begin_ = true;
  } else {
    if (type == WebInputEvent::Type::kGestureScrollUpdate) {
      // For injected GSUs, add a scroll update component to the latency info
      // so that it is properly classified as a scroll. If the last injected
      // gesture was a GSB, then this GSU is the first scroll update - mark
      // the LatencyInfo as such.
      scrollbar_latency_info.AddLatencyNumberWithTimestamp(
          (last_injected_gesture_was_begin_)
              ? ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT
              : ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
          original_timestamp);
    }

    last_injected_gesture_was_begin_ = false;
  }

  std::unique_ptr<EventWithCallback> gesture_event_with_callback_update =
      std::make_unique<EventWithCallback>(
          std::move(web_scoped_gesture_event), scrollbar_latency_info,
          original_timestamp, original_timestamp, nullptr);

  bool needs_animate_input = compositor_event_queue_->empty();
  compositor_event_queue_->Queue(std::move(gesture_event_with_callback_update),
                                 original_timestamp);

  if (needs_animate_input)
    input_handler_->SetNeedsAnimateInput();
}

bool HasModifier(const WebInputEvent& event) {
#if defined(OS_MACOSX)
  // Mac uses the "Option" key (which is mapped to the enum "kAltKey").
  return event.GetModifiers() & WebInputEvent::kAltKey;
#else
  return event.GetModifiers() & WebInputEvent::kShiftKey;
#endif
}

InputHandlerProxy::EventDisposition
InputHandlerProxy::RouteToTypeSpecificHandler(
    const WebInputEvent& event,
    const LatencyInfo& original_latency_info) {
  DCHECK(input_handler_);

  if (force_input_to_main_thread_)
    return DID_NOT_HANDLE;

  if (IsGestureScroll(event.GetType()) &&
      (snap_fling_controller_->FilterEventForSnap(
          GestureScrollEventType(event.GetType())))) {
    return DROP_EVENT;
  }

  switch (event.GetType()) {
    case WebInputEvent::kMouseWheel:
      return HandleMouseWheel(static_cast<const WebMouseWheelEvent&>(event));

    case WebInputEvent::kGestureScrollBegin:
      return HandleGestureScrollBegin(
          static_cast<const WebGestureEvent&>(event));

    case WebInputEvent::kGestureScrollUpdate:
      return HandleGestureScrollUpdate(
          static_cast<const WebGestureEvent&>(event));

    case WebInputEvent::kGestureScrollEnd:
      return HandleGestureScrollEnd(static_cast<const WebGestureEvent&>(event));

    case WebInputEvent::kGesturePinchBegin: {
      DCHECK(!gesture_pinch_in_progress_);
      input_handler_->PinchGestureBegin();
      gesture_pinch_in_progress_ = true;
      return DID_HANDLE;
    }

    case WebInputEvent::kGesturePinchEnd: {
      DCHECK(gesture_pinch_in_progress_);
      gesture_pinch_in_progress_ = false;
      const WebGestureEvent& gesture_event =
          static_cast<const WebGestureEvent&>(event);
      input_handler_->PinchGestureEnd(
          gfx::ToFlooredPoint(gesture_event.PositionInWidget()),
          gesture_event.SourceDevice() == blink::WebGestureDevice::kTouchpad);
      return DID_HANDLE;
    }

    case WebInputEvent::kGesturePinchUpdate: {
      DCHECK(gesture_pinch_in_progress_);
      const WebGestureEvent& gesture_event =
          static_cast<const WebGestureEvent&>(event);
      input_handler_->PinchGestureUpdate(
          gesture_event.data.pinch_update.scale,
          gfx::ToFlooredPoint(gesture_event.PositionInWidget()));
      return DID_HANDLE;
    }

    case WebInputEvent::kTouchStart:
      return HandleTouchStart(static_cast<const WebTouchEvent&>(event));

    case WebInputEvent::kTouchMove:
      return HandleTouchMove(static_cast<const WebTouchEvent&>(event));

    case WebInputEvent::kTouchEnd:
      return HandleTouchEnd(static_cast<const WebTouchEvent&>(event));

    case WebInputEvent::kMouseDown: {
      // Only for check scrollbar captured
      const WebMouseEvent& mouse_event =
          static_cast<const WebMouseEvent&>(event);

      if (mouse_event.button == blink::WebMouseEvent::Button::kLeft) {
        CHECK(input_handler_);
        // TODO(arakeri): Pass in the modifier instead of a bool once the
        // refactor (crbug.com/1022097) is done. For details, see
        // crbug.com/1016955.
        cc::InputHandlerPointerResult pointer_result =
            input_handler_->MouseDown(
                gfx::PointF(mouse_event.PositionInWidget()),
                HasModifier(event));
        if (pointer_result.type == cc::PointerResultType::kScrollbarScroll) {
          // Generate GSB and GSU events and add them to the
          // CompositorThreadEventQueue.
          // Note that the latency info passed in to
          // InjectScrollbarGestureScroll is the original LatencyInfo, not the
          // one that may be currently monitored. The currently monitored one
          // may be modified by the call to InjectScrollbarGestureScroll, as
          // it will SetNeedsAnimateInput if the CompositorThreadEventQueue is
          // currently empty.
          InjectScrollbarGestureScroll(WebInputEvent::Type::kGestureScrollBegin,
                                       mouse_event.PositionInWidget(),
                                       pointer_result, original_latency_info,
                                       mouse_event.TimeStamp());

          // Don't need to inject GSU if the scroll offset is zero (this can
          // be the case where mouse down occurs on the thumb).
          if (!pointer_result.scroll_offset.IsZero()) {
            InjectScrollbarGestureScroll(
                WebInputEvent::Type::kGestureScrollUpdate,
                mouse_event.PositionInWidget(), pointer_result,
                original_latency_info, mouse_event.TimeStamp());
          }

          // Drop the mousedown for now as the gesture event equivalent for this
          // has already been added to the CompositorThreadEventQueue and will
          // be dispatched at the vsync boundary.
          return DROP_EVENT;
        }
      }

      return DID_NOT_HANDLE;
    }
    case WebInputEvent::kMouseUp: {
      // Only for release scrollbar captured
      const WebMouseEvent& mouse_event =
          static_cast<const WebMouseEvent&>(event);

      if (mouse_event.button == blink::WebMouseEvent::Button::kLeft) {
        CHECK(input_handler_);
        cc::InputHandlerPointerResult pointer_result = input_handler_->MouseUp(
            gfx::PointF(mouse_event.PositionInWidget()));
        if (pointer_result.type == cc::PointerResultType::kScrollbarScroll) {
          // Generate a GSE and add it to the CompositorThreadEventQueue.
          InjectScrollbarGestureScroll(WebInputEvent::Type::kGestureScrollEnd,
                                       mouse_event.PositionInWidget(),
                                       pointer_result, original_latency_info,
                                       mouse_event.TimeStamp());

          // Drop the mouseup for now as the gesture event equivalent for this
          // has already been added to the CompositorThreadEventQueue and will
          // be dispatched at the vsync boundary.
          return DROP_EVENT;
        }
      }
      return DID_NOT_HANDLE;
    }
    case WebInputEvent::kMouseMove: {
      const WebMouseEvent& mouse_event =
          static_cast<const WebMouseEvent&>(event);
      // TODO(davemoore): This should never happen, but bug #326635 showed some
      // surprising crashes.
      CHECK(input_handler_);
      cc::InputHandlerPointerResult pointer_result =
          input_handler_->MouseMoveAt(
              gfx::Point(mouse_event.PositionInWidget().x,
                         mouse_event.PositionInWidget().y));
      if (pointer_result.type == cc::PointerResultType::kScrollbarScroll) {
        // Generate a GSU event and add it to the CompositorThreadEventQueue.
        InjectScrollbarGestureScroll(WebInputEvent::Type::kGestureScrollUpdate,
                                     mouse_event.PositionInWidget(),
                                     pointer_result, original_latency_info,
                                     mouse_event.TimeStamp());

        // Drop the mousemove for now as the gesture event equivalent for this
        // has already been added to the CompositorThreadEventQueue and will
        // be dispatched at the vsync boundary.
        return DROP_EVENT;
      }
      return DID_NOT_HANDLE;
    }
    case WebInputEvent::kMouseLeave: {
      CHECK(input_handler_);
      input_handler_->MouseLeave();
      return DID_NOT_HANDLE;
    }
    // Fling gestures are handled only in the browser process and not sent to
    // the renderer.
    case WebInputEvent::kGestureFlingStart:
    case WebInputEvent::kGestureFlingCancel:
      NOTREACHED();
      break;

    default:
      break;
  }

  return DID_NOT_HANDLE;
}

void InputHandlerProxy::RecordMainThreadScrollingReasons(
    blink::WebGestureDevice device,
    uint32_t reasons) {
  static const char* kGestureHistogramName =
      "Renderer4.MainThreadGestureScrollReason";
  static const char* kWheelHistogramName =
      "Renderer4.MainThreadWheelScrollReason";

  if (device != blink::WebGestureDevice::kTouchpad &&
      device != blink::WebGestureDevice::kTouchscreen) {
    return;
  }

  // NonCompositedScrollReasons should only be set on the main thread.
  DCHECK(
      !cc::MainThreadScrollingReason::HasNonCompositedScrollReasons(reasons));

  int32_t event_disposition_result =
      (device == blink::WebGestureDevice::kTouchpad ? mouse_wheel_result_
                                                    : touch_result_);
  if (event_disposition_result == DID_NOT_HANDLE) {
    // We should also collect main thread scrolling reasons if a scroll event
    // scrolls on impl thread but is blocked by main thread event handlers.
    reasons |= (device == blink::WebGestureDevice::kTouchpad
                    ? cc::MainThreadScrollingReason::kWheelEventHandlerRegion
                    : cc::MainThreadScrollingReason::kTouchEventHandlerRegion);
  }

  // UMA_HISTOGRAM_ENUMERATION requires that the enum_max must be strictly
  // greater than the sample value. kMainThreadScrollingReasonCount doesn't
  // include the NotScrollingOnMain enum but the histograms do so adding
  // the +1 is necessary.
  // TODO(dcheng): Fix https://crbug.com/705169 so this isn't needed.
  constexpr uint32_t kMainThreadScrollingReasonEnumMax =
      cc::MainThreadScrollingReason::kMainThreadScrollingReasonCount + 1;
  if (reasons == cc::MainThreadScrollingReason::kNotScrollingOnMain) {
    if (device == blink::WebGestureDevice::kTouchscreen) {
      UMA_HISTOGRAM_ENUMERATION(
          kGestureHistogramName,
          cc::MainThreadScrollingReason::kNotScrollingOnMain,
          kMainThreadScrollingReasonEnumMax);
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          kWheelHistogramName,
          cc::MainThreadScrollingReason::kNotScrollingOnMain,
          kMainThreadScrollingReasonEnumMax);
    }
  }

  for (uint32_t i = 0;
       i < cc::MainThreadScrollingReason::kMainThreadScrollingReasonCount;
       ++i) {
    unsigned val = 1 << i;
    if (reasons & val) {
      if (val == cc::MainThreadScrollingReason::kHandlingScrollFromMainThread) {
        // We only want to record "Handling scroll from main thread" reason if
        // it's the only reason. If it's not the only reason, the "real" reason
        // for scrolling on main is something else, and we only want to pay
        // attention to that reason.
        if (reasons & ~val)
          continue;
      }
      if (device == blink::WebGestureDevice::kTouchscreen) {
        UMA_HISTOGRAM_ENUMERATION(kGestureHistogramName, i + 1,
                                  kMainThreadScrollingReasonEnumMax);
      } else {
        UMA_HISTOGRAM_ENUMERATION(kWheelHistogramName, i + 1,
                                  kMainThreadScrollingReasonEnumMax);
      }
    }
  }
}

bool InputHandlerProxy::ShouldAnimate(bool has_precise_scroll_deltas) const {
#if defined(OS_MACOSX)
  // Mac does not smooth scroll wheel events (crbug.com/574283).
  return false;
#else
  return smooth_scroll_enabled_ && !has_precise_scroll_deltas;
#endif
}

InputHandlerProxy::EventDisposition InputHandlerProxy::HandleMouseWheel(
    const WebMouseWheelEvent& wheel_event) {
  InputHandlerProxy::EventDisposition result = DROP_EVENT;

  if (wheel_event.dispatch_type == WebInputEvent::kEventNonBlocking) {
    // The first wheel event in the sequence should be cancellable.
    DCHECK(wheel_event.phase != WebMouseWheelEvent::kPhaseBegan);
    // Noncancellable wheel events should have phase info.
    DCHECK(wheel_event.phase != WebMouseWheelEvent::kPhaseNone ||
           wheel_event.momentum_phase != WebMouseWheelEvent::kPhaseNone);

    result = static_cast<EventDisposition>(mouse_wheel_result_);

    if (wheel_event.phase == WebMouseWheelEvent::kPhaseEnded ||
        wheel_event.phase == WebMouseWheelEvent::kPhaseCancelled ||
        wheel_event.momentum_phase == WebMouseWheelEvent::kPhaseEnded ||
        wheel_event.momentum_phase == WebMouseWheelEvent::kPhaseCancelled) {
      mouse_wheel_result_ = kEventDispositionUndefined;
    }
    if (mouse_wheel_result_ != kEventDispositionUndefined)
      return result;
  }

  blink::WebFloatPoint position_in_widget = wheel_event.PositionInWidget();
  if (input_handler_->HasBlockingWheelEventHandlerAt(
          gfx::Point(position_in_widget.x, position_in_widget.y))) {
    result = DID_NOT_HANDLE;
  } else {
    cc::EventListenerProperties properties =
        input_handler_->GetEventListenerProperties(
            cc::EventListenerClass::kMouseWheel);
    switch (properties) {
      case cc::EventListenerProperties::kBlockingAndPassive:
      case cc::EventListenerProperties::kPassive:
        result = DID_HANDLE_NON_BLOCKING;
        break;
      case cc::EventListenerProperties::kNone:
        result = DROP_EVENT;
        break;
      default:
        // If properties is kBlocking, and the event falls outside wheel event
        // handler region, we should handle it the same as kNone.
        result = DROP_EVENT;
    }
  }

  mouse_wheel_result_ = result;
  return result;
}

InputHandlerProxy::EventDisposition InputHandlerProxy::HandleGestureScrollBegin(
    const WebGestureEvent& gesture_event) {
  TRACE_EVENT0("input", "InputHandlerProxy::HandleGestureScrollBegin");

  if (scroll_predictor_)
    scroll_predictor_->ResetOnGestureScrollBegin(gesture_event);

#if DCHECK_IS_ON()
  expect_scroll_update_end_ = true;
#endif
  cc::ScrollState scroll_state = CreateScrollStateForGesture(gesture_event);
  cc::InputHandler::ScrollStatus scroll_status;
  cc::ElementIdType element_id_type =
      gesture_event.data.scroll_begin.scrollable_area_element_id;
  if (element_id_type) {
    scroll_state.data()->set_current_native_scrolling_element(
        cc::ElementId(element_id_type));
  }
  if (gesture_event.data.scroll_begin.delta_hint_units ==
      ui::input_types::ScrollGranularity::kScrollByPage) {
    scroll_status.thread = cc::InputHandler::SCROLL_ON_MAIN_THREAD;
    scroll_status.main_thread_scrolling_reasons =
        cc::MainThreadScrollingReason::kContinuingMainThreadScroll;
  } else if (gesture_event.data.scroll_begin.target_viewport) {
    scroll_status = input_handler_->RootScrollBegin(
        &scroll_state, GestureScrollInputType(gesture_event.SourceDevice()));
  } else if (ShouldAnimate(
                 gesture_event.data.scroll_begin.delta_hint_units !=
                 ui::input_types::ScrollGranularity::kScrollByPixel)) {
    DCHECK(!scroll_state.is_in_inertial_phase());
    scroll_status = input_handler_->ScrollAnimatedBegin(&scroll_state);
  } else {
    scroll_status = input_handler_->ScrollBegin(
        &scroll_state, GestureScrollInputType(gesture_event.SourceDevice()));
  }
  RecordMainThreadScrollingReasons(gesture_event.SourceDevice(),
                                   scroll_status.main_thread_scrolling_reasons);

  InputHandlerProxy::EventDisposition result = DID_NOT_HANDLE;
  scroll_sequence_ignored_ = false;
  in_inertial_scrolling_ = false;
  switch (scroll_status.thread) {
    case cc::InputHandler::SCROLL_ON_IMPL_THREAD:
      TRACE_EVENT_INSTANT0("input", "Handle On Impl", TRACE_EVENT_SCOPE_THREAD);
      gesture_scroll_on_impl_thread_ = true;
      if (input_handler_->IsCurrentlyScrollingViewport())
        client_->DidStartScrollingViewport();

      if (scroll_status.bubble)
        result = DID_HANDLE_SHOULD_BUBBLE;
      else
        result = DID_HANDLE;
      break;
    case cc::InputHandler::SCROLL_UNKNOWN:
    case cc::InputHandler::SCROLL_ON_MAIN_THREAD:
      TRACE_EVENT_INSTANT0("input", "Handle On Main", TRACE_EVENT_SCOPE_THREAD);
      result = DID_NOT_HANDLE;
      break;
    case cc::InputHandler::SCROLL_IGNORED:
      TRACE_EVENT_INSTANT0("input", "Ignore Scroll", TRACE_EVENT_SCOPE_THREAD);
      scroll_sequence_ignored_ = true;
      result = DROP_EVENT;
      break;
  }
  if (scroll_elasticity_controller_ && result != DID_NOT_HANDLE)
    HandleScrollElasticityOverscroll(gesture_event,
                                     cc::InputHandlerScrollResult());

  return result;
}

InputHandlerProxy::EventDisposition
InputHandlerProxy::HandleGestureScrollUpdate(
    const WebGestureEvent& gesture_event) {
#if DCHECK_IS_ON()
  DCHECK(expect_scroll_update_end_);
#endif

  gfx::Vector2dF scroll_delta(-gesture_event.data.scroll_update.delta_x,
                              -gesture_event.data.scroll_update.delta_y);
  TRACE_EVENT2("input", "InputHandlerProxy::HandleGestureScrollUpdate", "dx",
               scroll_delta.x(), "dy", scroll_delta.y());

  if (scroll_sequence_ignored_) {
    TRACE_EVENT_INSTANT0("input", "Scroll Sequence Ignored",
                         TRACE_EVENT_SCOPE_THREAD);
    return DROP_EVENT;
  }

  if (!gesture_scroll_on_impl_thread_ && !gesture_pinch_in_progress_)
    return DID_NOT_HANDLE;

  cc::ScrollState scroll_state = CreateScrollStateForGesture(gesture_event);
  in_inertial_scrolling_ = scroll_state.is_in_inertial_phase();
  gfx::PointF scroll_point(gesture_event.PositionInWidget());

  if (ShouldAnimate(gesture_event.data.scroll_update.delta_units !=
                    ui::input_types::ScrollGranularity::kScrollByPixel)) {
    DCHECK(!scroll_state.is_in_inertial_phase());
    base::TimeTicks event_time = gesture_event.TimeStamp();
    base::TimeDelta delay = base::TimeTicks::Now() - event_time;
    switch (input_handler_
                ->ScrollAnimated(gfx::ToFlooredPoint(scroll_point),
                                 scroll_delta, delay)
                .thread) {
      case cc::InputHandler::SCROLL_ON_IMPL_THREAD:
        return DID_HANDLE;
      case cc::InputHandler::SCROLL_IGNORED:
        TRACE_EVENT_INSTANT0("input", "Scroll Ignored",
                             TRACE_EVENT_SCOPE_THREAD);
        return DROP_EVENT;
      case cc::InputHandler::SCROLL_ON_MAIN_THREAD:
      case cc::InputHandler::SCROLL_UNKNOWN:
        if (input_handler_->ScrollingShouldSwitchtoMainThread()) {
          TRACE_EVENT_INSTANT0("input", "Move Scroll To Main Thread",
                               TRACE_EVENT_SCOPE_THREAD);
          gesture_scroll_on_impl_thread_ = false;
          client_->GenerateScrollBeginAndSendToMainThread(gesture_event);
        }
        return DID_NOT_HANDLE;
    }
  }

  if (snap_fling_controller_->HandleGestureScrollUpdate(
          GetGestureScrollUpdateInfo(gesture_event))) {
#if DCHECK_IS_ON()
    expect_scroll_update_end_ = false;
#endif
    gesture_scroll_on_impl_thread_ = false;
    return DROP_EVENT;
  }

  cc::InputHandlerScrollResult scroll_result =
      input_handler_->ScrollBy(&scroll_state);

  if (!scroll_result.did_scroll &&
      input_handler_->ScrollingShouldSwitchtoMainThread()) {
    gesture_scroll_on_impl_thread_ = false;
    client_->GenerateScrollBeginAndSendToMainThread(gesture_event);

    if (!gesture_pinch_in_progress_)
      return DID_NOT_HANDLE;
  }

  HandleOverscroll(scroll_point, scroll_result);

  if (scroll_elasticity_controller_)
    HandleScrollElasticityOverscroll(gesture_event, scroll_result);

  return scroll_result.did_scroll ? DID_HANDLE : DROP_EVENT;
}

InputHandlerProxy::EventDisposition InputHandlerProxy::HandleGestureScrollEnd(
  const WebGestureEvent& gesture_event) {
  TRACE_EVENT0("input", "InputHandlerProxy::HandleGestureScrollEnd");
#if DCHECK_IS_ON()
  DCHECK(expect_scroll_update_end_);
  expect_scroll_update_end_ = false;
#endif

  if (scroll_sequence_ignored_)
    return DROP_EVENT;

  if (!gesture_scroll_on_impl_thread_)
    return DID_NOT_HANDLE;

  cc::ScrollState scroll_state = CreateScrollStateForGesture(gesture_event);
  input_handler_->ScrollEnd(&scroll_state, true);

  if (scroll_elasticity_controller_)
    HandleScrollElasticityOverscroll(gesture_event,
                                     cc::InputHandlerScrollResult());

  gesture_scroll_on_impl_thread_ = false;
  return DID_HANDLE;
}

InputHandlerProxy::EventDisposition InputHandlerProxy::HitTestTouchEvent(
    const blink::WebTouchEvent& touch_event,
    bool* is_touching_scrolling_layer,
    cc::TouchAction* white_listed_touch_action) {
  *is_touching_scrolling_layer = false;
  EventDisposition result = DROP_EVENT;
  for (size_t i = 0; i < touch_event.touches_length; ++i) {
    if (touch_event.touch_start_or_first_touch_move)
      DCHECK(white_listed_touch_action);
    else
      DCHECK(!white_listed_touch_action);

    if (touch_event.GetType() == WebInputEvent::kTouchStart &&
        touch_event.touches[i].state != WebTouchPoint::kStatePressed) {
      continue;
    }

    cc::TouchAction touch_action = cc::kTouchActionAuto;
    cc::InputHandler::TouchStartOrMoveEventListenerType event_listener_type =
        input_handler_->EventListenerTypeForTouchStartOrMoveAt(
            gfx::Point(touch_event.touches[i].PositionInWidget().x,
                       touch_event.touches[i].PositionInWidget().y),
            &touch_action);
    if (white_listed_touch_action)
      *white_listed_touch_action &= touch_action;

    if (event_listener_type !=
        cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER) {
      *is_touching_scrolling_layer =
          event_listener_type ==
          cc::InputHandler::TouchStartOrMoveEventListenerType::
              HANDLER_ON_SCROLLING_LAYER;
      // A non-passive touch start / move will always set the whitelisted touch
      // action to kTouchActionNone, and in that case we do not ack the event
      // from the compositor.
      if (compositor_touch_action_enabled_ && white_listed_touch_action &&
          *white_listed_touch_action != cc::kTouchActionNone)
        result = DID_HANDLE_NON_BLOCKING;
      else
        result = DID_NOT_HANDLE;
      break;
    }
  }

  // If |result| is DROP_EVENT it wasn't processed above.
  if (result == DROP_EVENT) {
    switch (input_handler_->GetEventListenerProperties(
        cc::EventListenerClass::kTouchStartOrMove)) {
      case cc::EventListenerProperties::kPassive:
        result = DID_HANDLE_NON_BLOCKING;
        break;
      case cc::EventListenerProperties::kBlocking:
        // The touch area rects above already have checked whether it hits
        // a blocking region. Since it does not the event can be dropped.
        result = DROP_EVENT;
        break;
      case cc::EventListenerProperties::kBlockingAndPassive:
        // There is at least one passive listener that needs to possibly
        // be notified so it can't be dropped.
        result = DID_HANDLE_NON_BLOCKING;
        break;
      case cc::EventListenerProperties::kNone:
        result = DROP_EVENT;
        break;
      default:
        NOTREACHED();
        result = DROP_EVENT;
        break;
    }
  }

  // Depending on which arm of the SkipTouchEventFilter experiment we're on, we
  // may need to simulate a passive listener instead of dropping touch events.
  if (result == DROP_EVENT &&
      (skip_touch_filter_all_ ||
       (skip_touch_filter_discrete_ &&
        touch_event.GetType() == WebInputEvent::kTouchStart)))
    result = DID_HANDLE_NON_BLOCKING;

  // Merge |touch_result_| and |result| so the result has the highest
  // priority value according to the sequence; (DROP_EVENT,
  // DID_HANDLE_NON_BLOCKING, DID_NOT_HANDLE).
  if (touch_result_ == kEventDispositionUndefined ||
      touch_result_ == DROP_EVENT || result == DID_NOT_HANDLE)
    touch_result_ = result;
  return result;
}

InputHandlerProxy::EventDisposition InputHandlerProxy::HandleTouchStart(
    const blink::WebTouchEvent& touch_event) {
  bool is_touching_scrolling_layer;
  cc::TouchAction white_listed_touch_action = cc::kTouchActionAuto;
  EventDisposition result = HitTestTouchEvent(
      touch_event, &is_touching_scrolling_layer, &white_listed_touch_action);

  // If |result| is still DROP_EVENT look at the touch end handler as
  // we may not want to discard the entire touch sequence. Note this
  // code is explicitly after the assignment of the |touch_result_|
  // so the touch moves are not sent to the main thread un-necessarily.
  if (result == DROP_EVENT &&
      input_handler_->GetEventListenerProperties(
          cc::EventListenerClass::kTouchEndOrCancel) !=
          cc::EventListenerProperties::kNone) {
    result = DID_HANDLE_NON_BLOCKING;
  }

  bool is_in_inertial_scrolling_on_impl =
      in_inertial_scrolling_ && gesture_scroll_on_impl_thread_;
  if (is_in_inertial_scrolling_on_impl && is_touching_scrolling_layer)
    result = DID_NOT_HANDLE_NON_BLOCKING_DUE_TO_FLING;

  client_->SetWhiteListedTouchAction(white_listed_touch_action,
                                     touch_event.unique_touch_event_id, result);

  return result;
}

InputHandlerProxy::EventDisposition InputHandlerProxy::HandleTouchMove(
    const blink::WebTouchEvent& touch_event) {
  // Hit test if this is the first touch move or we don't have any results
  // from a previous hit test.
  if (touch_result_ == kEventDispositionUndefined ||
      touch_event.touch_start_or_first_touch_move) {
    bool is_touching_scrolling_layer;
    cc::TouchAction white_listed_touch_action = cc::kTouchActionAuto;
    EventDisposition result = HitTestTouchEvent(
        touch_event, &is_touching_scrolling_layer, &white_listed_touch_action);
    client_->SetWhiteListedTouchAction(
        white_listed_touch_action, touch_event.unique_touch_event_id, result);
    return result;
  }
  return static_cast<EventDisposition>(touch_result_);
}

InputHandlerProxy::EventDisposition InputHandlerProxy::HandleTouchEnd(
    const blink::WebTouchEvent& touch_event) {
  if (touch_event.touches_length == 1)
    touch_result_ = kEventDispositionUndefined;
  return DID_NOT_HANDLE;
}

void InputHandlerProxy::Animate(base::TimeTicks time) {
  // If using synchronous animate, then only expect Animate attempts started by
  // the synchronous system. Don't let the InputHandler try to Animate also.
  DCHECK(!input_handler_->IsCurrentlyScrollingViewport() ||
         allow_root_animate_);

  if (scroll_elasticity_controller_)
    scroll_elasticity_controller_->Animate(time);

  snap_fling_controller_->Animate(time);
}

void InputHandlerProxy::ReconcileElasticOverscrollAndRootScroll() {
  if (scroll_elasticity_controller_)
    scroll_elasticity_controller_->ReconcileStretchAndScroll();
}

void InputHandlerProxy::UpdateRootLayerStateForSynchronousInputHandler(
    const gfx::ScrollOffset& total_scroll_offset,
    const gfx::ScrollOffset& max_scroll_offset,
    const gfx::SizeF& scrollable_size,
    float page_scale_factor,
    float min_page_scale_factor,
    float max_page_scale_factor) {
  if (synchronous_input_handler_) {
    synchronous_input_handler_->UpdateRootLayerState(
        total_scroll_offset, max_scroll_offset, scrollable_size,
        page_scale_factor, min_page_scale_factor, max_page_scale_factor);
  }
}

void InputHandlerProxy::DeliverInputForBeginFrame(
    const viz::BeginFrameArgs& args) {
  if (!scroll_predictor_)
    DispatchQueuedInputEvents();

  // Resampling GSUs and dispatch queued input events.
  while (!compositor_event_queue_->empty()) {
    std::unique_ptr<EventWithCallback> event_with_callback =
        scroll_predictor_->ResampleScrollEvents(compositor_event_queue_->Pop(),
                                                args.frame_time);

    DispatchSingleInputEvent(std::move(event_with_callback), args.frame_time);
  }
}

void InputHandlerProxy::DeliverInputForHighLatencyMode() {
  // When prediction enabled, do not handle input after commit complete.
  if (!scroll_predictor_)
    DispatchQueuedInputEvents();
}

void InputHandlerProxy::SetOnlySynchronouslyAnimateRootFlings(
    SynchronousInputHandler* synchronous_input_handler) {
  allow_root_animate_ = !synchronous_input_handler;
  synchronous_input_handler_ = synchronous_input_handler;
  if (synchronous_input_handler_)
    input_handler_->RequestUpdateForSynchronousInputHandler();
}

void InputHandlerProxy::SynchronouslyAnimate(base::TimeTicks time) {
  // When this function is used, SetOnlySynchronouslyAnimate() should have been
  // previously called. IOW you should either be entirely in synchronous mode or
  // not.
  DCHECK(synchronous_input_handler_);
  DCHECK(!allow_root_animate_);
  base::AutoReset<bool> reset(&allow_root_animate_, true);
  Animate(time);
}

void InputHandlerProxy::SynchronouslySetRootScrollOffset(
    const gfx::ScrollOffset& root_offset) {
  DCHECK(synchronous_input_handler_);
  input_handler_->SetSynchronousInputHandlerRootScrollOffset(root_offset);
}

void InputHandlerProxy::SynchronouslyZoomBy(float magnify_delta,
                                            const gfx::Point& anchor) {
  DCHECK(synchronous_input_handler_);
  input_handler_->PinchGestureBegin();
  input_handler_->PinchGestureUpdate(magnify_delta, anchor);
  input_handler_->PinchGestureEnd(anchor, false);
}

bool InputHandlerProxy::GetSnapFlingInfoAndSetSnapTarget(
    const gfx::Vector2dF& natural_displacement,
    gfx::Vector2dF* initial_offset,
    gfx::Vector2dF* target_offset) const {
  return input_handler_->GetSnapFlingInfoAndSetSnapTarget(
      natural_displacement, initial_offset, target_offset);
}

gfx::Vector2dF InputHandlerProxy::ScrollByForSnapFling(
    const gfx::Vector2dF& delta) {
  cc::ScrollState scroll_state = CreateScrollStateForInertialUpdate(delta);
  cc::InputHandlerScrollResult scroll_result =
      input_handler_->ScrollBy(&scroll_state);
  return scroll_result.current_visual_offset;
}

void InputHandlerProxy::ScrollEndForSnapFling() {
  cc::ScrollState scroll_state = CreateScrollStateForInertialEnd();
  input_handler_->ScrollEnd(&scroll_state, false);
}

void InputHandlerProxy::RequestAnimationForSnapFling() {
  RequestAnimation();
}

void InputHandlerProxy::HandleOverscroll(
    const gfx::PointF& causal_event_viewport_point,
    const cc::InputHandlerScrollResult& scroll_result) {
  DCHECK(client_);
  if (!scroll_result.did_overscroll_root)
    return;

  TRACE_EVENT2("input",
               "InputHandlerProxy::DidOverscroll",
               "dx",
               scroll_result.unused_scroll_delta.x(),
               "dy",
               scroll_result.unused_scroll_delta.y());

  // Bundle overscroll message with triggering event response, saving an IPC.
  current_overscroll_params_ = std::make_unique<DidOverscrollParams>();
  current_overscroll_params_->accumulated_overscroll =
      scroll_result.accumulated_root_overscroll;
  current_overscroll_params_->latest_overscroll_delta =
      scroll_result.unused_scroll_delta;
  current_overscroll_params_->causal_event_viewport_point =
      causal_event_viewport_point;
  current_overscroll_params_->overscroll_behavior =
      scroll_result.overscroll_behavior;
  return;
}

void InputHandlerProxy::RequestAnimation() {
  // When a SynchronousInputHandler is present, root flings should go through
  // it to allow it to control when or if the root fling is animated. Non-root
  // flings always go through the normal InputHandler.
  if (synchronous_input_handler_ &&
      input_handler_->IsCurrentlyScrollingViewport())
    synchronous_input_handler_->SetNeedsSynchronousAnimateInput();
  else
    input_handler_->SetNeedsAnimateInput();
}

void InputHandlerProxy::HandleScrollElasticityOverscroll(
    const WebGestureEvent& gesture_event,
    const cc::InputHandlerScrollResult& scroll_result) {
  DCHECK(scroll_elasticity_controller_);
  // Send the event and its disposition to the elasticity controller to update
  // the over-scroll animation. Note that the call to the elasticity controller
  // is made asynchronously, to minimize divergence between main thread and
  // impl thread event handling paths.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &InputScrollElasticityController::ObserveGestureEventAndResult,
          scroll_elasticity_controller_->GetWeakPtr(), gesture_event,
          scroll_result));
}

void InputHandlerProxy::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

}  // namespace ui
