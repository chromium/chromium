// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/touch_disposition_gesture_filter.h"

#include <stddef.h>

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/trace_event/typed_macros.h"
#include "base/types/cxx23_to_underlying.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/gesture_event_details.h"

namespace ui {
namespace {

// A BitSet32 is used for tracking dropped gesture types.
static_assert(base::to_underlying(EventType::kGestureTypeEnd) -
                      base::to_underlying(EventType::kGestureTypeStart) <
                  32,
              "gesture type count too large");

GestureEventData CreateGesture(EventType type,
                               int motion_event_id,
                               MotionEvent::ToolType primary_tool_type,
                               const GestureEventDataPacket& packet) {
  // As the event is purely synthetic, we needn't be strict with event flags.
  int flags = EF_NONE;
  GestureEventDetails details(type);
  details.set_device_type(GestureDeviceType::DEVICE_TOUCHSCREEN);
  return GestureEventData(details,
                          motion_event_id,
                          primary_tool_type,
                          packet.timestamp(),
                          packet.touch_location().x(),
                          packet.touch_location().y(),
                          packet.raw_touch_location().x(),
                          packet.raw_touch_location().y(),
                          1,
                          gfx::RectF(packet.touch_location(), gfx::SizeF()),
                          flags,
                          packet.unique_touch_event_id());
}

enum RequiredTouches {
  RT_NONE = 0,
  RT_START = 1 << 0,
  RT_CURRENT = 1 << 1,
};

struct DispositionHandlingInfo {
  // A bitwise-OR of |RequiredTouches|.
  int required_touches;
  EventType antecedent_event_type;

  explicit DispositionHandlingInfo(int required_touches)
      : required_touches(required_touches),
        antecedent_event_type(EventType::kUnknown) {}

  DispositionHandlingInfo(int required_touches,
                          EventType antecedent_event_type)
      : required_touches(required_touches),
        antecedent_event_type(antecedent_event_type) {}
};

DispositionHandlingInfo Info(int required_touches) {
  return DispositionHandlingInfo(required_touches);
}

DispositionHandlingInfo Info(int required_touches,
                             EventType antecedent_event_type) {
  return DispositionHandlingInfo(required_touches, antecedent_event_type);
}

// This approach to disposition handling is described at http://goo.gl/5G8PWJ.
DispositionHandlingInfo GetDispositionHandlingInfo(EventType type) {
  switch (type) {
    case EventType::kGestureTapDown:
      return Info(RT_START);
    case EventType::kGestureTapCancel:
      return Info(RT_START);
    case EventType::kGestureShowPress:
      return Info(RT_START);
    case EventType::kGestureLongPress:
      return Info(RT_START);
    case EventType::kGestureShortPress:
      return Info(RT_START);
    case EventType::kGestureLongTap:
      return Info(RT_START | RT_CURRENT);
    case EventType::kGestureTap:
      return Info(RT_START | RT_CURRENT, EventType::kGestureTapUnconfirmed);
    case EventType::kGestureTapUnconfirmed:
      return Info(RT_START | RT_CURRENT);
    case EventType::kGestureDoubleTap:
      return Info(RT_START | RT_CURRENT, EventType::kGestureTapUnconfirmed);
    case EventType::kGestureScrollBegin:
      return Info(RT_START);
    case EventType::kGestureScrollUpdate:
      return Info(RT_CURRENT, EventType::kGestureScrollBegin);
    case EventType::kGestureScrollEnd:
      return Info(RT_NONE, EventType::kGestureScrollBegin);
    case EventType::kScrollFlingStart:
      // We rely on |EndScrollGestureIfNecessary| to end the scroll if the fling
      // start is prevented.
      return Info(RT_NONE, EventType::kGestureScrollUpdate);
    case EventType::kScrollFlingCancel:
      return Info(RT_NONE, EventType::kScrollFlingStart);
    case EventType::kGesturePinchBegin:
      return Info(RT_START, EventType::kGestureScrollBegin);
    case EventType::kGesturePinchUpdate:
      return Info(RT_CURRENT, EventType::kGesturePinchBegin);
    case EventType::kGesturePinchEnd:
      return Info(RT_NONE, EventType::kGesturePinchBegin);
    case EventType::kGestureBegin:
      return Info(RT_START);
    case EventType::kGestureEnd:
      return Info(RT_NONE, EventType::kGestureBegin);
    case EventType::kGestureSwipe:
      return Info(RT_START, EventType::kGestureScrollBegin);
    case EventType::kGestureTwoFingerTap:
      return Info(RT_START);
    default:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return Info(RT_NONE);
}

int GetGestureTypeIndex(EventType type) {
  DCHECK_GE(type, EventType::kGestureTypeStart);
  DCHECK_LE(type, EventType::kGestureTypeEnd);
  return base::to_underlying(type) -
         base::to_underlying(EventType::kGestureTypeStart);
}

bool IsTouchStartEvent(GestureEventDataPacket::GestureSource gesture_source) {
  return gesture_source == GestureEventDataPacket::TOUCH_SEQUENCE_START ||
         gesture_source == GestureEventDataPacket::TOUCH_START;
}

bool DoAddInputTimestampsToGesture(const GestureEventData& gesture_data) {
  return gesture_data.type() == EventType::kGestureScrollUpdate ||
         gesture_data.type() == EventType::kGestureScrollBegin;
}

}  // namespace

// TouchDispositionGestureFilter

TouchDispositionGestureFilter::TouchDispositionGestureFilter(
    TouchDispositionGestureFilterClient* client)
    : client_(client),
      ending_event_motion_event_id_(0),
      ending_event_primary_tool_type_(MotionEvent::ToolType::UNKNOWN),
      needs_tap_ending_event_(false),
      needs_show_press_event_(false),
      needs_fling_ending_event_(false),
      needs_scroll_ending_event_(false) {
  DCHECK(client_);
}

TouchDispositionGestureFilter::~TouchDispositionGestureFilter() {
}

TouchDispositionGestureFilter::PacketResult
TouchDispositionGestureFilter::OnGesturePacket(
    const GestureEventDataPacket& packet) {
  if (packet.gesture_source() == GestureEventDataPacket::UNDEFINED ||
      packet.gesture_source() == GestureEventDataPacket::INVALID)
    return INVALID_PACKET_TYPE;

  if (packet.gesture_source() == GestureEventDataPacket::TOUCH_SEQUENCE_START)
    sequences_.push(GestureSequence());

  if (IsEmpty()) {
    if (packet.gesture_source() ==
        GestureEventDataPacket::TOUCH_SEQUENCE_CANCEL) {
      return EMPTY_GESTURE_SEQUENCE;
    }
    return INVALID_PACKET_ORDER;
  }

  if (packet.gesture_source() == GestureEventDataPacket::TOUCH_TIMEOUT &&
      Tail().empty()) {
    // Handle the timeout packet immediately if the packet preceding the timeout
    // has already been dispatched.
    FilterAndSendPacket(packet);
    return SUCCESS;
  }

  // Check the packet's unique_touch_event_id is valid and unique with the
  // exception of TOUCH_TIMEOUT packets which may share an id with other events.
  // |TOUCH_TIMEOUT| packets don't wait for an ack, they are dispatched
  // as soon as they reach the head of the queue, in |SendAckedEvents|.
  if (!Tail().empty()) {
    DCHECK((packet.gesture_source() == GestureEventDataPacket::TOUCH_TIMEOUT)
            || (packet.unique_touch_event_id() !=
                    Tail().back().unique_touch_event_id()));
  }
  if (!Head().empty()) {
    DCHECK((packet.gesture_source() == GestureEventDataPacket::TOUCH_TIMEOUT) ||
           packet.unique_touch_event_id() !=
               Head().front().unique_touch_event_id());
  }

  Tail().push(packet);
  return SUCCESS;
}

void TouchDispositionGestureFilter::OnTouchEventAck(
    uint32_t unique_touch_event_id,
    bool event_consumed,
    bool is_source_touch_event_set_blocking,
    const std::optional<EventLatencyMetadata>& event_latency_metadata) {
  // Spurious asynchronous acks should not trigger a crash.
  if (IsEmpty() || (Head().empty() && sequences_.size() == 1)) {
    TRACE_EVENT_INSTANT("input", "OnTouchEventAck spurious async ack");
    return;
  }

  if (Head().empty())
    PopGestureSequence();

  // If the tail's event_source is TOUCH_TIMEOUT, then it may share a
  // unique_touch_event_id() with another event; in this case don't ack it here.
  if (!Tail().empty() &&
      Tail().back().unique_touch_event_id() == unique_touch_event_id &&
      Tail().back().gesture_source() != GestureEventDataPacket::TOUCH_TIMEOUT) {
    Tail().back().Ack(event_consumed, is_source_touch_event_set_blocking);
    if (sequences_.size() == 1 && Tail().size() == 1)
      SendAckedEvents(event_latency_metadata);
  } else {
    DCHECK(!Head().empty());
    DCHECK_EQ(Head().front().unique_touch_event_id(), unique_touch_event_id);
    Head().front().Ack(event_consumed, is_source_touch_event_set_blocking);
    SendAckedEvents(event_latency_metadata);
  }
}

void TouchDispositionGestureFilter::SendAckedEvents(
    const std::optional<EventLatencyMetadata>& event_latency_metadata) {
  // Dispatch all packets corresponding to ack'ed touches, as well as
  // any pending timeout-based packets.
  bool touch_packet_for_current_ack_handled = false;
  while (!IsEmpty() && (!Head().empty() || sequences_.size() != 1)) {
    if (Head().empty())
      PopGestureSequence();
    GestureSequence& sequence = Head();

    DCHECK_NE(sequence.front().gesture_source(),
              GestureEventDataPacket::UNDEFINED);
    DCHECK_NE(sequence.front().gesture_source(),
              GestureEventDataPacket::INVALID);

    GestureEventDataPacket::GestureSource source =
        sequence.front().gesture_source();
    GestureEventDataPacket::AckState ack_state = sequence.front().ack_state();

    if (source != GestureEventDataPacket::TOUCH_TIMEOUT) {
      // We've sent all packets which aren't pending their ack.
      if (ack_state == GestureEventDataPacket::AckState::PENDING) {
        TRACE_EVENT_INSTANT("input", "SendAckedEvents RestPending");
        break;
      }
      state_.OnTouchEventAck(
          ack_state == GestureEventDataPacket::AckState::CONSUMED,
          IsTouchStartEvent(source));
    }
    // We need to pop the current sequence before sending the packet, because
    // sending the packet could result in this method being re-entered (e.g. on
    // Aura, we could trigger a touch-cancel). As popping the sequence destroys
    // the packet, we copy the packet before popping it.
    touch_packet_for_current_ack_handled = true;
    GestureEventDataPacket packet = sequence.front();
    sequence.pop();

    if (source == GestureEventDataPacket::TOUCH_MOVE &&
        event_latency_metadata.has_value()) {
      EventLatencyMetadata gesture_event_latency_metadata;
      gesture_event_latency_metadata
          .scrolls_blocking_touch_dispatched_to_renderer =
          event_latency_metadata->dispatched_to_renderer;
      packet.AddEventLatencyMetadataToGestures(
          std::move(gesture_event_latency_metadata),
          base::BindRepeating(DoAddInputTimestampsToGesture));
    }

    FilterAndSendPacket(packet);
  }
  DCHECK(touch_packet_for_current_ack_handled);
}

bool TouchDispositionGestureFilter::IsEmpty() const {
  return sequences_.empty();
}

void TouchDispositionGestureFilter::ResetGestureHandlingState() {
  state_ = GestureHandlingState();
}

void TouchDispositionGestureFilter::FilterAndSendPacket(
    const GestureEventDataPacket& packet) {
  TRACE_EVENT("input", "TouchDispositionGestureFilter::FilterAndSendPacket",
              [&](perfetto::EventContext ctx) {
                auto* event =
                    ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
                auto* filter = event->set_touch_disposition_gesture_filter();
                filter->set_gesture_count(packet.gesture_count());
              });
  if (packet.gesture_source() == GestureEventDataPacket::TOUCH_SEQUENCE_START) {
    CancelTapIfNecessary(packet);
    EndScrollIfNecessary(packet);
    CancelFlingIfNecessary(packet);
  } else if (packet.gesture_source() == GestureEventDataPacket::TOUCH_START) {
    CancelTapIfNecessary(packet);
  }
  int gesture_end_index = -1;
  for (size_t i = 0; i < packet.gesture_count(); ++i) {
    const GestureEventData& gesture = packet.gesture(i);
    DCHECK_GE(gesture.details.type(), EventType::kGestureTypeStart);
    DCHECK_LE(gesture.details.type(), EventType::kGestureTypeEnd);
    if (state_.Filter(gesture.details.type())) {
      CancelTapIfNecessary(packet);

      // Send the gesture begin and end events when the touch start event is
      // consumed. For every touch press and release, the gesture begin and end
      // events are always generated.
      if (base::FeatureList::IsEnabled(features::kEnableGestureBeginEndTypes) &&
          (gesture.details.type() == EventType::kGestureBegin ||
           gesture.details.type() == EventType::kGestureEnd)) {
        SendGesture(gesture, packet);
      }
      continue;
    }
    if (gesture.type() == EventType::kGestureTapCancel) {
      CancelTapIfNecessary(packet);
      continue;
    }
    if (packet.gesture_source() == GestureEventDataPacket::TOUCH_TIMEOUT) {
      // Sending a timed gesture could delete |this|, so we need to return
      // directly after the |SendGesture| call.
      SendGesture(gesture, packet);
      // We should not have a timeout gesture and other gestures in the same
      // packet.
      DCHECK_EQ(1U, packet.gesture_count());
      return;
    }
    // Occasionally scroll or tap cancel events are synthesized when a touch
    // sequence has been canceled or terminated, we want to make sure that
    // EventType::kGestureEnd always happens after them.
    if (gesture.type() == EventType::kGestureEnd) {
      // Make sure there is at most one EventType::kGestureEnd event in each
      // packet.
      DCHECK_EQ(-1, gesture_end_index);
      gesture_end_index = static_cast<int>(i);
      continue;
    }
    SendGesture(gesture, packet);
  }

  if (packet.gesture_source() ==
      GestureEventDataPacket::TOUCH_SEQUENCE_CANCEL) {
    EndScrollIfNecessary(packet);
    CancelTapIfNecessary(packet);
  } else if (packet.gesture_source() ==
             GestureEventDataPacket::TOUCH_SEQUENCE_END) {
    EndScrollIfNecessary(packet);
  }
  // Always send the EventType::kGestureEnd event as the last one for every
  // touch event.
  if (gesture_end_index >= 0)
    SendGesture(packet.gesture(gesture_end_index), packet);
}

void TouchDispositionGestureFilter::SendGesture(
    const GestureEventData& event,
    const GestureEventDataPacket& packet_being_sent) {
  TRACE_EVENT("input", "TouchDispositionGestureFilter::SendGesture");
  DCHECK(event.unique_touch_event_id ==
         packet_being_sent.unique_touch_event_id());

  // TODO(jdduke): Factor out gesture stream reparation code into a standalone
  // utility class.
  switch (event.type()) {
    case EventType::kGestureLongTap:
      if (!needs_tap_ending_event_)
        return;
      CancelTapIfNecessary(packet_being_sent);
      CancelFlingIfNecessary(packet_being_sent);
      break;
    case EventType::kGestureTapDown:
      DCHECK(!needs_tap_ending_event_);
      ending_event_motion_event_id_ = event.motion_event_id;
      ending_event_primary_tool_type_ = event.primary_tool_type;
      needs_show_press_event_ = true;
      needs_tap_ending_event_ = true;
      break;
    case EventType::kGestureShowPress:
      if (!needs_show_press_event_)
        return;
      needs_show_press_event_ = false;
      break;
    case EventType::kGestureDoubleTap:
      CancelTapIfNecessary(packet_being_sent);
      needs_show_press_event_ = false;
      break;
    case EventType::kGestureTap:
      DCHECK(needs_tap_ending_event_);
      if (needs_show_press_event_) {
        SendGesture(GestureEventData(EventType::kGestureShowPress, event),
                    packet_being_sent);
        DCHECK(!needs_show_press_event_);
      }
      needs_tap_ending_event_ = false;
      break;
    case EventType::kGestureTapCancel:
      needs_show_press_event_ = false;
      needs_tap_ending_event_ = false;
      break;
    case EventType::kGestureScrollBegin:
      CancelTapIfNecessary(packet_being_sent);
      CancelFlingIfNecessary(packet_being_sent);
      EndScrollIfNecessary(packet_being_sent);
      ending_event_motion_event_id_ = event.motion_event_id;
      ending_event_primary_tool_type_ = event.primary_tool_type;
      needs_scroll_ending_event_ = true;
      break;
    case EventType::kGestureScrollEnd:
      needs_scroll_ending_event_ = false;
      break;
    case EventType::kScrollFlingStart:
      CancelFlingIfNecessary(packet_being_sent);
      ending_event_motion_event_id_ = event.motion_event_id;
      ending_event_primary_tool_type_ = event.primary_tool_type;
      needs_fling_ending_event_ = true;
      needs_scroll_ending_event_ = false;
      break;
    case EventType::kScrollFlingCancel:
      needs_fling_ending_event_ = false;
      break;
    default:
      break;
  }
  client_->ForwardGestureEvent(event);
}

void TouchDispositionGestureFilter::CancelTapIfNecessary(
    const GestureEventDataPacket& packet_being_sent) {
  if (!needs_tap_ending_event_)
    return;

  SendGesture(
      CreateGesture(EventType::kGestureTapCancel, ending_event_motion_event_id_,
                    ending_event_primary_tool_type_, packet_being_sent),
      packet_being_sent);
  DCHECK(!needs_tap_ending_event_);
}

void TouchDispositionGestureFilter::CancelFlingIfNecessary(
    const GestureEventDataPacket& packet_being_sent) {
  if (!needs_fling_ending_event_)
    return;

  SendGesture(CreateGesture(EventType::kScrollFlingCancel,
                            ending_event_motion_event_id_,
                            ending_event_primary_tool_type_, packet_being_sent),
              packet_being_sent);
  DCHECK(!needs_fling_ending_event_);
}

void TouchDispositionGestureFilter::EndScrollIfNecessary(
    const GestureEventDataPacket& packet_being_sent) {
  if (!needs_scroll_ending_event_)
    return;

  SendGesture(
      CreateGesture(EventType::kGestureScrollEnd, ending_event_motion_event_id_,
                    ending_event_primary_tool_type_, packet_being_sent),
      packet_being_sent);
  DCHECK(!needs_scroll_ending_event_);
}

void TouchDispositionGestureFilter::PopGestureSequence() {
  DCHECK(Head().empty());
  state_ = GestureHandlingState();
  sequences_.pop();
}

TouchDispositionGestureFilter::GestureSequence&
TouchDispositionGestureFilter::Head() {
  DCHECK(!sequences_.empty());
  return sequences_.front();
}

TouchDispositionGestureFilter::GestureSequence&
TouchDispositionGestureFilter::Tail() {
  DCHECK(!sequences_.empty());
  return sequences_.back();
}

// TouchDispositionGestureFilter::GestureHandlingState

TouchDispositionGestureFilter::GestureHandlingState::GestureHandlingState()
    : start_touch_consumed_(false),
      current_touch_consumed_(false) {}

void TouchDispositionGestureFilter::GestureHandlingState::OnTouchEventAck(
    bool event_consumed,
    bool is_touch_start_event) {
  current_touch_consumed_ = event_consumed;
  if (event_consumed && is_touch_start_event)
    start_touch_consumed_ = true;
}

bool TouchDispositionGestureFilter::GestureHandlingState::Filter(
    EventType gesture_type) {
  DispositionHandlingInfo disposition_handling_info =
      GetDispositionHandlingInfo(gesture_type);

  int required_touches = disposition_handling_info.required_touches;
  EventType antecedent_event_type =
      disposition_handling_info.antecedent_event_type;
  if ((required_touches & RT_START && start_touch_consumed_) ||
      (required_touches & RT_CURRENT && current_touch_consumed_) ||
      (antecedent_event_type != EventType::kUnknown &&
       last_gesture_of_type_dropped_.has_bit(
           GetGestureTypeIndex(antecedent_event_type)))) {
    last_gesture_of_type_dropped_.mark_bit(GetGestureTypeIndex(gesture_type));
    any_gesture_of_type_dropped_.mark_bit(GetGestureTypeIndex(gesture_type));
    return true;
  }
  last_gesture_of_type_dropped_.clear_bit(GetGestureTypeIndex(gesture_type));
  return false;
}

bool TouchDispositionGestureFilter::GestureHandlingState::
    HasFilteredGestureType(EventType gesture_type) const {
  return any_gesture_of_type_dropped_.has_bit(
      GetGestureTypeIndex(gesture_type));
}

}  // namespace content
