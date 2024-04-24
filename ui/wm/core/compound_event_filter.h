// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_COMPOUND_EVENT_FILTER_H_
#define UI_WM_CORE_COMPOUND_EVENT_FILTER_H_

#include <string_view>

#include "base/component_export.h"
#include "base/observer_list.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
class GestureEvent;
class KeyEvent;
class MouseEvent;
class TouchEvent;
}

namespace wm {

// TODO(beng): This class should die. AddEventHandler() on the root Window
//             should be used instead.
// CompoundEventFilter gets all events first and can provide actions to those
// events. It implements global features such as click to activate a window and
// cursor change when moving mouse.
// Additional event filters can be added to CompoundEventFilter. Events will
// pass through those additional filters in their addition order and could be
// consumed by any of those filters. If an event is consumed by a filter, the
// rest of the filter(s) and CompoundEventFilter will not see the consumed
// event.
class COMPONENT_EXPORT(UI_WM) CompoundEventFilter : public ui::EventHandler {
 public:
  CompoundEventFilter();

  CompoundEventFilter(const CompoundEventFilter&) = delete;
  CompoundEventFilter& operator=(const CompoundEventFilter&) = delete;

  ~CompoundEventFilter() override;

  // Returns the cursor for the specified component.
  static gfx::NativeCursor CursorForWindowComponent(int window_component);

  // Returns the not-resizable cursor for the specified component.
  static gfx::NativeCursor NoResizeCursorForWindowComponent(
      int window_component);

  // Adds/removes additional event filters. This does not take ownership of
  // the EventHandler.
  // NOTE: These handlers are deprecated. Use env::AddPreTargetEventHandler etc.
  // instead.
  void AddHandler(ui::EventHandler* filter);
  void RemoveHandler(ui::EventHandler* filter);

 private:
  // Updates the cursor if the target provides a custom one, and provides
  // default resize cursors for window edges.
  void UpdateCursor(aura::Window* target, ui::MouseEvent* event);

  // Dispatches event to additional filters.
  void FilterKeyEvent(ui::KeyEvent* event);
  void FilterMouseEvent(ui::MouseEvent* event);
  void FilterTouchEvent(ui::TouchEvent* event);

  // Sets the visibility of the cursor if the event is not synthesized.
  void SetCursorVisibilityOnEvent(aura::Window* target,
                                  ui::Event* event,
                                  bool show);

  // Enables or disables mouse events if the event is not synthesized.
  void SetMouseEventsEnableStateOnEvent(aura::Window* target,
                                        ui::Event* event,
                                        bool enable);

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  std::string_view GetLogContext() const override;

  // Additional pre-target event handlers.
  base::ObserverList<ui::EventHandler, true>::Unchecked handlers_;
};

}  // namespace wm

#endif  // UI_WM_CORE_COMPOUND_EVENT_FILTER_H_
