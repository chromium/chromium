// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/compound_event_filter.h"

#include <string_view>

#include "base/check.h"
#include "base/observer_list.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/hit_test.h"
#include "ui/events/event.h"
#include "ui/wm/public/activation_client.h"

namespace wm {

////////////////////////////////////////////////////////////////////////////////
// CompoundEventFilter, public:

CompoundEventFilter::CompoundEventFilter() {
}

CompoundEventFilter::~CompoundEventFilter() {
  // Additional filters are not owned by CompoundEventFilter and they
  // should all be removed when running here. |handlers_| has
  // check_empty == true and will DCHECK failure if it is not empty.
}

// static
gfx::NativeCursor CompoundEventFilter::CursorForWindowComponent(
    int window_component) {
  switch (window_component) {
    case HTBOTTOM:
      return ui::mojom::CursorType::kSouthResize;
    case HTBOTTOMLEFT:
      return ui::mojom::CursorType::kSouthWestResize;
    case HTBOTTOMRIGHT:
      return ui::mojom::CursorType::kSouthEastResize;
    case HTLEFT:
      return ui::mojom::CursorType::kWestResize;
    case HTRIGHT:
      return ui::mojom::CursorType::kEastResize;
    case HTTOP:
      return ui::mojom::CursorType::kNorthResize;
    case HTTOPLEFT:
      return ui::mojom::CursorType::kNorthWestResize;
    case HTTOPRIGHT:
      return ui::mojom::CursorType::kNorthEastResize;
    default:
      return ui::mojom::CursorType::kNull;
  }
}

gfx::NativeCursor CompoundEventFilter::NoResizeCursorForWindowComponent(
    int window_component) {
  switch (window_component) {
    case HTBOTTOM:
      return ui::mojom::CursorType::kNorthSouthNoResize;
    case HTBOTTOMLEFT:
      return ui::mojom::CursorType::kNorthEastSouthWestNoResize;
    case HTBOTTOMRIGHT:
      return ui::mojom::CursorType::kNorthWestSouthEastNoResize;
    case HTLEFT:
      return ui::mojom::CursorType::kEastWestNoResize;
    case HTRIGHT:
      return ui::mojom::CursorType::kEastWestNoResize;
    case HTTOP:
      return ui::mojom::CursorType::kNorthSouthNoResize;
    case HTTOPLEFT:
      return ui::mojom::CursorType::kNorthWestSouthEastNoResize;
    case HTTOPRIGHT:
      return ui::mojom::CursorType::kNorthEastSouthWestNoResize;
    default:
      return ui::mojom::CursorType::kNull;
  }
}

void CompoundEventFilter::AddHandler(ui::EventHandler* handler) {
  handlers_.AddObserver(handler);
}

void CompoundEventFilter::RemoveHandler(ui::EventHandler* handler) {
  handlers_.RemoveObserver(handler);
}

////////////////////////////////////////////////////////////////////////////////
// CompoundEventFilter, private:

void CompoundEventFilter::UpdateCursor(aura::Window* target,
                                       ui::MouseEvent* event) {
  // If drag and drop is in progress, let the drag drop client set the cursor
  // instead of setting the cursor here.
  aura::Window* root_window = target->GetRootWindow();
  aura::client::DragDropClient* drag_drop_client =
      aura::client::GetDragDropClient(root_window);
  if (drag_drop_client && drag_drop_client->IsDragDropInProgress())
    return;

  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);
  if (cursor_client) {
    gfx::NativeCursor cursor = target->GetCursor(event->location());
    if ((event->flags() & ui::EF_IS_NON_CLIENT)) {
      if (target->delegate()) {
        int window_component =
            target->delegate()->GetNonClientComponent(event->location());

        if ((target->GetProperty(aura::client::kResizeBehaviorKey) &
             aura::client::kResizeBehaviorCanResize) != 0) {
          cursor = CursorForWindowComponent(window_component);
        } else {
          cursor = NoResizeCursorForWindowComponent(window_component);
        }
      } else {
        // Allow the OS to handle non client cursors if we don't have a
        // a delegate to handle the non client hittest.
        return;
      }
    }
    // For EventType::kMouseEntered, force the update of the cursor because it
    // may have changed without |cursor_client| knowing about it.
    if (event->type() == ui::EventType::kMouseEntered) {
      cursor_client->SetCursorForced(cursor);
    } else {
      cursor_client->SetCursor(cursor);
    }
  }
}

void CompoundEventFilter::FilterKeyEvent(ui::KeyEvent* event) {
  for (ui::EventHandler& handler : handlers_) {
    if (event->stopped_propagation())
      break;
    handler.OnKeyEvent(event);
  }
}

void CompoundEventFilter::FilterMouseEvent(ui::MouseEvent* event) {
  for (ui::EventHandler& handler : handlers_) {
    if (event->stopped_propagation())
      break;
    handler.OnMouseEvent(event);
  }
}

void CompoundEventFilter::FilterTouchEvent(ui::TouchEvent* event) {
  for (ui::EventHandler& handler : handlers_) {
    if (event->stopped_propagation())
      break;
    handler.OnTouchEvent(event);
  }
}

void CompoundEventFilter::SetCursorVisibilityOnEvent(aura::Window* target,
                                                     ui::Event* event,
                                                     bool show) {
  if (event->flags() & ui::EF_IS_SYNTHESIZED)
    return;

  aura::client::CursorClient* client =
      aura::client::GetCursorClient(target->GetRootWindow());
  if (!client)
    return;

  if (show)
    client->ShowCursor();
  else
    client->HideCursor();
}

void CompoundEventFilter::SetMouseEventsEnableStateOnEvent(aura::Window* target,
                                                           ui::Event* event,
                                                           bool enable) {
  TRACE_EVENT2("ui,input",
               "CompoundEventFilter::SetMouseEventsEnableStateOnEvent",
               "event_flags", event->flags(), "enable", enable);
  if (event->flags() & ui::EF_IS_SYNTHESIZED)
    return;
  aura::client::CursorClient* client =
      aura::client::GetCursorClient(target->GetRootWindow());
  if (!client) {
    TRACE_EVENT_INSTANT0(
        "ui,input",
        "CompoundEventFilter::SetMouseEventsEnableStateOnEvent - No Client",
        TRACE_EVENT_SCOPE_THREAD);
    return;
  }

  if (enable)
    client->EnableMouseEvents();
  else
    client->DisableMouseEvents();
}

////////////////////////////////////////////////////////////////////////////////
// CompoundEventFilter, ui::EventHandler implementation:

void CompoundEventFilter::OnKeyEvent(ui::KeyEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  aura::client::CursorClient* client =
      aura::client::GetCursorClient(target->GetRootWindow());
  if (client && client->ShouldHideCursorOnKeyEvent(*event))
    SetCursorVisibilityOnEvent(target, event, false);

  FilterKeyEvent(event);
}

void CompoundEventFilter::OnMouseEvent(ui::MouseEvent* event) {
  TRACE_EVENT2("ui,input", "CompoundEventFilter::OnMouseEvent", "event_type",
               event->type(), "event_flags", event->flags());
  aura::Window* window = static_cast<aura::Window*>(event->target());

  // We must always update the cursor, otherwise the cursor can get stuck if an
  // event filter registered with us consumes the event.
  // It should also update the cursor for clicking and wheels for ChromeOS boot.
  // When ChromeOS is booted, it hides the mouse cursor but immediate mouse
  // operation will show the cursor.
  // We also update the cursor for mouse enter in case a mouse cursor is sent to
  // outside of the root window and moved back for some reasons (e.g. running on
  // on Desktop for testing, or a bug in pointer barrier).
  if (!(event->flags() & ui::EF_FROM_TOUCH) &&
      (event->type() == ui::EventType::kMouseEntered ||
       event->type() == ui::EventType::kMouseMoved ||
       event->type() == ui::EventType::kMousePressed ||
       event->type() == ui::EventType::kMousewheel)) {
    SetMouseEventsEnableStateOnEvent(window, event, true);
    SetCursorVisibilityOnEvent(window, event, true);
    UpdateCursor(window, event);
  }

  FilterMouseEvent(event);
}

void CompoundEventFilter::OnScrollEvent(ui::ScrollEvent* event) {
}

void CompoundEventFilter::OnTouchEvent(ui::TouchEvent* event) {
  TRACE_EVENT2("ui,input", "CompoundEventFilter::OnTouchEvent", "event_type",
               event->type(), "event_handled", event->handled());
  FilterTouchEvent(event);
  if (!event->handled() && event->type() == ui::EventType::kTouchPressed) {
    aura::Window* target = static_cast<aura::Window*>(event->target());
    DCHECK(target);
    auto* client = aura::client::GetCursorClient(target->GetRootWindow());
    if (client && client->ShouldHideCursorOnTouchEvent(*event) &&
        !aura::Env::GetInstance()->IsMouseButtonDown()) {
      SetMouseEventsEnableStateOnEvent(target, event, false);
      SetCursorVisibilityOnEvent(target, event, false);
    }
  }
}

void CompoundEventFilter::OnGestureEvent(ui::GestureEvent* event) {
  for (ui::EventHandler& handler : handlers_) {
    if (event->stopped_propagation())
      break;
    handler.OnGestureEvent(event);
  }
}

std::string_view CompoundEventFilter::GetLogContext() const {
  return "CompoundEventFilter";
}

}  // namespace wm
