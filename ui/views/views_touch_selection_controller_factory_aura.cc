// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/views_touch_selection_controller_factory.h"

#include "ui/base/ui_base_switches_util.h"
#include "ui/views/touchui/touch_selection_controller_impl.h"

namespace views {

ViewsTouchEditingControllerFactory::ViewsTouchEditingControllerFactory() =
    default;

ui::TouchEditingControllerDeprecated*
ViewsTouchEditingControllerFactory::Create(
    ui::TouchEditable* client_view) {
  return new views::TouchSelectionControllerImpl(client_view);
}

}  // namespace views
