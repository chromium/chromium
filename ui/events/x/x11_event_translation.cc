// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/x/x11_event_translation.h"

#include <vector>

#include "base/check.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/pointer_details.h"
#include "ui/events/types/event_type.h"
#include "ui/events/x/events_x_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

namespace {

int XkbGroupForCoreState(int state) {
  return (state >> 13) & 0x3;
}

// In X11 touch events, a new tracking_id/slot mapping is set up for each new
// event (see |ui::GetTouchIdFromXEvent| function), which needs to be cleared
// at destruction time for corresponding release/cancel events. In this
// particular case, ui::TouchEvent class is extended so that dtor can be
// overridden in order to implement this platform-specific behavior.
class TouchEventX11 : public ui::TouchEvent {
 public:
  TouchEventX11(EventType type,
                gfx::Point location,
                base::TimeTicks timestamp,
                const PointerDetails& pointer_details)
      : TouchEvent(type, location, timestamp, pointer_details) {}

  ~TouchEventX11() override {
    if (type() == EventType::kTouchReleased ||
        type() == EventType::kTouchCancelled) {
      TouchFactory::GetInstance()->ReleaseSlot(pointer_details().id);
    }
  }

  // Event:
  std::unique_ptr<Event> Clone() const override {
    return std::make_unique<TouchEventX11>(*this);
  }
};

Event::Properties GetEventPropertiesFromXEvent(EventType type,
                                               const x11::Event& x11_event) {
  using Values = std::vector<uint8_t>;
  Event::Properties properties;
  if (type == EventType::kKeyPressed || type == EventType::kKeyReleased) {
    auto* key = x11_event.As<x11::KeyEvent>();

    // Keyboard group
    auto state = static_cast<uint32_t>(key->state);
    properties.emplace(kPropertyKeyboardState,
                       Values{
                           static_cast<uint8_t>(state),
                           static_cast<uint8_t>(state >> 8),
                           static_cast<uint8_t>(state >> 16),
                           static_cast<uint8_t>(state >> 24),
                       });

    uint8_t group = XkbGroupForCoreState(state);
    properties.emplace(kPropertyKeyboardGroup, Values{group});

    // Hardware keycode
    uint8_t hw_keycode = static_cast<uint8_t>(key->detail);
    properties.emplace(kPropertyKeyboardHwKeyCode, Values{hw_keycode});

    // IBus-/fctix-GTK specific flags
    uint8_t ime_flags = (state >> kPropertyKeyboardImeFlagOffset) &
                        kPropertyKeyboardImeFlagMask;
    if (ime_flags) {
      SetKeyboardImeFlagProperty(&properties, ime_flags);
    }
  } else if (type == EventType::kMouseExited) {
    // NotifyVirtual events are created for intermediate windows that the
    // pointer crosses through. These occur when middle clicking.
    // Change these into mouse move events.
    auto* crossing = x11_event.As<x11::CrossingEvent>();
    bool crossing_intermediate_window =
        crossing->detail == x11::NotifyDetail::Virtual;
    if (crossing_intermediate_window) {
      properties.emplace(kPropertyMouseCrossedIntermediateWindow,
                         crossing_intermediate_window);
    }
  }
  return properties;
}

std::unique_ptr<KeyEvent> CreateKeyEvent(EventType event_type,
                                         const x11::Event& x11_event) {
  KeyboardCode key_code = KeyboardCodeFromXKeyEvent(x11_event);
  int event_flags = EventFlagsFromXEvent(x11_event);

  // In Ozone builds, keep DomCode/DomKey unset, so they are extracted lazily
  // in KeyEvent::ApplyLayout() which makes it possible for CrOS/Linux, for
  // example, to support host system keyboard layouts.
  std::unique_ptr<KeyEvent> event =
#if BUILDFLAG(IS_CHROMEOS_ASH)
      std::make_unique<KeyEvent>(event_type, key_code, event_flags,
                                 EventTimeFromXEvent(x11_event));
#else
      std::make_unique<KeyEvent>(
          event_type, key_code, CodeFromXEvent(x11_event), event_flags,
          GetDomKeyFromXEvent(x11_event), EventTimeFromXEvent(x11_event));
#endif

  DCHECK(event);
  event->SetProperties(GetEventPropertiesFromXEvent(event_type, x11_event));
  event->InitializeNative();
  return event;
}

void SetEventSourceDeviceId(MouseEvent* event, const x11::Event& xev) {
  DCHECK(event);
  if (auto* xiev = xev.As<x11::Input::DeviceEvent>())
    event->set_source_device_id(static_cast<uint16_t>(xiev->sourceid));
}

std::unique_ptr<MouseEvent> CreateMouseEvent(EventType type,
                                             const x11::Event& x11_event) {
  if (auto* crossing = x11_event.As<x11::CrossingEvent>()) {
    // Ignore EventNotify and LeaveNotify events from children of the window.
    if (crossing->detail == x11::NotifyDetail::Inferior) {
      return nullptr;
    }
    // Ignore LeaveNotify grab events with a detail of Ancestor.  Some WMs
    // will grab the container window during a click.  Don't generate a
    // kMouseExited event in this case.
    // https://crbug.com/41314367
    if (crossing->detail == x11::NotifyDetail::Ancestor &&
        crossing->mode == x11::NotifyMode::Grab &&
        crossing->opcode == x11::CrossingEvent::LeaveNotify) {
      return nullptr;
    }
  }

  PointerDetails details = GetStylusPointerDetailsFromXEvent(x11_event);
  auto event = std::make_unique<MouseEvent>(
      type, EventLocationFromXEvent(x11_event),
      EventSystemLocationFromXEvent(x11_event), EventTimeFromXEvent(x11_event),
      EventFlagsFromXEvent(x11_event),
      GetChangedMouseButtonFlagsFromXEvent(x11_event), details);

  DCHECK(event);
  SetEventSourceDeviceId(event.get(), x11_event);
  event->SetProperties(GetEventPropertiesFromXEvent(type, x11_event));
  event->InitializeNative();
  return event;
}

std::unique_ptr<MouseWheelEvent> CreateMouseWheelEvent(
    const x11::Event& x11_event) {
  int button_flags = x11_event.As<x11::Input::DeviceEvent>()
                         ? GetChangedMouseButtonFlagsFromXEvent(x11_event)
                         : 0;
  auto event = std::make_unique<MouseWheelEvent>(
      GetMouseWheelOffsetFromXEvent(x11_event),
      EventLocationFromXEvent(x11_event),
      EventSystemLocationFromXEvent(x11_event), EventTimeFromXEvent(x11_event),
      EventFlagsFromXEvent(x11_event), button_flags);

  DCHECK(event);
  event->InitializeNative();
  return event;
}

std::unique_ptr<TouchEvent> CreateTouchEvent(EventType type,
                                             const x11::Event& xev) {
  auto event = std::make_unique<TouchEventX11>(
      type, EventLocationFromXEvent(xev), EventTimeFromXEvent(xev),
      GetTouchPointerDetailsFromXEvent(xev));
#if BUILDFLAG(IS_OZONE)
  // Touch events don't usually have |root_location| set differently than
  // |location|, since there is a touch device to display association, but
  // this doesn't happen in Ozone X11.
  event->set_root_location(EventSystemLocationFromXEvent(xev));
#endif
  return event;
}

std::unique_ptr<ScrollEvent> CreateScrollEvent(EventType type,
                                               const x11::Event& xev) {
  float x_offset, y_offset, x_offset_ordinal, y_offset_ordinal;
  int finger_count = 0;

  if (type == EventType::kScroll) {
    GetScrollOffsetsFromXEvent(xev, &x_offset, &y_offset, &x_offset_ordinal,
                               &y_offset_ordinal, &finger_count);
  } else {
    GetFlingDataFromXEvent(xev, &x_offset, &y_offset, &x_offset_ordinal,
                           &y_offset_ordinal, nullptr);
  }
  // When lifting up fingers x_offset and y_offset both have the value 0
  // If this is the case EventType::kScrollFlingStart needs to be emitted, in
  // order to trigger touchpad overscroll navigation gesture. x_offset and
  // y_offset should not be manipulated, however, since some X11 drivers such as
  // synaptics simulate the fling themselves
  if (!x_offset && !y_offset) {
    type = EventType::kScrollFlingStart;
  }

  auto event = std::make_unique<ScrollEvent>(
      type, EventLocationFromXEvent(xev), EventTimeFromXEvent(xev),
      EventFlagsFromXEvent(xev), x_offset, y_offset, x_offset_ordinal,
      y_offset_ordinal, finger_count);

  DCHECK(event);
  // We need to filter zero scroll offset here. Because MouseWheelEventQueue
  // assumes we'll never get a zero scroll offset event and we need delta to
  // determine which element to scroll on phaseBegan.
  return (event->x_offset() != 0.0 || event->y_offset() != 0.0 ||
          event->type() == EventType::kScrollFlingStart)
             ? std::move(event)
             : nullptr;
}

// Translates XI2 XEvent into a ui::Event.
std::unique_ptr<ui::Event> TranslateFromXI2Event(const x11::Event& xev,
                                                 EventType event_type) {
  switch (event_type) {
    case EventType::kKeyPressed:
    case EventType::kKeyReleased:
      return CreateKeyEvent(event_type, xev);
    case EventType::kMousePressed:
    case EventType::kMouseReleased:
    case EventType::kMouseMoved:
    case EventType::kMouseDragged:
    case EventType::kMouseEntered:
    case EventType::kMouseExited:
      return CreateMouseEvent(event_type, xev);
    case EventType::kMousewheel:
      return CreateMouseWheelEvent(xev);
    case EventType::kScrollFlingStart:
    case EventType::kScrollFlingCancel:
    case EventType::kScroll:
      return CreateScrollEvent(event_type, xev);
    case EventType::kTouchMoved:
    case EventType::kTouchPressed:
    case EventType::kTouchCancelled:
    case EventType::kTouchReleased:
      return CreateTouchEvent(event_type, xev);
    case EventType::kUnknown:
      return nullptr;
    default:
      break;
  }
  return nullptr;
}

std::unique_ptr<Event> TranslateFromXEvent(const x11::Event& xev) {
  EventType event_type = EventTypeFromXEvent(xev);
  if (xev.As<x11::CrossingEvent>() || xev.As<x11::MotionNotifyEvent>())
    return CreateMouseEvent(event_type, xev);
  if (xev.As<x11::KeyEvent>())
    return CreateKeyEvent(event_type, xev);
  if (xev.As<x11::ButtonEvent>()) {
    switch (event_type) {
      case EventType::kMousewheel:
        return CreateMouseWheelEvent(xev);
      case EventType::kMousePressed:
      case EventType::kMouseReleased:
        return CreateMouseEvent(event_type, xev);
      case EventType::kUnknown:
        // No event is created for X11-release events for mouse-wheel
        // buttons.
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }
  if (xev.As<x11::Input::DeviceEvent>())
    return TranslateFromXI2Event(xev, event_type);
  return nullptr;
}

}  // namespace

// Translates a XEvent into a ui::Event.
std::unique_ptr<Event> BuildEventFromXEvent(const x11::Event& xev) {
  auto event = TranslateFromXEvent(xev);
  if (event)
    ui::ComputeEventLatencyOS(event.get());
  return event;
}

// Convenience function that translates XEvent into ui::KeyEvent
std::unique_ptr<KeyEvent> BuildKeyEventFromXEvent(const x11::Event& xev) {
  auto event = BuildEventFromXEvent(xev);
  if (!event || !event->IsKeyEvent())
    return nullptr;
  return std::unique_ptr<KeyEvent>{event.release()->AsKeyEvent()};
}

// Convenience function that translates XEvent into ui::MouseEvent
std::unique_ptr<MouseEvent> BuildMouseEventFromXEvent(const x11::Event& xev) {
  auto event = BuildEventFromXEvent(xev);
  if (!event || !event->IsMouseEvent())
    return nullptr;
  return std::unique_ptr<MouseEvent>{event.release()->AsMouseEvent()};
}

// Convenience function that translates XEvent into ui::TouchEvent
std::unique_ptr<TouchEvent> BuildTouchEventFromXEvent(const x11::Event& xev) {
  auto event = BuildEventFromXEvent(xev);
  if (!event || !event->IsTouchEvent())
    return nullptr;
  return std::unique_ptr<TouchEvent>{event.release()->AsTouchEvent()};
}

// Convenience function that translates XEvent into ui::MouseWheelEvent
std::unique_ptr<MouseWheelEvent> BuildMouseWheelEventFromXEvent(
    const x11::Event& xev) {
  auto event = BuildEventFromXEvent(xev);
  if (!event || !event->IsMouseWheelEvent())
    return nullptr;
  return std::unique_ptr<MouseWheelEvent>{event.release()->AsMouseWheelEvent()};
}

}  // namespace ui
