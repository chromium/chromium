// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_WINDOW_EVENT_FILTER_LACROS_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_WINDOW_EVENT_FILTER_LACROS_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/hit_test.h"
#include "ui/events/event_handler.h"
#include "ui/views/views_export.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class GestureEvent;
class MouseEvent;
class LocatedEvent;
class WmMoveResizeHandler;
}  // namespace ui

namespace views {

class DesktopWindowTreeHostPlatform;

// An EventFilter that sets properties on native windows. Uses
// WmMoveResizeHandler to dispatch move/resize requests.
class VIEWS_EXPORT WindowEventFilterLacros : public ui::EventHandler {
 public:
  WindowEventFilterLacros(
      DesktopWindowTreeHostPlatform* desktop_window_tree_host,
      ui::WmMoveResizeHandler* handler);

  WindowEventFilterLacros(const WindowEventFilterLacros&) = delete;
  WindowEventFilterLacros& operator=(const WindowEventFilterLacros&) = delete;

  ~WindowEventFilterLacros() override;

  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

 private:
  void MaybeToggleMaximizedState(aura::Window* window);

  // Dispatches a message to the window manager to tell it to act as if a border
  // or titlebar drag occurred with left mouse click. In case of X11, a
  // _NET_WM_MOVERESIZE message is sent.
  void MaybeDispatchHostWindowDragMovement(int hittest,
                                           ui::LocatedEvent* event);

  const raw_ptr<DesktopWindowTreeHostPlatform> desktop_window_tree_host_;

  int previous_pressed_component_ = HTNOWHERE;

  // A handler, which is used for interactive move/resize events if set and
  // unless MaybeDispatchHostWindowDragMovement is overridden by a derived
  // class.
  const raw_ptr<ui::WmMoveResizeHandler> handler_;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_WINDOW_EVENT_FILTER_LACROS_H_
