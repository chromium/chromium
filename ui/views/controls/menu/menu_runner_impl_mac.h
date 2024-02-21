// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_MAC_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_MAC_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "components/remote_cocoa/common/menu.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "ui/views/controls/menu/menu_runner_impl_interface.h"

namespace views::internal {

// Menu runner implementation that chooses between and delegates to either
// MenuRunnerImplCocoa or MenuRunnerImplRemoteCocoa, depending on if the widget
// the menu is being shown in is local or remote.
class VIEWS_EXPORT MenuRunnerImplMac : public MenuRunnerImplInterface {
 public:
  MenuRunnerImplMac(ui::MenuModel* menu_model,
                    base::RepeatingClosure on_menu_closed_callback);

  MenuRunnerImplMac(const MenuRunnerImplMac&) = delete;
  MenuRunnerImplMac& operator=(const MenuRunnerImplMac&) = delete;

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
  ~MenuRunnerImplMac() override;

  raw_ptr<ui::MenuModel> menu_model_;
  base::RepeatingClosure on_menu_closed_callback_;

  raw_ptr<MenuRunnerImplInterface> implementation_ = nullptr;
};

}  // namespace views::internal

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_MAC_H_
