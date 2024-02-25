// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_ADAPTER_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_ADAPTER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/views/controls/menu/menu_runner_impl_interface.h"
#include "ui/views/views_export.h"

namespace gfx {
class RoundedCornersF;
}  // namespace gfx

namespace views {

class MenuModelAdapter;

namespace internal {

class MenuRunnerImpl;

// Given a MenuModel, adapts MenuRunnerImpl which expects a MenuItemView.
class VIEWS_EXPORT MenuRunnerImplAdapter : public MenuRunnerImplInterface {
 public:
  MenuRunnerImplAdapter(ui::MenuModel* menu_model,
                        base::RepeatingClosure on_menu_closed_callback);

  MenuRunnerImplAdapter(const MenuRunnerImplAdapter&) = delete;
  MenuRunnerImplAdapter& operator=(const MenuRunnerImplAdapter&) = delete;

  // MenuRunnerImplInterface:
  bool IsRunning() const override;
  void Release() override;
  void RunMenuAt(Widget* parent,
                 MenuButtonController* button_controller,
                 const gfx::Rect& bounds,
                 MenuAnchorPosition anchor,
                 int32_t types,
                 gfx::NativeView native_view_for_gestures,
                 std::optional<gfx::RoundedCornersF> corners = std::nullopt,
                 std::optional<std::string> show_menu_host_duration_histogram =
                     std::nullopt) override;
  void Cancel() override;
  base::TimeTicks GetClosingEventTime() const override;

 private:
  ~MenuRunnerImplAdapter() override;

  std::unique_ptr<MenuModelAdapter> menu_model_adapter_;
  raw_ptr<MenuRunnerImpl> impl_;
};

}  // namespace internal
}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_ADAPTER_H_
