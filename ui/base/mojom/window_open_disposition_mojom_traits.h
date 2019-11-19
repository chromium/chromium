// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MOJOM_WINDOW_OPEN_DISPOSITION_MOJOM_TRAITS_H_
#define UI_BASE_MOJOM_WINDOW_OPEN_DISPOSITION_MOJOM_TRAITS_H_

#include "base/logging.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/base/window_open_disposition.h"

namespace mojo {

template <>
struct EnumTraits<ui::mojom::WindowOpenDisposition, WindowOpenDisposition> {
  static ui::mojom::WindowOpenDisposition ToMojom(
      WindowOpenDisposition disposition) {
    switch (disposition) {
      case WindowOpenDisposition::UNKNOWN:
        return ui::mojom::WindowOpenDisposition::UNKNOWN;
      case WindowOpenDisposition::CURRENT_TAB:
        return ui::mojom::WindowOpenDisposition::CURRENT_TAB;
      case WindowOpenDisposition::SINGLETON_TAB:
        return ui::mojom::WindowOpenDisposition::SINGLETON_TAB;
      case WindowOpenDisposition::NEW_FOREGROUND_TAB:
        return ui::mojom::WindowOpenDisposition::NEW_FOREGROUND_TAB;
      case WindowOpenDisposition::NEW_BACKGROUND_TAB:
        return ui::mojom::WindowOpenDisposition::NEW_BACKGROUND_TAB;
      case WindowOpenDisposition::NEW_POPUP:
        return ui::mojom::WindowOpenDisposition::NEW_POPUP;
      case WindowOpenDisposition::NEW_WINDOW:
        return ui::mojom::WindowOpenDisposition::NEW_WINDOW;
      case WindowOpenDisposition::SAVE_TO_DISK:
        return ui::mojom::WindowOpenDisposition::SAVE_TO_DISK;
      case WindowOpenDisposition::OFF_THE_RECORD:
        return ui::mojom::WindowOpenDisposition::OFF_THE_RECORD;
      case WindowOpenDisposition::IGNORE_ACTION:
        return ui::mojom::WindowOpenDisposition::IGNORE_ACTION;
      default:
        NOTREACHED();
        return ui::mojom::WindowOpenDisposition::UNKNOWN;
    }
  }

  static bool FromMojom(ui::mojom::WindowOpenDisposition disposition,
                        WindowOpenDisposition* out) {
    switch (disposition) {
      case ui::mojom::WindowOpenDisposition::UNKNOWN:
        *out = WindowOpenDisposition::UNKNOWN;
        return true;
      case ui::mojom::WindowOpenDisposition::CURRENT_TAB:
        *out = WindowOpenDisposition::CURRENT_TAB;
        return true;
      case ui::mojom::WindowOpenDisposition::SINGLETON_TAB:
        *out = WindowOpenDisposition::SINGLETON_TAB;
        return true;
      case ui::mojom::WindowOpenDisposition::NEW_FOREGROUND_TAB:
        *out = WindowOpenDisposition::NEW_FOREGROUND_TAB;
        return true;
      case ui::mojom::WindowOpenDisposition::NEW_BACKGROUND_TAB:
        *out = WindowOpenDisposition::NEW_BACKGROUND_TAB;
        return true;
      case ui::mojom::WindowOpenDisposition::NEW_POPUP:
        *out = WindowOpenDisposition::NEW_POPUP;
        return true;
      case ui::mojom::WindowOpenDisposition::NEW_WINDOW:
        *out = WindowOpenDisposition::NEW_WINDOW;
        return true;
      case ui::mojom::WindowOpenDisposition::SAVE_TO_DISK:
        *out = WindowOpenDisposition::SAVE_TO_DISK;
        return true;
      case ui::mojom::WindowOpenDisposition::OFF_THE_RECORD:
        *out = WindowOpenDisposition::OFF_THE_RECORD;
        return true;
      case ui::mojom::WindowOpenDisposition::IGNORE_ACTION:
        *out = WindowOpenDisposition::IGNORE_ACTION;
        return true;
      default:
        NOTREACHED();
        return false;
    }
  }
};

}  // namespace mojo

#endif  // UI_BASE_MOJOM_WINDOW_OPEN_DISPOSITION_MOJOM_TRAITS_H_
