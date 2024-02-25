// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/views/controls/menu/menu_controller_delegate.h"
#include "ui/views/controls/menu/menu_runner_impl_interface.h"
#include "ui/views/views_export.h"

namespace gfx {
class RoundedCornersF;
}  // namespace gfx

namespace views {

class MenuController;
class MenuDelegate;
class MenuItemView;

namespace test {

class MenuRunnerDestructionTest;

}  // namespace test

namespace internal {

// A menu runner implementation that uses views::MenuItemView to show a menu.
class VIEWS_EXPORT MenuRunnerImpl : public MenuRunnerImplInterface,
                                    public MenuControllerDelegate {
 public:
  explicit MenuRunnerImpl(std::unique_ptr<MenuItemView> menu);

  MenuRunnerImpl(const MenuRunnerImpl&) = delete;
  MenuRunnerImpl& operator=(const MenuRunnerImpl&) = delete;

  bool IsRunning() const override;
  void Release() override;
  void RunMenuAt(Widget* parent,
                 MenuButtonController* button_controller,
                 const gfx::Rect& bounds,
                 MenuAnchorPosition anchor,
                 int32_t run_types,
                 gfx::NativeView native_view_for_gestures,
                 std::optional<gfx::RoundedCornersF> corners = std::nullopt,
                 std::optional<std::string> show_menu_host_duration_histogram =
                     std::nullopt) override;
  void Cancel() override;
  base::TimeTicks GetClosingEventTime() const override;

  // MenuControllerDelegate:
  void OnMenuClosed(NotifyType type,
                    MenuItemView* menu,
                    int mouse_event_flags) override;
  void SiblingMenuCreated(MenuItemView* menu) override;

 private:
  friend class ::views::test::MenuRunnerDestructionTest;

  ~MenuRunnerImpl() override;

  // Returns true if mnemonics should be shown in the menu.
  bool ShouldShowMnemonics(int32_t run_types);

  // Created and set as the delegate of the MenuItemView if Release() is
  // invoked.  This is done to make sure the delegate isn't notified after
  // Release() is invoked. We do this as we assume the delegate is no longer
  // valid if MenuRunner has been deleted.
  std::unique_ptr<MenuDelegate> empty_delegate_;

  // The menu.
  std::unique_ptr<MenuItemView> menu_;

  // Any sibling menus. Does not include |menu_|. We own these too.
  std::set<raw_ptr<MenuItemView, SetExperimental>> sibling_menus_;

  // Are we in run waiting for it to return?
  bool running_ = false;

  // Set if |running_| and Release() has been invoked.
  bool delete_after_run_ = false;

  // Are we running for a drop?
  bool for_drop_ = false;

  // The controller.
  base::WeakPtr<MenuController> controller_;

  // Do we own the controller?
  bool owns_controller_ = false;

  // The timestamp of the event which closed the menu - or 0.
  base::TimeTicks closing_event_time_;

  // Used to detect deletion of |this| when notifying delegate of success.
  base::WeakPtrFactory<MenuRunnerImpl> weak_factory_{this};
};

}  // namespace internal
}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_H_
