// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_WINDOW_EVENT_FILTER_LINUX_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_WINDOW_EVENT_FILTER_LINUX_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/hit_test.h"
#include "ui/events/event_handler.h"
#include "ui/views/views_export.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class LocatedEvent;
class MouseEvent;
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

  // Called when the user clicked the maximize button.
  void OnClickedMaximizeButton(ui::MouseEvent* event);

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
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_WINDOW_EVENT_FILTER_LINUX_H_
