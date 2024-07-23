// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/mojom/event_mojom_traits.h"

#include "base/time/time.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_event_details.h"
#include "ui/events/ipc/ui_events_param_traits_macros.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/mojom/event_constants.mojom.h"
#include "ui/latency/mojom/latency_info_mojom_traits.h"

namespace mojo {

namespace {

ui::mojom::LocationDataPtr CreateLocationData(const ui::LocatedEvent* event) {
  ui::mojom::LocationDataPtr location_data(ui::mojom::LocationData::New());
  location_data->relative_location = event->location_f();
  location_data->root_location = event->root_location_f();
  return location_data;
}

bool ReadScrollData(ui::mojom::EventDataView* event,
                    base::TimeTicks time_stamp,
                    EventUniquePtr* out) {
  ui::mojom::ScrollDataPtr scroll_data;
  if (!event->ReadScrollData<ui::mojom::ScrollDataPtr>(&scroll_data))
    return false;

  *out = std::make_unique<ui::ScrollEvent>(
      mojo::ConvertTo<ui::EventType>(event->action()),
      scroll_data->location->relative_location,
      scroll_data->location->root_location, time_stamp, event->flags(),
      scroll_data->x_offset, scroll_data->y_offset,
      scroll_data->x_offset_ordinal, scroll_data->y_offset_ordinal,
      scroll_data->finger_count, scroll_data->momentum_phase);
  return true;
}

bool ReadGestureData(ui::mojom::EventDataView* event,
                     base::TimeTicks time_stamp,
                     EventUniquePtr* out) {
  ui::mojom::GestureDataPtr gesture_data;
  if (!event->ReadGestureData<ui::mojom::GestureDataPtr>(&gesture_data))
    return false;

  ui::GestureEventDetails details(ConvertTo<ui::EventType>(event->action()));
  details.set_device_type(gesture_data->device_type);
  switch (details.type()) {
    case ui::EventType::kGesturePinchUpdate:
      if (!gesture_data->details->is_pinch()) {
        return false;
      }
      details.set_scale(gesture_data->details->get_pinch()->scale);
      break;
    case ui::EventType::kGestureSwipe:
      if (!gesture_data->details->is_swipe()) {
        return false;
      }
      details.set_swipe_left(gesture_data->details->get_swipe()->left);
      details.set_swipe_right(gesture_data->details->get_swipe()->right);
      details.set_swipe_up(gesture_data->details->get_swipe()->up);
      details.set_swipe_down(gesture_data->details->get_swipe()->down);
      break;
    default:
      break;
  }

  *out = std::make_unique<ui::GestureEvent>(
      gesture_data->location->relative_location.x(),
      gesture_data->location->relative_location.y(), event->flags(), time_stamp,
      details);
  return true;
}

}  // namespace

static_assert(ui::mojom::kEventFlagNone == static_cast<int32_t>(ui::EF_NONE),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagIsSynthesized ==
                  static_cast<int32_t>(ui::EF_IS_SYNTHESIZED),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagShiftDown ==
                  static_cast<int32_t>(ui::EF_SHIFT_DOWN),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagControlDown ==
                  static_cast<int32_t>(ui::EF_CONTROL_DOWN),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagAltDown ==
                  static_cast<int32_t>(ui::EF_ALT_DOWN),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagCommandDown ==
                  static_cast<int32_t>(ui::EF_COMMAND_DOWN),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagAltgrDown ==
                  static_cast<int32_t>(ui::EF_ALTGR_DOWN),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagMod3Down ==
                  static_cast<int32_t>(ui::EF_MOD3_DOWN),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagNumLockOn ==
                  static_cast<int32_t>(ui::EF_NUM_LOCK_ON),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagCapsLockOn ==
                  static_cast<int32_t>(ui::EF_CAPS_LOCK_ON),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagScrollLockOn ==
                  static_cast<int32_t>(ui::EF_SCROLL_LOCK_ON),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagLeftMouseButton ==
                  static_cast<int32_t>(ui::EF_LEFT_MOUSE_BUTTON),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagMiddleMouseButton ==
                  static_cast<int32_t>(ui::EF_MIDDLE_MOUSE_BUTTON),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagRightMouseButton ==
                  static_cast<int32_t>(ui::EF_RIGHT_MOUSE_BUTTON),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagBackMouseButton ==
                  static_cast<int32_t>(ui::EF_BACK_MOUSE_BUTTON),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagForwardMouseButton ==
                  static_cast<int32_t>(ui::EF_FORWARD_MOUSE_BUTTON),
              "EVENT_FLAGS must match");

// static
ui::mojom::EventType TypeConverter<ui::mojom::EventType,
                                   ui::EventType>::Convert(ui::EventType type) {
  switch (type) {
    case ui::EventType::kUnknown:
      return ui::mojom::EventType::UNKNOWN;
    case ui::EventType::kKeyPressed:
      return ui::mojom::EventType::KEY_PRESSED;
    case ui::EventType::kKeyReleased:
      return ui::mojom::EventType::KEY_RELEASED;
    case ui::EventType::kGestureTap:
      return ui::mojom::EventType::GESTURE_TAP;
    case ui::EventType::kGestureSwipe:
      return ui::mojom::EventType::GESTURE_SWIPE;
    case ui::EventType::kGesturePinchBegin:
      return ui::mojom::EventType::GESTURE_PINCH_BEGIN;
    case ui::EventType::kGesturePinchEnd:
      return ui::mojom::EventType::GESTURE_PINCH_END;
    case ui::EventType::kGesturePinchUpdate:
      return ui::mojom::EventType::GESTURE_PINCH_UPDATE;
    case ui::EventType::kScroll:
      return ui::mojom::EventType::SCROLL;
    case ui::EventType::kScrollFlingStart:
      return ui::mojom::EventType::SCROLL_FLING_START;
    case ui::EventType::kScrollFlingCancel:
      return ui::mojom::EventType::SCROLL_FLING_CANCEL;
    case ui::EventType::kCancelMode:
      return ui::mojom::EventType::CANCEL_MODE;
    case ui::EventType::kMousePressed:
      return ui::mojom::EventType::MOUSE_PRESSED_EVENT;
    case ui::EventType::kMouseDragged:
      return ui::mojom::EventType::MOUSE_DRAGGED_EVENT;
    case ui::EventType::kMouseReleased:
      return ui::mojom::EventType::MOUSE_RELEASED_EVENT;
    case ui::EventType::kMouseMoved:
      return ui::mojom::EventType::MOUSE_MOVED_EVENT;
    case ui::EventType::kMouseEntered:
      return ui::mojom::EventType::MOUSE_ENTERED_EVENT;
    case ui::EventType::kMouseExited:
      return ui::mojom::EventType::MOUSE_EXITED_EVENT;
    case ui::EventType::kMousewheel:
      return ui::mojom::EventType::MOUSE_WHEEL_EVENT;
    case ui::EventType::kMouseCaptureChanged:
      return ui::mojom::EventType::MOUSE_CAPTURE_CHANGED_EVENT;
    case ui::EventType::kTouchReleased:
      return ui::mojom::EventType::TOUCH_RELEASED;
    case ui::EventType::kTouchPressed:
      return ui::mojom::EventType::TOUCH_PRESSED;
    case ui::EventType::kTouchMoved:
      return ui::mojom::EventType::TOUCH_MOVED;
    case ui::EventType::kTouchCancelled:
      return ui::mojom::EventType::TOUCH_CANCELLED;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Using unknown event types closes connections:"
          << ui::EventTypeName(type);
      break;
  }
  return ui::mojom::EventType::UNKNOWN;
}

// static
ui::EventType TypeConverter<ui::EventType, ui::mojom::EventType>::Convert(
    ui::mojom::EventType type) {
  switch (type) {
    case ui::mojom::EventType::UNKNOWN:
      return ui::EventType::kUnknown;
    case ui::mojom::EventType::KEY_PRESSED:
      return ui::EventType::kKeyPressed;
    case ui::mojom::EventType::KEY_RELEASED:
      return ui::EventType::kKeyReleased;
    case ui::mojom::EventType::GESTURE_TAP:
      return ui::EventType::kGestureTap;
    case ui::mojom::EventType::GESTURE_SWIPE:
      return ui::EventType::kGestureSwipe;
    case ui::mojom::EventType::GESTURE_PINCH_BEGIN:
      return ui::EventType::kGesturePinchBegin;
    case ui::mojom::EventType::GESTURE_PINCH_END:
      return ui::EventType::kGesturePinchEnd;
    case ui::mojom::EventType::GESTURE_PINCH_UPDATE:
      return ui::EventType::kGesturePinchUpdate;
    case ui::mojom::EventType::SCROLL:
      return ui::EventType::kScroll;
    case ui::mojom::EventType::SCROLL_FLING_START:
      return ui::EventType::kScrollFlingStart;
    case ui::mojom::EventType::SCROLL_FLING_CANCEL:
      return ui::EventType::kScrollFlingCancel;
    case ui::mojom::EventType::MOUSE_PRESSED_EVENT:
      return ui::EventType::kMousePressed;
    case ui::mojom::EventType::MOUSE_DRAGGED_EVENT:
      return ui::EventType::kMouseDragged;
    case ui::mojom::EventType::MOUSE_RELEASED_EVENT:
      return ui::EventType::kMouseReleased;
    case ui::mojom::EventType::MOUSE_MOVED_EVENT:
      return ui::EventType::kMouseMoved;
    case ui::mojom::EventType::MOUSE_ENTERED_EVENT:
      return ui::EventType::kMouseEntered;
    case ui::mojom::EventType::MOUSE_EXITED_EVENT:
      return ui::EventType::kMouseExited;
    case ui::mojom::EventType::MOUSE_WHEEL_EVENT:
      return ui::EventType::kMousewheel;
    case ui::mojom::EventType::MOUSE_CAPTURE_CHANGED_EVENT:
      return ui::EventType::kMouseCaptureChanged;
    case ui::mojom::EventType::TOUCH_RELEASED:
      return ui::EventType::kTouchReleased;
    case ui::mojom::EventType::TOUCH_PRESSED:
      return ui::EventType::kTouchPressed;
    case ui::mojom::EventType::TOUCH_MOVED:
      return ui::EventType::kTouchMoved;
    case ui::mojom::EventType::TOUCH_CANCELLED:
      return ui::EventType::kTouchCancelled;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return ui::EventType::kUnknown;
}

// static
ui::mojom::EventType
StructTraits<ui::mojom::EventDataView, EventUniquePtr>::action(
    const EventUniquePtr& event) {
  return mojo::ConvertTo<ui::mojom::EventType>(event->type());
}

// static
int32_t StructTraits<ui::mojom::EventDataView, EventUniquePtr>::flags(
    const EventUniquePtr& event) {
  return event->flags();
}

// static
base::TimeTicks
StructTraits<ui::mojom::EventDataView, EventUniquePtr>::time_stamp(
    const EventUniquePtr& event) {
  return event->time_stamp();
}

// static
const ui::LatencyInfo&
StructTraits<ui::mojom::EventDataView, EventUniquePtr>::latency(
    const EventUniquePtr& event) {
  return *event->latency();
}

// static
ui::mojom::KeyDataPtr
StructTraits<ui::mojom::EventDataView, EventUniquePtr>::key_data(
    const EventUniquePtr& event) {
  if (!event->IsKeyEvent())
    return nullptr;

  const ui::KeyEvent* key_event = event->AsKeyEvent();
  ui::mojom::KeyDataPtr key_data(ui::mojom::KeyData::New());

  key_data->key_code = static_cast<int32_t>(key_event->key_code());
  key_data->is_char = key_event->is_char();
  key_data->dom_code = static_cast<uint32_t>(key_event->code());
  key_data->dom_key = static_cast<int32_t>(key_event->GetDomKey());
  return key_data;
}

// static
ui::mojom::MouseDataPtr
StructTraits<ui::mojom::EventDataView, EventUniquePtr>::mouse_data(
    const EventUniquePtr& event) {
  if (!event->IsMouseEvent())
    return nullptr;

  const ui::MouseEvent* mouse_event = event->AsMouseEvent();
  ui::mojom::MouseDataPtr mouse_data(ui::mojom::MouseData::New());
  mouse_data->changed_button_flags = mouse_event->changed_button_flags();
  mouse_data->pointer_details = mouse_event->pointer_details();
  mouse_data->location = CreateLocationData(mouse_event);
  if (mouse_event->IsMouseWheelEvent()) {
    mouse_data->wheel_offset = mouse_event->AsMouseWheelEvent()->offset();
    mouse_data->tick_120ths = mouse_event->AsMouseWheelEvent()->tick_120ths();
  }
  return mouse_data;
}

// static
ui::mojom::GestureDataPtr
StructTraits<ui::mojom::EventDataView, EventUniquePtr>::gesture_data(
    const EventUniquePtr& event) {
  if (!event->IsGestureEvent())
    return nullptr;

  const ui::GestureEvent* gesture_event = event->AsGestureEvent();
  ui::mojom::GestureDataPtr gesture_data(ui::mojom::GestureData::New());
  gesture_data->location = CreateLocationData(gesture_event);
  gesture_data->device_type = gesture_event->details().device_type();
  switch (event->type()) {
    case ui::EventType::kGesturePinchUpdate:
      gesture_data->details = ui::mojom::GestureDataDetails::NewPinch(
          ui::mojom::GesturePinchData::New(gesture_event->details().scale()));
      break;
    case ui::EventType::kGestureSwipe:
      gesture_data->details = ui::mojom::GestureDataDetails::NewSwipe(
          ui::mojom::GestureSwipeData::New(
              gesture_event->details().swipe_left(),
              gesture_event->details().swipe_right(),
              gesture_event->details().swipe_up(),
              gesture_event->details().swipe_down()));
      break;
    default:
      break;
  }
  return gesture_data;
}

// static
ui::mojom::ScrollDataPtr
StructTraits<ui::mojom::EventDataView, EventUniquePtr>::scroll_data(
    const EventUniquePtr& event) {
  if (!event->IsScrollEvent())
    return nullptr;

  ui::mojom::ScrollDataPtr scroll_data(ui::mojom::ScrollData::New());
  scroll_data->location = CreateLocationData(event->AsLocatedEvent());
  const ui::ScrollEvent* scroll_event = event->AsScrollEvent();
  scroll_data->x_offset = scroll_event->x_offset();
  scroll_data->y_offset = scroll_event->y_offset();
  scroll_data->x_offset_ordinal = scroll_event->x_offset_ordinal();
  scroll_data->y_offset_ordinal = scroll_event->y_offset_ordinal();
  scroll_data->finger_count = scroll_event->finger_count();
  scroll_data->momentum_phase = scroll_event->momentum_phase();
  return scroll_data;
}

// static
ui::mojom::TouchDataPtr
StructTraits<ui::mojom::EventDataView, EventUniquePtr>::touch_data(
    const EventUniquePtr& event) {
  if (!event->IsTouchEvent())
    return nullptr;

  const ui::TouchEvent* touch_event = event->AsTouchEvent();
  ui::mojom::TouchDataPtr touch_data(ui::mojom::TouchData::New());
  touch_data->may_cause_scrolling = touch_event->may_cause_scrolling();
  touch_data->hovering = touch_event->hovering();
  touch_data->location = CreateLocationData(touch_event);
  touch_data->pointer_details = touch_event->pointer_details();
  return touch_data;
}

// static
base::flat_map<std::string, std::vector<uint8_t>>
StructTraits<ui::mojom::EventDataView, EventUniquePtr>::properties(
    const EventUniquePtr& event) {
  return event->properties() ? *(event->properties()) : ui::Event::Properties();
}

// static
bool StructTraits<ui::mojom::EventDataView, EventUniquePtr>::Read(
    ui::mojom::EventDataView event,
    EventUniquePtr* out) {
  DCHECK(!out->get());

  base::TimeTicks time_stamp;
  if (!event.ReadTimeStamp(&time_stamp))
    return false;

  switch (event.action()) {
    case ui::mojom::EventType::KEY_PRESSED:
    case ui::mojom::EventType::KEY_RELEASED: {
      ui::mojom::KeyDataPtr key_data;
      if (!event.ReadKeyData<ui::mojom::KeyDataPtr>(&key_data))
        return false;

      std::optional<ui::DomKey> dom_key =
          ui::DomKey::FromBase(key_data->dom_key);
      if (!dom_key)
        return false;

      if (!key_data->is_char &&
          (key_data->key_code < 0 || key_data->key_code > 255)) {
        return false;
      }
      if (event.flags() > ui::EF_MAX_KEY_EVENT_FLAGS_VALUE)
        return false;

      const ui::KeyboardCode key_code =
          static_cast<ui::KeyboardCode>(key_data->key_code);
      // Deserialization uses UsbKeycodeToDomCode() rather than a direct cast
      // to ensure the value is valid. Invalid values are mapped to
      // DomCode::NONE.
      const ui::DomCode dom_code =
          ui::KeycodeConverter::UsbKeycodeToDomCode(key_data->dom_code);
      const ui::EventType event_type =
          (event.action() == ui::mojom::EventType::KEY_PRESSED)
              ? ui::EventType::kKeyPressed
              : ui::EventType::kKeyReleased;
      *out = std::make_unique<ui::KeyEvent>(event_type, key_code, dom_code,
                                            event.flags(), *dom_key, time_stamp,
                                            key_data->is_char);
      break;
    }
    case ui::mojom::EventType::GESTURE_TAP:
    case ui::mojom::EventType::GESTURE_SWIPE:
    case ui::mojom::EventType::GESTURE_PINCH_BEGIN:
    case ui::mojom::EventType::GESTURE_PINCH_END:
    case ui::mojom::EventType::GESTURE_PINCH_UPDATE:
      if (!ReadGestureData(&event, time_stamp, out))
        return false;
      break;
    case ui::mojom::EventType::SCROLL:
      if (!ReadScrollData(&event, time_stamp, out))
        return false;
      break;
    case ui::mojom::EventType::SCROLL_FLING_START:
    case ui::mojom::EventType::SCROLL_FLING_CANCEL:
      // SCROLL_FLING_START/CANCEL is represented by a GestureEvent if
      // EF_FROM_TOUCH is set.
      if ((event.flags() & ui::EF_FROM_TOUCH) != 0) {
        if (!ReadGestureData(&event, time_stamp, out))
          return false;
      } else if (!ReadScrollData(&event, time_stamp, out)) {
        return false;
      }
      break;
    case ui::mojom::EventType::CANCEL_MODE:
      *out = std::make_unique<ui::CancelModeEvent>();
      break;
    case ui::mojom::EventType::MOUSE_PRESSED_EVENT:
    case ui::mojom::EventType::MOUSE_RELEASED_EVENT:
    case ui::mojom::EventType::MOUSE_DRAGGED_EVENT:
    case ui::mojom::EventType::MOUSE_MOVED_EVENT:
    case ui::mojom::EventType::MOUSE_ENTERED_EVENT:
    case ui::mojom::EventType::MOUSE_EXITED_EVENT:
    case ui::mojom::EventType::MOUSE_WHEEL_EVENT:
    case ui::mojom::EventType::MOUSE_CAPTURE_CHANGED_EVENT: {
      ui::mojom::MouseDataPtr mouse_data;
      if (!event.ReadMouseData(&mouse_data))
        return false;

      std::unique_ptr<ui::MouseEvent> mouse_event;
      if (event.action() == ui::mojom::EventType::MOUSE_WHEEL_EVENT) {
        mouse_event = std::make_unique<ui::MouseWheelEvent>(
            mouse_data->wheel_offset, mouse_data->location->relative_location,
            mouse_data->location->root_location, time_stamp, event.flags(),
            mouse_data->changed_button_flags, mouse_data->tick_120ths);
      } else {
        mouse_event = std::make_unique<ui::MouseEvent>(
            mojo::ConvertTo<ui::EventType>(event.action()),
            mouse_data->location->relative_location,
            mouse_data->location->root_location, time_stamp, event.flags(),
            mouse_data->changed_button_flags, mouse_data->pointer_details);
      }
      *out = std::move(mouse_event);
      break;
    }
    case ui::mojom::EventType::TOUCH_RELEASED:
    case ui::mojom::EventType::TOUCH_PRESSED:
    case ui::mojom::EventType::TOUCH_MOVED:
    case ui::mojom::EventType::TOUCH_CANCELLED: {
      ui::mojom::TouchDataPtr touch_data;
      if (!event.ReadTouchData(&touch_data))
        return false;
      std::unique_ptr<ui::TouchEvent> touch_event =
          std::make_unique<ui::TouchEvent>(
              mojo::ConvertTo<ui::EventType>(event.action()),
              touch_data->location->relative_location,
              touch_data->location->root_location, time_stamp,
              touch_data->pointer_details, event.flags());
      touch_event->set_may_cause_scrolling(touch_data->may_cause_scrolling);
      touch_event->set_hovering(touch_data->hovering);
      *out = std::move(touch_event);
      break;
    }
    case ui::mojom::EventType::UNKNOWN:
      NOTREACHED_IN_MIGRATION()
          << "Using unknown event types closes connections";
      return false;
  }

  if (!out->get())
    return false;

  if (!event.ReadLatency((*out)->latency()))
    return false;

  std::optional<ui::Event::Properties> properties;
  if (!event.ReadProperties(&properties))
    return false;
  if (properties && !properties->empty())
    (*out)->SetProperties(std::move(*properties));

  return true;
}

// static
bool StructTraits<ui::mojom::PointerDetailsDataView, ui::PointerDetails>::Read(
    ui::mojom::PointerDetailsDataView data,
    ui::PointerDetails* out) {
  if (!data.ReadPointerType(&out->pointer_type))
    return false;
  out->radius_x = data.radius_x();
  out->radius_y = data.radius_y();
  out->force = data.force();
  out->tilt_x = data.tilt_x();
  out->tilt_y = data.tilt_y();
  out->tangential_pressure = data.tangential_pressure();
  out->twist = data.twist();
  out->id = data.id();
  out->offset.set_x(data.offset_x());
  out->offset.set_y(data.offset_y());
  return true;
}

}  // namespace mojo
