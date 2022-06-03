// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/views_touch_selection_controller_factory.h"

namespace views {

ViewsTouchEditingControllerFactory::ViewsTouchEditingControllerFactory() =
    default;

ui::TouchEditingControllerDeprecated*
ViewsTouchEditingControllerFactory::Create(ui::TouchEditable* client_view) {
  return nullptr;
}

}  // namespace views
