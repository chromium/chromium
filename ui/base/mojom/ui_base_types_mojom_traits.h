// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MOJOM_UI_BASE_TYPES_MOJOM_TRAITS_H_
#define UI_BASE_MOJOM_UI_BASE_TYPES_MOJOM_TRAITS_H_

#include "build/build_config.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "ui/base/mojom/ui_base_types.mojom.h"
#include "ui/base/ui_base_types.h"

namespace mojo {

template <>
struct EnumTraits<ui::mojom::DialogButton, ui::DialogButton> {
  static ui::mojom::DialogButton ToMojom(ui::DialogButton modal_type) {
    switch (modal_type) {
      case ui::DIALOG_BUTTON_NONE:
        return ui::mojom::DialogButton::NONE;
      case ui::DIALOG_BUTTON_OK:
        return ui::mojom::DialogButton::OK;
      case ui::DIALOG_BUTTON_CANCEL:
        return ui::mojom::DialogButton::CANCEL;
      default:
        NOTREACHED();
        return ui::mojom::DialogButton::NONE;
    }
  }

  static bool FromMojom(ui::mojom::DialogButton modal_type,
                        ui::DialogButton* out) {
    switch (modal_type) {
      case ui::mojom::DialogButton::NONE:
        *out = ui::DIALOG_BUTTON_NONE;
        return true;
      case ui::mojom::DialogButton::OK:
        *out = ui::DIALOG_BUTTON_OK;
        return true;
      case ui::mojom::DialogButton::CANCEL:
        *out = ui::DIALOG_BUTTON_CANCEL;
        return true;
      default:
        NOTREACHED();
        return false;
    }
  }
};

template <>
struct EnumTraits<ui::mojom::ModalType, ui::ModalType> {
  static ui::mojom::ModalType ToMojom(ui::ModalType modal_type) {
    switch (modal_type) {
      case ui::MODAL_TYPE_NONE:
        return ui::mojom::ModalType::NONE;
      case ui::MODAL_TYPE_WINDOW:
        return ui::mojom::ModalType::WINDOW;
      case ui::MODAL_TYPE_CHILD:
        return ui::mojom::ModalType::CHILD;
      case ui::MODAL_TYPE_SYSTEM:
        return ui::mojom::ModalType::SYSTEM;
      default:
        NOTREACHED();
        return ui::mojom::ModalType::NONE;
    }
  }

  static bool FromMojom(ui::mojom::ModalType modal_type, ui::ModalType* out) {
    switch (modal_type) {
      case ui::mojom::ModalType::NONE:
        *out = ui::MODAL_TYPE_NONE;
        return true;
      case ui::mojom::ModalType::WINDOW:
        *out = ui::MODAL_TYPE_WINDOW;
        return true;
      case ui::mojom::ModalType::CHILD:
        *out = ui::MODAL_TYPE_CHILD;
        return true;
      case ui::mojom::ModalType::SYSTEM:
        *out = ui::MODAL_TYPE_SYSTEM;
        return true;
      default:
        NOTREACHED();
        return false;
    }
  }
};

}  // namespace mojo

#endif  // UI_BASE_MOJOM_UI_BASE_TYPES_MOJOM_TRAITS_H_
