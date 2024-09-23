// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_PRE_TARGET_HANDLER_AURA_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_PRE_TARGET_HANDLER_AURA_H_

#include "base/memory/raw_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event_handler.h"
#include "ui/views/controls/menu/menu_pre_target_handler.h"
#include "ui/views/views_export.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {

class MenuController;
class Widget;

// MenuPreTargetHandlerAura is used to observe activation changes, cancel
// events, and root window destruction, to shutdown the menu.
//
// Additionally handles key events to provide accelerator support to menus.
class VIEWS_EXPORT MenuPreTargetHandlerAura
    : public wm::ActivationChangeObserver,
      public aura::WindowObserver,
      public ui::EventHandler,
      public MenuPreTargetHandler {
 public:
  MenuPreTargetHandlerAura(MenuController* controller, Widget* owner);

  MenuPreTargetHandlerAura(const MenuPreTargetHandlerAura&) = delete;
  MenuPreTargetHandlerAura& operator=(const MenuPreTargetHandlerAura&) = delete;

  ~MenuPreTargetHandlerAura() override;

  // aura::client:ActivationChangeObserver:
  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // ui::EventHandler:
  void OnCancelMode(ui::CancelModeEvent* event) override;
  void OnKeyEvent(ui::KeyEvent* event) override;

  // Return true, if the key event is supposed to perform some task.
  bool ShouldCancelMenuForEvent(const ui::KeyEvent& event);

 private:
  void Cleanup();

  raw_ptr<MenuController> controller_;
  raw_ptr<aura::Window> root_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_PRE_TARGET_HANDLER_AURA_H_
