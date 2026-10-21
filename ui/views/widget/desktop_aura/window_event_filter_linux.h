// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_WINDOW_EVENT_FILTER_LINUX_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_WINDOW_EVENT_FILTER_LINUX_H_

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/hit_test.h"
#include "ui/events/event_handler.h"
#include "ui/events/pointer_details.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/views_export.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class EventTarget;
class LocatedEvent;
class MouseEvent;
class TouchEvent;
class WmMoveResizeHandler;
}  // namespace ui

namespace views {

class DesktopWindowTreeHostPlatform;

// An EventFilter that sets properties on native windows. Uses
// WmMoveResizeHandler to dispatch move/resize requests.
class VIEWS_EXPORT WindowEventFilterLinux : public ui::EventHandler {
 public:
  WindowEventFilterLinux(
      DesktopWindowTreeHostPlatform* desktop_window_tree_host,
      ui::WmMoveResizeHandler* handler);

  WindowEventFilterLinux(const WindowEventFilterLinux&) = delete;
  WindowEventFilterLinux& operator=(const WindowEventFilterLinux&) = delete;

  ~WindowEventFilterLinux() override;

  void HandleLocatedEventWithHitTest(int hit_test, ui::LocatedEvent* event);

 private:
  bool HandleMouseEventWithHitTest(int hit_test, ui::MouseEvent* event);

  // Called when the user clicked the caption area.
  void OnClickedCaption(ui::MouseEvent* event, int previous_click_component);

  // Clears `begin_drag_location_` and releases the associated data drag
  // suppression.
  void ResetCaptionHold();

  // Tracks touches pressed on the caption or a resize border and suppresses
  // data drags while they are held.
  void HandleTouchEventWithHitTest(int hit_test, ui::TouchEvent* event);

  // Clears `move_resize_touch_id_` and releases the associated data drag
  // suppression.
  void ResetMoveResizeTouch();

  void MaybeToggleMaximizedState(aura::Window* window);

  // Dispatches a message to the window manager to tell it to act as if a border
  // or titlebar drag occurred with left mouse click. In case of X11, a
  // _NET_WM_MOVERESIZE message is sent.
  void MaybeDispatchHostWindowDragMovement(int hittest,
                                           ui::LocatedEvent* event);

  // A signal to lower an attached to this filter window to the bottom of the
  // stack.
  void LowerWindow();

  // ui::EventHandler overrides:
  void OnGestureEvent(ui::GestureEvent* event) override;

  const raw_ptr<DesktopWindowTreeHostPlatform> desktop_window_tree_host_;

  // A handler, which is used for interactive move/resize events if set and
  // unless MaybeDispatchHostWindowDragMovement is overridden by a derived
  // class.
  const raw_ptr<ui::WmMoveResizeHandler> handler_;

  // The non-client component for the target of a MouseEvent. Mouse events can
  // be destructive to the window tree, which can cause the component of a
  // ui::EF_IS_DOUBLE_CLICK event to no longer be the same as that of the
  // initial click. Acting on a double click should only occur for matching
  // components.
  int click_component_ = HTNOWHERE;

  // Capture the beginning of a potential drag on caption left-click.
  // The drag will only happen if the system-provided mouse move threshold is
  // met.
  std::optional<gfx::Point> begin_drag_location_;

  // Suppresses renderer-initiated data drags while `begin_drag_location_` is
  // set, so that a data drag cannot latch onto the user's caption click-hold.
  // Must be reset whenever `begin_drag_location_` is reset.
  base::ScopedClosureRunner caption_hold_drag_suppression_;

  // The pointer ID of a touch that was pressed on the caption or a resize
  // border and has not yet been released or cancelled.
  std::optional<ui::PointerId> move_resize_touch_id_;

  // Suppresses renderer-initiated data drags while `move_resize_touch_id_` is
  // set, so that a data drag cannot latch onto the user's caption or resize
  // border touch, either before the touch slop is crossed or after the move or
  // resize has been handed off to the compositor. Must be reset whenever
  // `move_resize_touch_id_` is reset.
  base::ScopedClosureRunner move_resize_touch_drag_suppression_;

  base::ScopedObservation<ui::EventTarget, ui::EventHandler>
      window_observation_{this};
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_WINDOW_EVENT_FILTER_LINUX_H_
