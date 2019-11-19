// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/x/x11_event_translation.h"

#include <vector>

#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/x/events_x_utils.h"
#include "ui/gfx/x/x11.h"

namespace ui {

namespace {

Event::Properties GetEventPropertiesFromXKeyEvent(const XKeyEvent& xev) {
  using Values = std::vector<uint8_t>;
  Event::Properties properties;

  // Keyboard group
  uint8_t group = XkbGroupForCoreState(xev.state);
  properties.emplace(kPropertyKeyboardGroup, Values{group});

  // IBus-gtk specific flags
  uint8_t ibus_flags = (xev.state >> kPropertyKeyboardIBusFlagOffset) &
                       kPropertyKeyboardIBusFlagMask;
  properties.emplace(kPropertyKeyboardIBusFlag, Values{ibus_flags});

  return properties;
}

std::unique_ptr<KeyEvent> CreateKeyEvent(EventType event_type,
                                         const XEvent& xev) {
  KeyboardCode key_code = KeyboardCodeFromXKeyEvent(&xev);
  int event_flags = EventFlagsFromXEvent(xev);

  // In Ozone builds, keep DomCode/DomKey unset, so they are extracted lazily
  // in KeyEvent::ApplyLayout() which makes it possible for CrOS/Linux, for
  // example, to support host system keyboard layouts.
#if defined(USE_OZONE)
  auto event = std::make_unique<KeyEvent>(event_type, key_code, event_flags);
#else
  DomCode dom_code = CodeFromXEvent(&xev);
  DomKey dom_key = GetDomKeyFromXEvent(&xev);
  base::TimeTicks timestamp = EventTimeFromXEvent(xev);
  ValidateEventTimeClock(&timestamp);
  auto event = std::make_unique<KeyEvent>(event_type, key_code, dom_code,
                                          event_flags, dom_key, timestamp);
#endif

  DCHECK(event);
  event->SetProperties(GetEventPropertiesFromXKeyEvent(xev.xkey));
  return event;
}

std::unique_ptr<MouseEvent> CreateMouseEvent(EventType type,
                                             const XEvent& xev) {
  // Don't generate synthetic mouse move events for EnterNotify/LeaveNotify
  // from nested XWindows. https://crbug.com/792322
  bool enter_or_leave = xev.type == LeaveNotify || xev.type == EnterNotify;
  if (enter_or_leave && xev.xcrossing.detail == NotifyInferior)
    return nullptr;

  int button_flags =
      (enter_or_leave) ? GetChangedMouseButtonFlagsFromXEvent(xev) : 0;
  PointerDetails details{EventPointerType::POINTER_TYPE_MOUSE};
  auto event = std::make_unique<MouseEvent>(
      type, EventLocationFromXEvent(xev), EventSystemLocationFromXEvent(xev),
      EventTimeFromXEvent(xev), EventFlagsFromXEvent(xev), button_flags,
      details);

  DCHECK(event);
  return event;
}

std::unique_ptr<MouseWheelEvent> CreateMouseWheelEvent(const XEvent& xev) {
  int button_flags = (xev.type == GenericEvent)
                         ? GetChangedMouseButtonFlagsFromXEvent(xev)
                         : 0;
  auto event = std::make_unique<MouseWheelEvent>(
      GetMouseWheelOffsetFromXEvent(xev), EventLocationFromXEvent(xev),
      EventSystemLocationFromXEvent(xev), EventTimeFromXEvent(xev),
      EventFlagsFromXEvent(xev), button_flags);

  DCHECK(event);
  return event;
}

std::unique_ptr<TouchEvent> CreateTouchEvent(EventType type,
                                             const XEvent& xev) {
  std::unique_ptr<TouchEvent> event = std::make_unique<TouchEvent>(
      type, EventLocationFromXEvent(xev), EventTimeFromXEvent(xev),
      GetTouchPointerDetailsFromXEvent(xev));

  DCHECK(event);

  // Touch events don't usually have |root_location| set differently than
  // |location|, since there is a touch device to display association, but this
  // doesn't happen in Ozone X11.
  event->set_root_location(EventSystemLocationFromXEvent(xev));

  return event;
}

std::unique_ptr<ScrollEvent> CreateScrollEvent(EventType type,
                                               const XEvent& xev) {
  float x_offset, y_offset, x_offset_ordinal, y_offset_ordinal;
  int finger_count = 0;

  if (type == ET_SCROLL) {
    GetScrollOffsetsFromXEvent(xev, &x_offset, &y_offset, &x_offset_ordinal,
                               &y_offset_ordinal, &finger_count);
  } else {
    GetFlingDataFromXEvent(xev, &x_offset, &y_offset, &x_offset_ordinal,
                           &y_offset_ordinal, nullptr);
  }
  auto event = std::make_unique<ScrollEvent>(
      type, EventLocationFromXEvent(xev), EventTimeFromXEvent(xev),
      EventFlagsFromXEvent(xev), x_offset, y_offset, x_offset_ordinal,
      y_offset_ordinal, finger_count);

  DCHECK(event);
  return event;
}

// Translates XI2 XEvent into a ui::Event.
std::unique_ptr<ui::Event> TranslateFromXI2Event(const XEvent& xev) {
  EventType event_type = EventTypeFromXEvent(xev);
  switch (event_type) {
    case ET_KEY_PRESSED:
    case ET_KEY_RELEASED:
      return CreateKeyEvent(event_type, xev);
    case ET_MOUSE_PRESSED:
    case ET_MOUSE_RELEASED:
    case ET_MOUSE_MOVED:
    case ET_MOUSE_DRAGGED:
      return CreateMouseEvent(event_type, xev);
    case ET_MOUSEWHEEL:
      return CreateMouseWheelEvent(xev);
    case ET_SCROLL_FLING_START:
    case ET_SCROLL_FLING_CANCEL:
    case ET_SCROLL:
      return CreateScrollEvent(event_type, xev);
    case ET_TOUCH_MOVED:
    case ET_TOUCH_PRESSED:
    case ET_TOUCH_CANCELLED:
    case ET_TOUCH_RELEASED:
      return CreateTouchEvent(event_type, xev);
    case ET_UNKNOWN:
      return nullptr;
    default:
      break;
  }
  return nullptr;
}

std::unique_ptr<Event> TranslateFromXEvent(const XEvent& xev) {
  EventType event_type = EventTypeFromXEvent(xev);
  switch (xev.type) {
    case LeaveNotify:
    case EnterNotify:
      return CreateMouseEvent(event_type, xev);
    case KeyPress:
    case KeyRelease:
      return CreateKeyEvent(event_type, xev);
    case ButtonPress:
    case ButtonRelease: {
      switch (event_type) {
        case ET_MOUSEWHEEL:
          return CreateMouseWheelEvent(xev);
        case ET_MOUSE_PRESSED:
        case ET_MOUSE_RELEASED:
          return CreateMouseEvent(event_type, xev);
        case ET_UNKNOWN:
          // No event is created for X11-release events for mouse-wheel
          // buttons.
          break;
        default:
          NOTREACHED();
      }
      break;
    }
    case GenericEvent:
      return TranslateFromXI2Event(xev);
  }
  return nullptr;
}

}  // namespace

// Translates a XEvent into a ui::Event.
std::unique_ptr<Event> BuildEventFromXEvent(const XEvent& xev) {
  return TranslateFromXEvent(xev);
}

// Convenience function that translates XEvent into ui::KeyEvent
std::unique_ptr<KeyEvent> BuildKeyEventFromXEvent(const XEvent& xev) {
  auto event = BuildEventFromXEvent(xev);
  if (!event || !event->IsKeyEvent())
    return nullptr;
  return std::unique_ptr<KeyEvent>{event.release()->AsKeyEvent()};
}

// Convenience function that translates XEvent into ui::MouseEvent
std::unique_ptr<MouseEvent> BuildMouseEventFromXEvent(const XEvent& xev) {
  auto event = BuildEventFromXEvent(xev);
  if (!event || !event->IsMouseEvent())
    return nullptr;
  return std::unique_ptr<MouseEvent>{event.release()->AsMouseEvent()};
}

// Convenience function that translates XEvent into ui::TouchEvent
std::unique_ptr<TouchEvent> BuildTouchEventFromXEvent(const XEvent& xev) {
  auto event = BuildEventFromXEvent(xev);
  if (!event || !event->IsTouchEvent())
    return nullptr;
  return std::unique_ptr<TouchEvent>{event.release()->AsTouchEvent()};
}

// Convenience function that translates XEvent into ui::MouseWheelEvent
std::unique_ptr<MouseWheelEvent> BuildMouseWheelEventFromXEvent(
    const XEvent& xev) {
  auto event = BuildEventFromXEvent(xev);
  if (!event || !event->IsMouseWheelEvent())
    return nullptr;
  return std::unique_ptr<MouseWheelEvent>{event.release()->AsMouseWheelEvent()};
}

}  // namespace ui
