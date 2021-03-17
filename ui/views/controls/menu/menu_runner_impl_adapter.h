// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_ADAPTER_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_ADAPTER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "ui/views/controls/menu/menu_runner_impl_interface.h"
#include "ui/views/views_export.h"

namespace views {

class MenuModelAdapter;

namespace internal {

class MenuRunnerImpl;

// Given a MenuModel, adapts MenuRunnerImpl which expects a MenuItemView.
class VIEWS_EXPORT MenuRunnerImplAdapter : public MenuRunnerImplInterface {
 public:
  MenuRunnerImplAdapter(ui::MenuModel* menu_model,
                        base::RepeatingClosure on_menu_closed_callback);

  // MenuRunnerImplInterface:
  bool IsRunning() const override;
  void Release() override;
  void RunMenuAt(Widget* parent,
                 MenuButtonController* button_controller,
                 const gfx::Rect& bounds,
                 MenuAnchorPosition anchor,
                 int32_t types) override;
  void Cancel() override;
  base::TimeTicks GetClosingEventTime() const override;

 private:
  ~MenuRunnerImplAdapter() override;

  std::unique_ptr<MenuModelAdapter> menu_model_adapter_;
  MenuRunnerImpl* impl_;

  DISALLOW_COPY_AND_ASSIGN(MenuRunnerImplAdapter);
};

}  // namespace internal
}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_ADAPTER_H_
