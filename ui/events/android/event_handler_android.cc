// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/android/event_handler_android.h"

#include "ui/gfx/geometry/rect.h"

namespace ui {

bool EventHandlerAndroid::OnTouchEvent(const MotionEventAndroid& event) {
  return false;
}

bool EventHandlerAndroid::OnMouseEvent(const MotionEventAndroid& event) {
  return false;
}

bool EventHandlerAndroid::OnMouseWheelEvent(const MotionEventAndroid& event) {
  return false;
}

bool EventHandlerAndroid::OnDragEvent(const DragEventAndroid& event) {
  return false;
}

bool EventHandlerAndroid::OnGestureEvent(const GestureEventAndroid& event) {
  return false;
}

bool EventHandlerAndroid::OnGenericMotionEvent(
    const MotionEventAndroid& event) {
  return false;
}

bool EventHandlerAndroid::OnKeyUp(const KeyEventAndroid& event) {
  return false;
}

bool EventHandlerAndroid::DispatchKeyEvent(const KeyEventAndroid& event) {
  return false;
}

bool EventHandlerAndroid::ScrollBy(float delta_x, float delta_y) {
  return false;
}

bool EventHandlerAndroid::ScrollTo(float x, float y) {
  return false;
}

void EventHandlerAndroid::OnSizeChanged() {}

void EventHandlerAndroid::OnPhysicalBackingSizeChanged(
    std::optional<base::TimeDelta> deadline_override) {}

void EventHandlerAndroid::OnBrowserControlsHeightChanged() {}

void EventHandlerAndroid::OnControlsResizeViewChanged() {}

void EventHandlerAndroid::NotifyVirtualKeyboardOverlayRect(
    const gfx::Rect& keyboard_rect) {}
}  // namespace ui
