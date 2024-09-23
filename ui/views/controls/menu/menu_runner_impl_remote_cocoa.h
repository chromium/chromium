// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_REMOTE_COCOA_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_REMOTE_COCOA_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "components/remote_cocoa/common/menu.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "ui/views/controls/menu/menu_runner_impl_interface.h"

namespace views {

// Menu runner implementation that serializes the menu model over mojo, to then
// use NSMenu in possibly a different process to show a context menu.
// Like MenuRunnerImplCocoa this only supports context menus.
class VIEWS_EXPORT MenuRunnerImplRemoteCocoa
    : public internal::MenuRunnerImplInterface,
      public remote_cocoa::mojom::MenuHost {
 public:
  MenuRunnerImplRemoteCocoa(ui::MenuModel* menu_model,
                            base::RepeatingClosure on_menu_closed_callback);

  MenuRunnerImplRemoteCocoa(const MenuRunnerImplRemoteCocoa&) = delete;
  MenuRunnerImplRemoteCocoa& operator=(const MenuRunnerImplRemoteCocoa&) =
      delete;

  // Runs the menu at the specific `anchor` in screen coordinates. If specified,
  // `target_view_id` is used to look up the NSView to use as target of the
  // menu (which is then used by AppKit to for example populate the Services
  // submenu).
  void RunMenu(Widget* widget,
               const gfx::Point& anchor,
               uint64_t target_view_id = 0);

  // Updates the label and state of the top-level menu item with id equal to
  // `command_id`. Should only be called while the menu is running.
  void UpdateMenuItem(int command_id,
                      bool enabled,
                      bool hidden,
                      const std::u16string& title);

  // MenuRunnerImplInterface:
  bool IsRunning() const override;
  void Release() override;
  void RunMenuAt(
      Widget* parent,
      MenuButtonController* button_controller,
      const gfx::Rect& bounds,
      MenuAnchorPosition anchor,
      int32_t run_types,
      gfx::NativeView native_view_for_gestures,
      std::optional<gfx::RoundedCornersF> corners,
      std::optional<std::string> show_menu_host_duration_histogram) override;
  void Cancel() override;
  base::TimeTicks GetClosingEventTime() const override;

 private:
  ~MenuRunnerImplRemoteCocoa() override;

  // remote_cocoa::mojom::MenuHost:
  void CommandActivated(int32_t command_id, int32_t event_flags) override;
  void MenuClosed() override;

  // Recursively converts `menu_model` to mojom structs. Since we rely on
  // command IDs being unique, this uses `command_ids` to verify that all
  // menu items in the model do in fact have unique command IDs.
  std::vector<remote_cocoa::mojom::MenuItemPtr> ModelToMojo(
      const ui::MenuModel& menu_model,
      std::set<int>& command_ids);

  raw_ptr<ui::MenuModel> menu_model_;
  mojo::Receiver<remote_cocoa::mojom::MenuHost> host_receiver_{this};
  mojo::Remote<remote_cocoa::mojom::Menu> menu_remote_;

  // Is the context menu currently being displayed?
  bool running_ = false;

  // The timestamp of the event which closed the menu - or 0.
  base::TimeTicks closing_event_time_;

  base::RepeatingClosure on_menu_closed_callback_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_REMOTE_COCOA_H_
