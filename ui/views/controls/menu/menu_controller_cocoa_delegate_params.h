// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_CONTROLLER_COCOA_DELEGATE_PARAMS_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_CONTROLLER_COCOA_DELEGATE_PARAMS_H_

#include "components/remote_cocoa/common/menu.mojom-forward.h"
#include "ui/views/views_export.h"

namespace views {
class Widget;

// Create and populate a MenuControllerParams struct with the right parameters
// for a menu associated with `widget`.
VIEWS_EXPORT remote_cocoa::mojom::MenuControllerParamsPtr
MenuControllerParamsForWidget(Widget* widget);

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_CONTROLLER_COCOA_DELEGATE_PARAMS_H_
