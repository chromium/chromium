// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/window_event_filter.h"

#include "services/ws/public/mojom/window_tree_constants.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/platform_window/platform_window_handler/wm_move_resize_handler.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

bool CanPerformDragOrResize(int hittest) {
  switch (hittest) {
    case HTBOTTOM:
    case HTBOTTOMLEFT:
    case HTBOTTOMRIGHT:
    case HTCAPTION:
    case HTLEFT:
    case HTRIGHT:
    case HTTOP:
    case HTTOPLEFT:
    case HTTOPRIGHT:
      return true;
    default:
      return false;
  }
  return true;
}

}  // namespace

WindowEventFilter::WindowEventFilter(DesktopWindowTreeHost* window_tree_host)
    : window_tree_host_(window_tree_host), click_component_(HTNOWHERE) {}

WindowEventFilter::~WindowEventFilter() {}

void WindowEventFilter::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() != ui::ET_MOUSE_PRESSED)
    return;

  aura::Window* target = static_cast<aura::Window*>(event->target());
  if (!target->delegate())
    return;

  int previous_click_component = HTNOWHERE;
  int component = target->delegate()->GetNonClientComponent(event->location());
  if (event->IsLeftMouseButton()) {
    previous_click_component = click_component_;
    click_component_ = component;
  }

  if (component == HTCAPTION) {
    OnClickedCaption(event, previous_click_component);
  } else if (component == HTMAXBUTTON) {
    OnClickedMaximizeButton(event);
  } else {
    if (target->GetProperty(aura::client::kResizeBehaviorKey) &
        ws::mojom::kResizeBehaviorCanResize) {
      MaybeDispatchHostWindowDragMovement(component, event);
    }
  }
}

void WindowEventFilter::SetWmMoveResizeHandler(
    ui::WmMoveResizeHandler* handler) {
  DCHECK(!handler_);
  handler_ = handler;
}

void WindowEventFilter::OnClickedCaption(ui::MouseEvent* event,
                                         int previous_click_component) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  LinuxUI* linux_ui = LinuxUI::instance();

  views::LinuxUI::NonClientWindowFrameActionSourceType action_type;
  views::LinuxUI::NonClientWindowFrameAction default_action;

  if (event->IsRightMouseButton()) {
    action_type = LinuxUI::WINDOW_FRAME_ACTION_SOURCE_RIGHT_CLICK;
    default_action = LinuxUI::WINDOW_FRAME_ACTION_MENU;
  } else if (event->IsMiddleMouseButton()) {
    action_type = LinuxUI::WINDOW_FRAME_ACTION_SOURCE_MIDDLE_CLICK;
    default_action = LinuxUI::WINDOW_FRAME_ACTION_NONE;
  } else if (event->IsLeftMouseButton() &&
             event->flags() & ui::EF_IS_DOUBLE_CLICK) {
    click_component_ = HTNOWHERE;
    if (previous_click_component == HTCAPTION) {
      action_type = LinuxUI::WINDOW_FRAME_ACTION_SOURCE_DOUBLE_CLICK;
      default_action = LinuxUI::WINDOW_FRAME_ACTION_TOGGLE_MAXIMIZE;
    } else {
      return;
    }
  } else {
    MaybeDispatchHostWindowDragMovement(HTCAPTION, event);
    return;
  }

  LinuxUI::NonClientWindowFrameAction action =
      linux_ui ? linux_ui->GetNonClientWindowFrameAction(action_type)
               : default_action;
  switch (action) {
    case LinuxUI::WINDOW_FRAME_ACTION_NONE:
      break;
    case LinuxUI::WINDOW_FRAME_ACTION_LOWER:
      LowerWindow();
      event->SetHandled();
      break;
    case LinuxUI::WINDOW_FRAME_ACTION_MINIMIZE:
      window_tree_host_->Minimize();
      event->SetHandled();
      break;
    case LinuxUI::WINDOW_FRAME_ACTION_TOGGLE_MAXIMIZE:
      if (target->GetProperty(aura::client::kResizeBehaviorKey) &
          ws::mojom::kResizeBehaviorCanMaximize)
        ToggleMaximizedState();
      event->SetHandled();
      break;
    case LinuxUI::WINDOW_FRAME_ACTION_MENU:
      views::Widget* widget = views::Widget::GetWidgetForNativeView(target);
      if (!widget)
        break;
      views::View* view = widget->GetContentsView();
      if (!view || !view->context_menu_controller())
        break;
      gfx::Point location(event->location());
      views::View::ConvertPointToScreen(view, &location);
      view->ShowContextMenu(location, ui::MENU_SOURCE_MOUSE);
      event->SetHandled();
      break;
  }
}

void WindowEventFilter::OnClickedMaximizeButton(ui::MouseEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  views::Widget* widget = views::Widget::GetWidgetForNativeView(target);
  if (!widget)
    return;

  gfx::Rect display_work_area =
      display::Screen::GetScreen()->GetDisplayNearestWindow(target).work_area();
  gfx::Rect bounds = widget->GetWindowBoundsInScreen();
  if (event->IsMiddleMouseButton()) {
    bounds.set_y(display_work_area.y());
    bounds.set_height(display_work_area.height());
    widget->SetBounds(bounds);
    event->StopPropagation();
  } else if (event->IsRightMouseButton()) {
    bounds.set_x(display_work_area.x());
    bounds.set_width(display_work_area.width());
    widget->SetBounds(bounds);
    event->StopPropagation();
  }
}

void WindowEventFilter::ToggleMaximizedState() {
  if (window_tree_host_->IsMaximized())
    window_tree_host_->Restore();
  else
    window_tree_host_->Maximize();
}

void WindowEventFilter::LowerWindow() {}

void WindowEventFilter::MaybeDispatchHostWindowDragMovement(
    int hittest,
    ui::MouseEvent* event) {
  if (handler_ && event->IsLeftMouseButton() &&
      CanPerformDragOrResize(hittest)) {
    // Some platforms (eg X11) may require last pointer location not in the
    // local surface coordinates, but rather in the screen coordinates for
    // interactive move/resize.
    const gfx::Point last_pointer_location =
        aura::Env::GetInstance()->last_mouse_location();
    handler_->DispatchHostWindowDragMovement(hittest, last_pointer_location);
    event->StopPropagation();
    return;
  }
}

}  // namespace views
