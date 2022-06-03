// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEWS_TOUCH_SELECTION_CONTROLLER_FACTORY_H_
#define UI_VIEWS_VIEWS_TOUCH_SELECTION_CONTROLLER_FACTORY_H_

#include "ui/base/pointer/touch_editing_controller.h"
#include "ui/views/views_export.h"

namespace views {

class VIEWS_EXPORT ViewsTouchEditingControllerFactory
    : public ui::TouchEditingControllerFactory {
 public:
  ViewsTouchEditingControllerFactory();

  // Overridden from ui::TouchEditingControllerFactory.
  ui::TouchEditingControllerDeprecated* Create(
      ui::TouchEditable* client_view) override;
};

}  // namespace views

#endif  // UI_VIEWS_VIEWS_TOUCH_SELECTION_CONTROLLER_FACTORY_H_
