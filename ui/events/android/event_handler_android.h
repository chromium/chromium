// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ANDROID_EVENT_HANDLER_ANDROID_H_
#define UI_EVENTS_ANDROID_EVENT_HANDLER_ANDROID_H_

#include <optional>

#include "base/time/time.h"
#include "ui/events/events_export.h"

namespace gfx {
class Rect;
}

namespace ui {

class DragEventAndroid;
class GestureEventAndroid;
class KeyEventAndroid;
class MotionEventAndroid;

// Dispatches events to appropriate targets. The default implementations of
// all of the specific handlers do nothing. Implementations should set
// themselves to the ViewAndroid in the view tree to get the calls routed.
// Use bool return type to stop propagating the call i.e. overriden method
// should return true to indicate that the event was handled and stop
// the processing.
class EVENTS_EXPORT EventHandlerAndroid {
 public:
  virtual bool OnDragEvent(const DragEventAndroid& event);
  virtual bool OnTouchEvent(const MotionEventAndroid& event);
  virtual bool OnMouseEvent(const MotionEventAndroid& event);
  virtual bool OnMouseWheelEvent(const MotionEventAndroid& event);
  virtual bool OnGestureEvent(const GestureEventAndroid& event);
  virtual void OnSizeChanged();
  virtual void OnPhysicalBackingSizeChanged(
      std::optional<base::TimeDelta> deadline_override);
  virtual void OnBrowserControlsHeightChanged();
  virtual void OnControlsResizeViewChanged();

  virtual bool OnGenericMotionEvent(const MotionEventAndroid& event);
  virtual bool OnKeyUp(const KeyEventAndroid& event);
  virtual bool DispatchKeyEvent(const KeyEventAndroid& event);
  virtual bool ScrollBy(float delta_x, float delta_y);
  virtual bool ScrollTo(float x, float y);
  virtual void NotifyVirtualKeyboardOverlayRect(const gfx::Rect& keyboard_rect);
};

}  // namespace ui

#endif  // UI_EVENTS_ANDROID_EVENT_HANDLER_ANDROID_H_
