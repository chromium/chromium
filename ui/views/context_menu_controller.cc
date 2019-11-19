// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/context_menu_controller.h"

#include "base/auto_reset.h"

namespace views {

ContextMenuController::ContextMenuController() = default;

ContextMenuController::~ContextMenuController() = default;

void ContextMenuController::ShowContextMenuForView(
    View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  // Use a boolean flag to early-exit out of re-entrant behavior.
  if (is_opening_)
    return;
  is_opening_ = true;

  // We might get deleted while showing the context menu (including as a result
  // of showing it). If so, we need to make sure we're not accessing
  // |is_opening_|.
  auto weak_ptr = weak_factory_.GetWeakPtr();

  ShowContextMenuForViewImpl(source, point, source_type);

  if (!weak_ptr)
    return;

  is_opening_ = false;
}

}  // namespace views
