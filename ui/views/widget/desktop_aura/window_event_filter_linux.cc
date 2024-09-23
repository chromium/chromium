// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/window_event_filter_linux.h"

#include <optional>

#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/linux/linux_ui.h"
#include "ui/platform_window/wm/wm_move_resize_handler.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"

namespace views {

WindowEventFilterLinux::WindowEventFilterLinux(
    DesktopWindowTreeHostPlatform* desktop_window_tree_host,
    ui::WmMoveResizeHandler* handler)
    : desktop_window_tree_host_(desktop_window_tree_host), handler_(handler) {
  desktop_window_tree_host_->window()->AddPreTargetHandler(this);
}

WindowEventFilterLinux::~WindowEventFilterLinux() {
  desktop_window_tree_host_->window()->RemovePreTargetHandler(this);
}

void WindowEventFilterLinux::HandleLocatedEventWithHitTest(
    int hit_test,
    ui::LocatedEvent* event) {
  if (event->type() != ui::EventType::kMousePressed) {
    return;
  }

  if (event->IsMouseEvent() &&
      HandleMouseEventWithHitTest(hit_test, event->AsMouseEvent())) {
    return;
  }

  if (desktop_window_tree_host_->GetContentWindow()->GetProperty(
          aura::client::kResizeBehaviorKey) &
      aura::client::kResizeBehaviorCanResize) {
    MaybeDispatchHostWindowDragMovement(hit_test, event);
  }
}

bool WindowEventFilterLinux::HandleMouseEventWithHitTest(
    int hit_test,
    ui::MouseEvent* event) {
  int previous_click_component = HTNOWHERE;
  if (event->IsLeftMouseButton()) {
    previous_click_component = click_component_;
    click_component_ = hit_test;
  }

  if (hit_test == HTCAPTION) {
    OnClickedCaption(event, previous_click_component);
    return true;
  }

  if (hit_test == HTMAXBUTTON) {
    OnClickedMaximizeButton(event);
    return true;
  }

  return false;
}

void WindowEventFilterLinux::OnClickedCaption(ui::MouseEvent* event,
                                              int previous_click_component) {
  ui::LinuxUi::WindowFrameActionSource action_type;
  ui::LinuxUi::WindowFrameAction default_action;

  if (event->IsRightMouseButton()) {
    action_type = ui::LinuxUi::WindowFrameActionSource::kRightClick;
    default_action = ui::LinuxUi::WindowFrameAction::kMenu;
  } else if (event->IsMiddleMouseButton()) {
    action_type = ui::LinuxUi::WindowFrameActionSource::kMiddleClick;
    default_action = ui::LinuxUi::WindowFrameAction::kNone;
  } else if (event->IsLeftMouseButton() &&
             event->flags() & ui::EF_IS_DOUBLE_CLICK) {
    click_component_ = HTNOWHERE;
    if (previous_click_component == HTCAPTION) {
      action_type = ui::LinuxUi::WindowFrameActionSource::kDoubleClick;
      default_action = ui::LinuxUi::WindowFrameAction::kToggleMaximize;
    } else {
      return;
    }
  } else {
    MaybeDispatchHostWindowDragMovement(HTCAPTION, event);
    return;
  }

  auto* content_window = desktop_window_tree_host_->GetContentWindow();
  auto* linux_ui_theme = ui::LinuxUi::instance();
  ui::LinuxUi::WindowFrameAction action =
      linux_ui_theme ? linux_ui_theme->GetWindowFrameAction(action_type)
                     : default_action;
  switch (action) {
    case ui::LinuxUi::WindowFrameAction::kNone:
      break;
    case ui::LinuxUi::WindowFrameAction::kLower:
      LowerWindow();
      event->SetHandled();
      break;
    case ui::LinuxUi::WindowFrameAction::kMinimize:
      desktop_window_tree_host_->Minimize();
      event->SetHandled();
      break;
    case ui::LinuxUi::WindowFrameAction::kToggleMaximize:
      MaybeToggleMaximizedState(content_window);
      event->SetHandled();
      break;
    case ui::LinuxUi::WindowFrameAction::kMenu:
      views::Widget* widget =
          views::Widget::GetWidgetForNativeView(content_window);
      if (!widget)
        break;
      views::View* view = widget->GetContentsView();
      if (!view || !view->context_menu_controller())
        break;
      // Controller requires locations to be in DIP, while |this| receives the
      // location in px.
      gfx::PointF location = desktop_window_tree_host_->GetRootTransform()
                                 .InverseMapPoint(event->location_f())
                                 .value_or(event->location_f());
      gfx::Point location_in_screen = gfx::ToRoundedPoint(location);
      views::View::ConvertPointToScreen(view, &location_in_screen);
      view->ShowContextMenu(location_in_screen, ui::MENU_SOURCE_MOUSE);
      event->SetHandled();
      break;
  }
}

void WindowEventFilterLinux::OnClickedMaximizeButton(ui::MouseEvent* event) {
  auto* content_window = desktop_window_tree_host_->GetContentWindow();
  views::Widget* widget = views::Widget::GetWidgetForNativeView(content_window);
  if (!widget)
    return;

  gfx::Rect display_work_area = display::Screen::GetScreen()
                                    ->GetDisplayNearestWindow(content_window)
                                    .work_area();
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

void WindowEventFilterLinux::MaybeToggleMaximizedState(aura::Window* window) {
  if (!(window->GetProperty(aura::client::kResizeBehaviorKey) &
        aura::client::kResizeBehaviorCanMaximize)) {
    return;
  }

  if (desktop_window_tree_host_->IsMaximized())
    desktop_window_tree_host_->Restore();
  else
    desktop_window_tree_host_->Maximize();
}

void WindowEventFilterLinux::LowerWindow() {
#if BUILDFLAG(IS_OZONE_X11)
  desktop_window_tree_host_->LowerWindow();
#endif  // BUILDFLAG(IS_OZONE_X11)
}

void WindowEventFilterLinux::MaybeDispatchHostWindowDragMovement(
    int hittest,
    ui::LocatedEvent* event) {
  if (!event->IsMouseEvent() && !event->IsGestureEvent())
    return;

  if (event->IsMouseEvent() && !event->AsMouseEvent()->IsLeftMouseButton())
    return;

  if (!handler_ || !ui::CanPerformDragOrResize(hittest))
    return;

  // Some platforms (eg X11) may require last pointer location not in the
  // local surface coordinates, but rather in the screen coordinates for
  // interactive move/resize.
  auto bounds_in_px =
      desktop_window_tree_host_->AsWindowTreeHost()->GetBoundsInPixels();
  auto screen_point_in_px = event->location();
  screen_point_in_px.Offset(bounds_in_px.x(), bounds_in_px.y());
  handler_->DispatchHostWindowDragMovement(hittest, screen_point_in_px);

  // Stop the event propagation for mouse events only (not touch), given that
  // it'd prevent the Gesture{Provider,Detector} machirery to get triggered,
  // breaking gestures including tapping, double tapping, show press and
  // long press.
  if (event->IsMouseEvent())
    event->StopPropagation();
}

void WindowEventFilterLinux::OnGestureEvent(ui::GestureEvent* event) {
  auto* window = static_cast<aura::Window*>(event->target());
  int hit_test_code =
      window->delegate()
          ? window->delegate()->GetNonClientComponent(event->location())
          : HTNOWHERE;

  // Double tap to maximize.
  if (event->type() == ui::EventType::kGestureTap) {
    int previous_click_component = click_component_;
    click_component_ = hit_test_code;

    if (click_component_ == HTCAPTION &&
        click_component_ == previous_click_component &&
        event->details().tap_count() == 2) {
      MaybeToggleMaximizedState(window);
      click_component_ = HTNOWHERE;
      event->StopPropagation();
    }
    return;
  }

  // Interactive window move.
  if (event->type() == ui::EventType::kGestureScrollBegin) {
    MaybeDispatchHostWindowDragMovement(hit_test_code, event);
  }
}

}  // namespace views
