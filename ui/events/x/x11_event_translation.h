// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_X_X11_EVENT_TRANSLATION_H_
#define UI_EVENTS_X_X11_EVENT_TRANSLATION_H_

#include <memory>

#include "base/component_export.h"
#include "ui/events/events_export.h"
#include "ui/gfx/x/x11_types.h"

namespace ui {

class Event;
class KeyEvent;
class MouseEvent;
class MouseWheelEvent;
class TouchEvent;

// Translates a XEvent into a ui::Event.
EVENTS_EXPORT std::unique_ptr<Event> BuildEventFromXEvent(const XEvent& xev);

// Convenience function that translates XEvent into ui::KeyEvent
EVENTS_EXPORT std::unique_ptr<KeyEvent> BuildKeyEventFromXEvent(
    const XEvent& xev);

// Convenience function that translates XEvent into ui::MouseEvent
EVENTS_EXPORT std::unique_ptr<MouseEvent> BuildMouseEventFromXEvent(
    const XEvent& xev);

// Convenience function that translates XEvent into ui::MouseWheelEvent
EVENTS_EXPORT std::unique_ptr<MouseWheelEvent> BuildMouseWheelEventFromXEvent(
    const XEvent& xev);

// Convenience function that translates XEvent into ui::TouchEvent
EVENTS_EXPORT std::unique_ptr<TouchEvent> BuildTouchEventFromXEvent(
    const XEvent& xev);

}  // namespace ui

#endif  // UI_EVENTS_X_X11_EVENT_TRANSLATION_H_
