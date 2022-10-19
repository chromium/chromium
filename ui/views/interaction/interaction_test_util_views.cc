// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/interaction_test_util_views.h"

#include "build/build_config.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/gesture_event_details.h"
#include "ui/events/types/event_type.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/menu/menu_host.h"
#include "ui/views/controls/menu/menu_host_root_view.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace views::test {

InteractionTestUtilSimulatorViews::InteractionTestUtilSimulatorViews() =
    default;
InteractionTestUtilSimulatorViews::~InteractionTestUtilSimulatorViews() =
    default;

namespace {

gfx::Point GetCenter(views::View* view) {
  return view->GetLocalBounds().CenterPoint();
}

template <class T>
void SendDefaultAction(T* target) {
  ui::AXActionData action;
  action.action = ax::mojom::Action::kDoDefault;
  CHECK(target->HandleAccessibleAction(action));
}

// Sends a mouse click to the specified `target`.
// Views are EventHandlers but Widgets are not despite having the same API for
// event handling, so use a templated approach to support both cases.
template <class T>
void SendMouseClick(T* target, const gfx::Point& point) {
  ui::MouseEvent mouse_down(ui::ET_MOUSE_PRESSED, point, point,
                            ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                            ui::EF_LEFT_MOUSE_BUTTON);
  target->OnMouseEvent(&mouse_down);

  ui::MouseEvent mouse_up(ui::ET_MOUSE_RELEASED, point, point,
                          ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                          ui::EF_LEFT_MOUSE_BUTTON);
  target->OnMouseEvent(&mouse_up);
}

// Sends a tap gesture to the specified `target`.
// Views are EventHandlers but Widgets are not despite having the same API for
// event handling, so use a templated approach to support both cases.
template <class T>
void SendTapGesture(T* target, const gfx::Point& point) {
  ui::GestureEventDetails press_details(ui::ET_GESTURE_TAP);
  press_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent press_event(point.x(), point.y(), ui::EF_NONE,
                               ui::EventTimeForNow(), press_details);
  target->OnGestureEvent(&press_event);

  ui::GestureEventDetails release_details(ui::ET_GESTURE_END);
  release_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent release_event(point.x(), point.y(), ui::EF_NONE,
                                 ui::EventTimeForNow(), release_details);
  target->OnGestureEvent(&release_event);
}

// Sends a key press to the specified `target`.
void SendKeyPress(View* view, ui::KeyboardCode code) {
  view->OnKeyPressed(ui::KeyEvent(ui::ET_KEY_PRESSED, code, ui::EF_NONE,
                                  ui::EventTimeForNow()));

  view->OnKeyReleased(ui::KeyEvent(ui::ET_KEY_RELEASED, code, ui::EF_NONE,
                                   ui::EventTimeForNow()));
}

}  // namespace

bool InteractionTestUtilSimulatorViews::PressButton(ui::TrackedElement* element,
                                                    InputType input_type) {
  if (!element->IsA<TrackedElementViews>())
    return false;
  auto* const button =
      Button::AsButton(element->AsA<TrackedElementViews>()->view());
  if (!button)
    return false;

  PressButton(button, input_type);
  return true;
}

bool InteractionTestUtilSimulatorViews::SelectMenuItem(
    ui::TrackedElement* element,
    InputType input_type) {
  if (!element->IsA<TrackedElementViews>())
    return false;
  auto* const menu_item =
      AsViewClass<MenuItemView>(element->AsA<TrackedElementViews>()->view());
  if (!menu_item)
    return false;

#if BUILDFLAG(IS_MAC)
  // Keyboard input isn't reliable on Mac for submenus, so unless the test
  // specifically calls for keyboard input, prefer mouse.
  if (input_type == ui::test::InteractionTestUtil::InputType::kDontCare)
    input_type = ui::test::InteractionTestUtil::InputType::kMouse;
#endif  // BUILDFLAG(IS_MAC)

  auto* const host = menu_item->GetWidget()->GetRootView();
  gfx::Point point = GetCenter(menu_item);
  View::ConvertPointToTarget(menu_item, host, &point);

  switch (input_type) {
    case ui::test::InteractionTestUtil::InputType::kMouse:
      SendMouseClick(host->GetWidget(), point);
      break;
    case ui::test::InteractionTestUtil::InputType::kTouch:
      SendTapGesture(host->GetWidget(), point);
      break;
    case ui::test::InteractionTestUtil::InputType::kKeyboard:
    case ui::test::InteractionTestUtil::InputType::kDontCare: {
#if BUILDFLAG(IS_MAC)
      constexpr ui::KeyboardCode kSelectMenuKeyboardCode = ui::VKEY_SPACE;
#else
      constexpr ui::KeyboardCode kSelectMenuKeyboardCode = ui::VKEY_RETURN;
#endif
      MenuController* const controller = menu_item->GetMenuController();
      controller->SelectItemAndOpenSubmenu(menu_item);
      ui::KeyEvent key_event(ui::ET_KEY_PRESSED, kSelectMenuKeyboardCode,
                             ui::EF_NONE, ui::EventTimeForNow());
      controller->OnWillDispatchKeyEvent(&key_event);
      break;
    }
  }
  return true;
}

bool InteractionTestUtilSimulatorViews::DoDefaultAction(
    ui::TrackedElement* element,
    InputType input_type) {
  if (!element->IsA<TrackedElementViews>())
    return false;
  DoDefaultAction(element->AsA<TrackedElementViews>()->view(), input_type);
  return true;
}

void InteractionTestUtilSimulatorViews::DoDefaultAction(View* view,
                                                        InputType input_type) {
  switch (input_type) {
    case ui::test::InteractionTestUtil::InputType::kDontCare:
      SendDefaultAction(view);
      break;
    case ui::test::InteractionTestUtil::InputType::kMouse:
      SendMouseClick(view, GetCenter(view));
      break;
    case ui::test::InteractionTestUtil::InputType::kTouch:
      SendTapGesture(view, GetCenter(view));
      break;
    case ui::test::InteractionTestUtil::InputType::kKeyboard:
      SendKeyPress(view, ui::VKEY_SPACE);
      break;
  }
}

// static
void InteractionTestUtilSimulatorViews::PressButton(Button* button,
                                                    InputType input_type) {
  switch (input_type) {
    case ui::test::InteractionTestUtil::InputType::kMouse:
      SendMouseClick(button, GetCenter(button));
      break;
    case ui::test::InteractionTestUtil::InputType::kTouch:
      SendTapGesture(button, GetCenter(button));
      break;
    case ui::test::InteractionTestUtil::InputType::kKeyboard:
    case ui::test::InteractionTestUtil::InputType::kDontCare:
      SendKeyPress(button, ui::VKEY_SPACE);
      break;
  }
}

// static

}  // namespace views::test
