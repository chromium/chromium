// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/fuchsia/pointer_events_handler.h"

#include <lib/async/default.h>

#include <cmath>
#include <limits>
#include <memory>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "ui/events/event.h"
#include "ui/events/pointer_details.h"
#include "ui/events/types/event_type.h"

namespace fuchsia_ui_pointer {
// For using TouchInteractionId as a map key.
bool operator==(const TouchInteractionId& a, const TouchInteractionId& b) {
  return a.device_id() == b.device_id() && a.pointer_id() == b.pointer_id() &&
         a.interaction_id() == b.interaction_id();
}
}  // namespace fuchsia_ui_pointer

namespace ui {

namespace {

const int kWheelDelta = 120;

void IssueTouchTraceEvent(const fuchsia_ui_pointer::TouchEvent& event) {
  DCHECK(event.trace_flow_id()) << "API guarantee";
  TRACE_EVENT_WITH_FLOW0("input", "dispatch_event_to_client",
                         event.trace_flow_id().value(),
                         TRACE_EVENT_FLAG_FLOW_OUT);
}

void IssueMouseTraceEvent(const fuchsia_ui_pointer::MouseEvent& event) {
  DCHECK(event.trace_flow_id()) << "API guarantee";
  TRACE_EVENT_WITH_FLOW0("input", "dispatch_event_to_client",
                         event.trace_flow_id().value(),
                         TRACE_EVENT_FLAG_FLOW_OUT);
}

bool HasValidTouchSample(const fuchsia_ui_pointer::TouchEvent& event) {
  if (!event.pointer_sample()) {
    return false;
  }
  DCHECK(event.pointer_sample()->interaction()) << "API guarantee";
  DCHECK(event.pointer_sample()->phase()) << "API guarantee";
  DCHECK(event.pointer_sample()->position_in_viewport()) << "API guarantee";
  return true;
}

bool HasValidMouseSample(const fuchsia_ui_pointer::MouseEvent& event) {
  if (!event.pointer_sample()) {
    return false;
  }
  const auto& sample = event.pointer_sample();
  DCHECK(sample->device_id()) << "API guarantee";
  DCHECK(sample->position_in_viewport()) << "API guarantee";
  DCHECK(!sample->pressed_buttons() || sample->pressed_buttons()->size() > 0)
      << "API guarantee";

  return true;
}

inline int FuchsiaButtonVectorToChromeButtonBitmap(
    const std::vector<uint8_t>& pressed_buttons,
    const std::vector<uint8_t>& possible_buttons) {
  int result = 0;
  const size_t num_buttons = possible_buttons.size();

  for (auto button : pressed_buttons) {
    // 0 maps to kPrimaryButton, and so on.
    if (num_buttons > 0 && button == possible_buttons[0]) {
      result |= EF_LEFT_MOUSE_BUTTON;
    }
    if (num_buttons > 1 && button == possible_buttons[1]) {
      result |= EF_RIGHT_MOUSE_BUTTON;
    }
    if (num_buttons > 2 && button == possible_buttons[2]) {
      result |= EF_MIDDLE_MOUSE_BUTTON;
    }
  }

  return result;
}

std::array<float, 2> ViewportToViewCoordinates(
    std::array<float, 2> viewport_coordinates,
    const std::array<float, 9>& viewport_to_view_transform) {
  // The transform matrix is a FIDL array with matrix data in column-major
  // order. For a matrix with data [a b c d e f g h i], and with the viewport
  // coordinates expressed as homogeneous coordinates, the logical view
  // coordinates are obtained with the following formula:
  //   |a d g|   |x|   |x'|
  //   |b e h| * |y| = |y'|
  //   |c f i|   |1|   |w'|
  // which we then normalize based on the w component:
  //   if w' not zero: (x'/w', y'/w')
  //   else (x', y')
  const auto& M = viewport_to_view_transform;
  const float x = viewport_coordinates[0];
  const float y = viewport_coordinates[1];
  const float xp = M[0] * x + M[3] * y + M[6];
  const float yp = M[1] * x + M[4] * y + M[7];
  const float wp = M[2] * x + M[5] * y + M[8];
  if (wp != 0) {
    return {xp / wp, yp / wp};
  } else {
    return {xp, yp};
  }
}

EventType GetEventTypeFromTouchEventPhase(
    fuchsia_ui_pointer::EventPhase phase) {
  switch (phase) {
    case fuchsia_ui_pointer::EventPhase::kAdd:
      return EventType::kTouchPressed;
    case fuchsia_ui_pointer::EventPhase::kChange:
      return EventType::kTouchMoved;
    case fuchsia_ui_pointer::EventPhase::kRemove:
      return EventType::kTouchReleased;
    case fuchsia_ui_pointer::EventPhase::kCancel:
      return EventType::kTouchCancelled;
  }
}

// TODO(crbug.com/40805737): Check if chrome gestures require strict boundaries.
std::array<float, 2> ClampToViewSpace(
    const float x,
    const float y,
    const fuchsia_ui_pointer::ViewParameters& p) {
  const float min_x = p.view().min()[0];
  const float min_y = p.view().min()[1];
  const float max_x = p.view().max()[0];
  const float max_y = p.view().max()[1];
  if (min_x <= x && x < max_x && min_y <= y && y < max_y) {
    return {x, y};  // No clamping to perform.
  }

  // View boundary is [min_x, max_x) x [min_y, max_y). Note that min is
  // inclusive, but max is exclusive - so we subtract epsilon.
  const float max_x_inclusive = std::nextafter(max_x, min_x);
  const float max_y_inclusive = std::nextafter(max_y, min_y);
  const float clamped_x = base::ranges::clamp(x, min_x, max_x_inclusive);
  const float clamped_y = base::ranges::clamp(y, min_y, max_y_inclusive);
  return {clamped_x, clamped_y};
}

// It returns a "draft" because the coordinates are logical. FlatlandWindow
// might apply view pixel ratio to obtain physical coordinates.
//
// The gestures expect a gesture to start within the logical view space, and
// is not tolerant of floating point drift. This function coerces just the DOWN
// event's coordinate to start within the logical view.
TouchEvent CreateTouchEventDraft(
    const fuchsia_ui_pointer::TouchEvent& event,
    const fuchsia_ui_pointer::ViewParameters& view_parameters) {
  DCHECK(HasValidTouchSample(event)) << "precondition";
  const auto& sample = event.pointer_sample();
  const auto& interaction = sample->interaction();

  auto timestamp = base::TimeTicks::FromZxTime(event.timestamp().value());
  auto event_type = GetEventTypeFromTouchEventPhase(sample->phase().value());

  // TODO(crbug.com/40808970): Consider packing device_id field into PointerId.
  DCHECK_LE(interaction->pointer_id(), 31U);
  PointerDetails pointer_details(EventPointerType::kTouch,
                                 interaction->pointer_id());
  // View parameters can change mid-interaction; apply transform on the fly.
  auto logical =
      ViewportToViewCoordinates(sample->position_in_viewport().value(),
                                view_parameters.viewport_to_view_transform());
  // TODO(fxbug.dev/88580): Consider setting hover via
  // ui::TouchEvent::set_hovering().
  gfx::PointF location(logical[0], logical[1]);
  gfx::PointF root_location(sample->position_in_viewport().value()[0],
                            sample->position_in_viewport().value()[1]);
  return TouchEvent(event_type, location, root_location, timestamp,
                    pointer_details);
}

// It returns a "draft" because the coordinates are logical. Later,
// FlatlandWindow might apply view pixel ratio to obtain physical coordinates.
//
// Phase data is computed before this call; it involves state tracking based on
// button-down state.
//
// Button data, if available, gets packed into the |buttons_flags| field, in
// button order (kMousePrimaryButton, etc). The device-assigned button
// IDs are provided in priority order in MouseEvent.device_info (at the start of
// channel connection), and maps from device button ID (given in
// fuchsia_ui_pointer::MouseEvent) to Chrome ui::EventFlags.
//
// Scroll data, if available, gets packed into the |scroll_delta_x| or
// |scroll_delta_y| fields, and the |signal_kind| field is set to kScroll.
// The PointerDataPacketConverter reads this field to synthesize events to match
// Chrome's expected pointer stream.
//
// The gestures expect a gesture to start within the logical view space, and
// is not tolerant of floating point drift. This function coerces just the DOWN
// event's coordinate to start within the logical view.
std::unique_ptr<MouseEvent> CreateMouseEventDraft(
    const fuchsia_ui_pointer::MouseEvent& event,
    const EventType event_type,
    const int pressed_buttons_flags,
    const int changed_buttons_flags,
    const fuchsia_ui_pointer::ViewParameters& view_parameters,
    const fuchsia_ui_pointer::MouseDeviceInfo& device_info) {
  DCHECK(HasValidMouseSample(event)) << "precondition";
  const auto& sample = event.pointer_sample();

  auto timestamp = base::TimeTicks::FromZxTime(event.timestamp().value());
  PointerDetails pointer_details(EventPointerType::kMouse,
                                 sample->device_id().value());
  // View parameters can change mid-interaction; apply transform on the fly.
  auto logical =
      ViewportToViewCoordinates(sample->position_in_viewport().value(),
                                view_parameters.viewport_to_view_transform());

  // Ensure gesture recognition: DOWN starts in the logical view space.
  if (event_type == EventType::kMousePressed) {
    logical = ClampToViewSpace(logical[0], logical[1], view_parameters);
  }

  auto location = gfx::PointF(logical[0], logical[1]);
  auto root_location = gfx::PointF(sample->position_in_viewport().value()[0],
                                   sample->position_in_viewport().value()[1]);

  if (event_type == EventType::kMousewheel) {
    // TODO(fxbug.dev/92938): Maybe also support ctrl+wheel event here.

    const int tick_x_120ths = sample->scroll_h().value_or(0) * kWheelDelta;
    const int tick_y_120ths = sample->scroll_v().value_or(0) * kWheelDelta;

    // Fuchsia reports suggested scroll pixel in physical, but for old version,
    // Fuchsia reports wheel rotated ticks need to multiple |kWheelDelta| for
    // pixel offset.
    const float offset_x =
        sample->scroll_h_physical_pixel().value_or(tick_x_120ths);
    const float offset_y =
        sample->scroll_v_physical_pixel().value_or(tick_y_120ths);

    if (sample->is_precision_scroll().has_value() &&
        sample->is_precision_scroll().value()) {
      // For precision scroll device, mostly are touchpads for now, need to use
      // ScrollEvent instead of MouseWheelEvent to prevent animation
      // interpolation in smooth scrolling.
      // Because we only support touchpad as precision scroll device now,
      // finger_count is 2. Maybe need to use different number when we support
      // precision wheel mouse.
      return std::make_unique<ScrollEvent>(
          ui::EventType::kScroll, location, root_location, timestamp,
          pressed_buttons_flags, offset_x, offset_y, offset_x, offset_y,
          /*finger_count=*/2);
    }
    return std::make_unique<MouseWheelEvent>(
        gfx::Vector2d(static_cast<int>(offset_x), static_cast<int>(offset_y)),
        location, root_location, timestamp, pressed_buttons_flags,
        changed_buttons_flags, gfx::Vector2d(tick_x_120ths, tick_y_120ths));
  }
  auto mouse_event = std::make_unique<MouseEvent>(
      event_type, location, root_location, timestamp, pressed_buttons_flags,
      changed_buttons_flags, pointer_details);
  mouse_event->InitializeNative();
  return mouse_event;
}

}  // namespace

PointerEventsHandler::PointerEventsHandler(
    fidl::ClientEnd<fuchsia_ui_pointer::TouchSource> touch_source,
    fidl::ClientEnd<fuchsia_ui_pointer::MouseSource> mouse_source)
    : touch_source_(std::move(touch_source), async_get_default_dispatcher()),
      mouse_source_(std::move(mouse_source), async_get_default_dispatcher()) {}

PointerEventsHandler::~PointerEventsHandler() = default;

// Core logic of this class.
// Aim to keep state management in this function.
void PointerEventsHandler::StartWatching(
    base::RepeatingCallback<void(Event*)> event_callback) {
  if (event_callback_) {
    LOG(ERROR) << "PointerEventsHandler::StartWatching() must be called once.";
    return;
  }
  event_callback_ = event_callback;

  // Start watching both channels.
  touch_source_->Watch(std::vector<fuchsia_ui_pointer::TouchResponse>())
      .Then(fit::bind_member(this,
                             &PointerEventsHandler::OnTouchSourceWatchResult));
  mouse_source_->Watch().Then(
      fit::bind_member(this, &PointerEventsHandler::OnMouseSourceWatchResult));
}

// There are three basic interaction forms that we need to handle, and the API
// guarantees we see only these three forms. S=sample, R(g)=result-granted,
// R(d)=result-denied, and + means packaged in the same table. Time flows from
// left to right. Samples start with ADD, and end in REMOVE or CANCEL. Each
// interaction receives just one ownership result.
//   (1) Late grant. S S S R(g) S S S
//   (1-a) Combo.    S S S+R(g) S S S
//   (2) Early grant. S+R(g) S S S S S
//   (3) Late deny. S S S R(d)
//   (3-a) Combo.   S S S+R(d)
//
// This results in the following high-level algorithm to correctly deal with
// buffer allocation and deletion, and event flushing or event dropping based
// on ownership.
//   if event.sample.phase == ADD && !event.result
//     allocate buffer[event.sample.interaction]
//   if buffer[event.sample.interaction]
//     buffer[event.sample.interaction].push(event.sample)
//   else
//     flush_to_client(event.sample)
//   if event.result
//     if event.result == GRANTED
//       flush_to_client(buffer[event.result.interaction])
//     delete buffer[event.result.interaction]
void PointerEventsHandler::OnTouchSourceWatchResult(
    fidl::Result<fuchsia_ui_pointer::TouchSource::Watch>& watch_result) {
  if (watch_result.is_error()) {
    ZX_DLOG(ERROR, watch_result.error_value().status()) << " in " << __func__;
    return;
  }
  auto& events = watch_result->events();
  TRACE_EVENT0("input", "PointerEventsHandler::OnTouchSourceWatchResult");
  std::vector<fuchsia_ui_pointer::TouchResponse> touch_responses;
  for (const fuchsia_ui_pointer::TouchEvent& event : events) {
    IssueTouchTraceEvent(event);
    fuchsia_ui_pointer::TouchResponse
        response;  // Response per event, matched on event's index.
    if (event.view_parameters()) {
      touch_view_parameters_ = std::move(event.view_parameters().value());
    }
    if (HasValidTouchSample(event)) {
      const auto& sample = event.pointer_sample();
      const auto& interaction = sample->interaction().value();
      if (sample->phase().value() == fuchsia_ui_pointer::EventPhase::kAdd &&
          !event.interaction_result()) {
        touch_buffer_.emplace(interaction, std::vector<TouchEvent>());
      }

      DCHECK(touch_view_parameters_.has_value()) << "API guarantee";
      auto draft = CreateTouchEventDraft(event, touch_view_parameters_.value());
      if (touch_buffer_.count(interaction) > 0) {
        touch_buffer_[interaction].emplace_back(std::move(draft));
      } else {
        event_callback_.Run(&draft);
      }

      // TODO(fxbug.dev/89296): Consider deriving response from
      // Event::handled().
      response.response_type(fuchsia_ui_pointer::TouchResponseType::kYes);
    }
    if (event.interaction_result()) {
      const auto& result = event.interaction_result();
      const auto& interaction = result->interaction();
      if (result->status() ==
              fuchsia_ui_pointer::TouchInteractionStatus::kGranted &&
          touch_buffer_.count(interaction) > 0) {
        for (auto& touch : touch_buffer_[interaction]) {
          event_callback_.Run(&touch);
        }
      }
      touch_buffer_.erase(interaction);  // Result seen, delete the buffer.
    }
    touch_responses.push_back(std::move(response));
  }

  touch_source_->Watch(std::move(touch_responses))
      .Then(fit::bind_member(this,
                             &PointerEventsHandler::OnTouchSourceWatchResult));
}

void PointerEventsHandler::OnMouseSourceWatchResult(
    fidl::Result<fuchsia_ui_pointer::MouseSource::Watch>& watch_result) {
  if (watch_result.is_error()) {
    DLOG(ERROR) << "OnMouseSourceWatchResult: "
                << watch_result.error_value().status_string();
    return;
  }
  auto& events = watch_result->events();
  TRACE_EVENT0("input", "PointerEventsHandler::OnMouseSourceWatchResult");
  for (fuchsia_ui_pointer::MouseEvent& event : events) {
    IssueMouseTraceEvent(event);
    if (event.device_info()) {
      const auto& id = event.device_info()->id().value();
      mouse_device_info_[id] = std::move(event.device_info().value());
    }
    if (event.view_parameters()) {
      mouse_view_parameters_ = std::move(event.view_parameters().value());
    }
    if (HasValidMouseSample(event)) {
      const auto& sample = event.pointer_sample();
      const auto& id = sample->device_id().value();
      DCHECK(mouse_view_parameters_.has_value()) << "API guarantee";
      DCHECK(mouse_device_info_.count(id) > 0) << "API guarantee";

      int pressed_buttons = sample->pressed_buttons()
                                ? FuchsiaButtonVectorToChromeButtonBitmap(
                                      sample->pressed_buttons().value(),
                                      mouse_device_info_[id].buttons().value())
                                : 0;
      int previous_buttons = mouse_down_[id];
      int changed_buttons = pressed_buttons ^ previous_buttons;

      // Update mouse_down_ for the next Fuchsia event.
      mouse_down_[id] = pressed_buttons;

      const bool is_wheel_event = sample->scroll_v() || sample->scroll_h() ||
                                  sample->scroll_h_physical_pixel() ||
                                  sample->scroll_v_physical_pixel();
      // Do not filterout mouse wheel here, because the wheel event may be
      // bundled with button down and button up event. Chromium will need to
      // split it to 2 events.
      const bool is_button_event = changed_buttons != 0;

      const bool is_move_or_drag_event = !is_wheel_event && !is_button_event;

      if (is_button_event) {
        // Iterate through possible mouse buttons and potentially emit an event
        // for each one.
        for (int button = EF_LEFT_MOUSE_BUTTON; button <= EF_RIGHT_MOUSE_BUTTON;
             button = button << 1) {
          DCHECK(button == EF_LEFT_MOUSE_BUTTON ||
                 button == EF_MIDDLE_MOUSE_BUTTON ||
                 button == EF_RIGHT_MOUSE_BUTTON);
          bool prev_down = previous_buttons & button;
          bool curr_down = pressed_buttons & button;

          if (!prev_down && !curr_down) {
            // We already handled move events and don't want to send extraneous
            // ones.
            continue;
          } else if (!prev_down && curr_down) {
            auto event_type = EventType::kMousePressed;
            auto draft = CreateMouseEventDraft(
                event, event_type, button, changed_buttons,
                mouse_view_parameters_.value(), mouse_device_info_[id]);
            event_callback_.Run(draft.get());
          } else if (prev_down && !curr_down) {
            auto event_type = EventType::kMouseReleased;
            auto draft = CreateMouseEventDraft(
                event, event_type, button, changed_buttons,
                mouse_view_parameters_.value(), mouse_device_info_[id]);
            event_callback_.Run(draft.get());
          }
        }
      }

      if (is_wheel_event) {
        // Handle the mouse scroll.
        auto draft = CreateMouseEventDraft(
            event, EventType::kMousewheel, pressed_buttons, changed_buttons,
            mouse_view_parameters_.value(), mouse_device_info_[id]);
        event_callback_.Run(draft.get());
      }

      if (is_move_or_drag_event) {
        auto event_type = (pressed_buttons == 0) ? EventType::kMouseMoved
                                                 : EventType::kMouseDragged;
        auto draft = CreateMouseEventDraft(
            event, event_type, pressed_buttons, changed_buttons,
            mouse_view_parameters_.value(), mouse_device_info_[id]);
        event_callback_.Run(draft.get());
      }
    }
  }

  mouse_source_->Watch().Then(
      fit::bind_member(this, &PointerEventsHandler::OnMouseSourceWatchResult));
}

}  // namespace ui
