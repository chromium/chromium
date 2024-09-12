// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MOJOM_UI_BASE_TYPES_MOJOM_TRAITS_H_
#define UI_BASE_MOJOM_UI_BASE_TYPES_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "ui/base/mojom/ui_base_types.mojom.h"
#include "ui/base/ui_base_types.h"

namespace mojo {

template <>
struct EnumTraits<ui::mojom::MenuSourceType, ui::MenuSourceType> {
  static ui::mojom::MenuSourceType ToMojom(ui::MenuSourceType modal_type) {
    switch (modal_type) {
      case ui::MENU_SOURCE_NONE:
        return ui::mojom::MenuSourceType::NONE;
      case ui::MENU_SOURCE_MOUSE:
        return ui::mojom::MenuSourceType::MOUSE;
      case ui::MENU_SOURCE_KEYBOARD:
        return ui::mojom::MenuSourceType::KEYBOARD;
      case ui::MENU_SOURCE_TOUCH:
        return ui::mojom::MenuSourceType::TOUCH;
      case ui::MENU_SOURCE_TOUCH_EDIT_MENU:
        return ui::mojom::MenuSourceType::TOUCH_EDIT_MENU;
      case ui::MENU_SOURCE_LONG_PRESS:
        return ui::mojom::MenuSourceType::LONG_PRESS;
      case ui::MENU_SOURCE_LONG_TAP:
        return ui::mojom::MenuSourceType::LONG_TAP;
      case ui::MENU_SOURCE_TOUCH_HANDLE:
        return ui::mojom::MenuSourceType::TOUCH_HANDLE;
      case ui::MENU_SOURCE_STYLUS:
        return ui::mojom::MenuSourceType::STYLUS;
      case ui::MENU_SOURCE_ADJUST_SELECTION:
        return ui::mojom::MenuSourceType::ADJUST_SELECTION;
      case ui::MENU_SOURCE_ADJUST_SELECTION_RESET:
        return ui::mojom::MenuSourceType::ADJUST_SELECTION_RESET;
    }
    NOTREACHED();
  }

  static bool FromMojom(ui::mojom::MenuSourceType modal_type,
                        ui::MenuSourceType* out) {
    switch (modal_type) {
      case ui::mojom::MenuSourceType::NONE:
        *out = ui::MENU_SOURCE_NONE;
        return true;
      case ui::mojom::MenuSourceType::MOUSE:
        *out = ui::MENU_SOURCE_MOUSE;
        return true;
      case ui::mojom::MenuSourceType::KEYBOARD:
        *out = ui::MENU_SOURCE_KEYBOARD;
        return true;
      case ui::mojom::MenuSourceType::TOUCH:
        *out = ui::MENU_SOURCE_TOUCH;
        return true;
      case ui::mojom::MenuSourceType::TOUCH_EDIT_MENU:
        *out = ui::MENU_SOURCE_TOUCH_EDIT_MENU;
        return true;
      case ui::mojom::MenuSourceType::LONG_PRESS:
        *out = ui::MENU_SOURCE_LONG_PRESS;
        return true;
      case ui::mojom::MenuSourceType::LONG_TAP:
        *out = ui::MENU_SOURCE_LONG_TAP;
        return true;
      case ui::mojom::MenuSourceType::TOUCH_HANDLE:
        *out = ui::MENU_SOURCE_TOUCH_HANDLE;
        return true;
      case ui::mojom::MenuSourceType::STYLUS:
        *out = ui::MENU_SOURCE_STYLUS;
        return true;
      case ui::mojom::MenuSourceType::ADJUST_SELECTION:
        *out = ui::MENU_SOURCE_ADJUST_SELECTION;
        return true;
      case ui::mojom::MenuSourceType::ADJUST_SELECTION_RESET:
        *out = ui::MENU_SOURCE_ADJUST_SELECTION_RESET;
        return true;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // UI_BASE_MOJOM_UI_BASE_TYPES_MOJOM_TRAITS_H_
