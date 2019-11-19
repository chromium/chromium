// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"

namespace ui {

base::TimeTicks EventTimeFromNative(const PlatformEvent& native_event) {
  const ui::Event* event = static_cast<const ui::Event*>(native_event);
  return event->time_stamp();
}

int EventFlagsFromNative(const PlatformEvent& native_event) {
  const ui::Event* event = static_cast<const ui::Event*>(native_event);
  return event->flags();
}

EventType EventTypeFromNative(const PlatformEvent& native_event) {
  const ui::Event* event = static_cast<const ui::Event*>(native_event);
  return event->type();
}

gfx::Point EventSystemLocationFromNative(const PlatformEvent& native_event) {
  const ui::LocatedEvent* e =
      static_cast<const ui::LocatedEvent*>(native_event);
  DCHECK(e->IsMouseEvent() || e->IsTouchEvent() || e->IsGestureEvent() ||
         e->IsScrollEvent());
  return e->location();
}

gfx::PointF EventLocationFromNative(const PlatformEvent& native_event) {
  const ui::LocatedEvent* e =
      static_cast<const ui::LocatedEvent*>(native_event);
  DCHECK(e->IsMouseEvent() || e->IsTouchEvent() || e->IsGestureEvent() ||
         e->IsScrollEvent());
  return e->location_f();
}

int GetChangedMouseButtonFlagsFromNative(const PlatformEvent& native_event) {
  const ui::MouseEvent* event =
      static_cast<const ui::MouseEvent*>(native_event);
  DCHECK(event->IsMouseEvent() || event->IsScrollEvent());
  return event->changed_button_flags();
}

PointerDetails GetMousePointerDetailsFromNative(
    const PlatformEvent& native_event) {
  const ui::MouseEvent* event =
      static_cast<const ui::MouseEvent*>(native_event);
  DCHECK(event->IsMouseEvent() || event->IsScrollEvent());
  PointerDetails pointer_detail = event->pointer_details();
  pointer_detail.id = kPointerIdMouse;
  return pointer_detail;
}

KeyboardCode KeyboardCodeFromNative(const PlatformEvent& native_event) {
  const ui::KeyEvent* event = static_cast<const ui::KeyEvent*>(native_event);
  DCHECK(event->IsKeyEvent());
  return event->key_code();
}

DomCode CodeFromNative(const PlatformEvent& native_event) {
  const ui::KeyEvent* event = static_cast<const ui::KeyEvent*>(native_event);
  DCHECK(event->IsKeyEvent());
  return event->code();
}

bool IsCharFromNative(const PlatformEvent& native_event) {
  const ui::KeyEvent* event = static_cast<const ui::KeyEvent*>(native_event);
  DCHECK(event->IsKeyEvent());
  return event->is_char();
}

gfx::Vector2d GetMouseWheelOffset(const PlatformEvent& native_event) {
  const ui::MouseWheelEvent* event =
      static_cast<const ui::MouseWheelEvent*>(native_event);
  DCHECK(event->type() == ET_MOUSEWHEEL);
  return event->offset();
}

PlatformEvent CopyNativeEvent(const PlatformEvent& event) {
  return NULL;
}

void ReleaseCopiedNativeEvent(const PlatformEvent& event) {}

// TODO(687724): Will remove all GetTouchId functions.
int GetTouchId(const PlatformEvent& native_event) {
  const ui::TouchEvent* event =
      static_cast<const ui::TouchEvent*>(native_event);
  DCHECK(event->IsTouchEvent());
  return event->pointer_details().id;
}

PointerDetails GetTouchPointerDetailsFromNative(
    const PlatformEvent& native_event) {
  const ui::TouchEvent* event =
      static_cast<const ui::TouchEvent*>(native_event);
  DCHECK(event->IsTouchEvent());
  return event->pointer_details();
}

bool GetScrollOffsets(const PlatformEvent& native_event,
                      float* x_offset,
                      float* y_offset,
                      float* x_offset_ordinal,
                      float* y_offset_ordinal,
                      int* finger_count,
                      EventMomentumPhase* momentum_phase) {
  const ui::ScrollEvent* event =
      static_cast<const ui::ScrollEvent*>(native_event);
  DCHECK(event->IsScrollEvent());
  if (x_offset)
    *x_offset = event->x_offset();
  if (y_offset)
    *y_offset = event->y_offset();
  if (x_offset_ordinal)
    *x_offset_ordinal = event->x_offset_ordinal();
  if (y_offset_ordinal)
    *y_offset_ordinal = event->y_offset_ordinal();
  if (finger_count)
    *finger_count = event->finger_count();
  if (momentum_phase)
    *momentum_phase = event->momentum_phase();

  return true;
}

bool GetFlingData(const PlatformEvent& native_event,
                  float* vx,
                  float* vy,
                  float* vx_ordinal,
                  float* vy_ordinal,
                  bool* is_cancel) {
  const ui::ScrollEvent* event =
      static_cast<const ui::ScrollEvent*>(native_event);
  DCHECK(event->IsScrollEvent());
  if (vx)
    *vx = event->x_offset();
  if (vy)
    *vy = event->y_offset();
  if (vx_ordinal)
    *vx_ordinal = event->x_offset_ordinal();
  if (vy_ordinal)
    *vy_ordinal = event->y_offset_ordinal();
  if (is_cancel)
    *is_cancel = event->type() == ET_SCROLL_FLING_CANCEL;

  return true;
}

}  // namespace ui
