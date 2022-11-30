// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/dialog_model_context_menu_controller.h"

#include "ui/base/models/dialog_model.h"
#include "ui/views/view.h"

namespace views {

DialogModelContextMenuController::DialogModelContextMenuController(
    View* host,
    base::RepeatingCallback<std::unique_ptr<ui::DialogModel>()>
        model_generator_callback,
    int run_types,
    MenuAnchorPosition anchor_position)
    : host_(host),
      run_types_(run_types),
      anchor_position_(anchor_position),
      model_generator_callback_(model_generator_callback) {
  host_->set_context_menu_controller(this);
}

DialogModelContextMenuController::~DialogModelContextMenuController() {
  host_->set_context_menu_controller(nullptr);
}

void DialogModelContextMenuController::ShowContextMenuForViewImpl(
    View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  DCHECK_EQ(source, host_);

  menu_model_ = std::make_unique<ui::DialogModelMenuModelAdapter>(
      model_generator_callback_.Run());
  menu_runner_ =
      std::make_unique<views::MenuRunner>(menu_model_.get(), run_types_);
  menu_runner_->RunMenuAt(source->GetWidget(), /*button_controller=*/nullptr,
                          gfx::Rect(point, gfx::Size()), anchor_position_,
                          source_type);
}
}  // namespace views
