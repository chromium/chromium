// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_DIALOG_MODEL_CONTEXT_MENU_CONTROLLER_H_
#define UI_VIEWS_DIALOG_MODEL_CONTEXT_MENU_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/base/models/dialog_model_menu_model_adapter.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/views_export.h"

namespace views {

class View;

// A ContextMenuController that makes it easier to host context menus using
// ui::DialogModel. The constructor registers `this` as the context-menu
// controller of the `host` View.
class VIEWS_EXPORT DialogModelContextMenuController final
    : public ContextMenuController {
 public:
  DialogModelContextMenuController(
      View* host,
      base::RepeatingCallback<std::unique_ptr<ui::DialogModel>()>
          model_generator_callback,
      int run_types,
      MenuAnchorPosition anchor_position = MenuAnchorPosition::kTopLeft);

  DialogModelContextMenuController(const DialogModelContextMenuController&) =
      delete;
  DialogModelContextMenuController& operator=(
      const DialogModelContextMenuController&) = delete;

  ~DialogModelContextMenuController() override;

  void ShowContextMenuForViewImpl(View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

 private:
  const raw_ptr<View> host_;
  const int run_types_;
  const MenuAnchorPosition anchor_position_;
  const base::RepeatingCallback<std::unique_ptr<ui::DialogModel>()>
      model_generator_callback_;

  std::unique_ptr<ui::DialogModelMenuModelAdapter> menu_model_;
  std::unique_ptr<MenuRunner> menu_runner_;
};

}  // namespace views

#endif  // UI_VIEWS_DIALOG_MODEL_CONTEXT_MENU_CONTROLLER_H_
