// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MOJOM_WINDOW_OPEN_DISPOSITION_MOJOM_TRAITS_H_
#define UI_BASE_MOJOM_WINDOW_OPEN_DISPOSITION_MOJOM_TRAITS_H_

#include "base/notreached.h"
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
      case WindowOpenDisposition::NEW_PICTURE_IN_PICTURE:
        return ui::mojom::WindowOpenDisposition::NEW_PICTURE_IN_PICTURE;
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
      case WindowOpenDisposition::SWITCH_TO_TAB:
        return ui::mojom::WindowOpenDisposition::SWITCH_TO_TAB;
      case WindowOpenDisposition::NEW_SPLIT_VIEW:
        return ui::mojom::WindowOpenDisposition::NEW_SPLIT_VIEW;
    }
    NOTREACHED();
  }

  static WindowOpenDisposition FromMojom(
      ui::mojom::WindowOpenDisposition disposition) {
    switch (disposition) {
      case ui::mojom::WindowOpenDisposition::UNKNOWN:
        return WindowOpenDisposition::UNKNOWN;
      case ui::mojom::WindowOpenDisposition::CURRENT_TAB:
        return WindowOpenDisposition::CURRENT_TAB;
      case ui::mojom::WindowOpenDisposition::SINGLETON_TAB:
        return WindowOpenDisposition::SINGLETON_TAB;
      case ui::mojom::WindowOpenDisposition::NEW_FOREGROUND_TAB:
        return WindowOpenDisposition::NEW_FOREGROUND_TAB;
      case ui::mojom::WindowOpenDisposition::NEW_BACKGROUND_TAB:
        return WindowOpenDisposition::NEW_BACKGROUND_TAB;
      case ui::mojom::WindowOpenDisposition::NEW_PICTURE_IN_PICTURE:
        return WindowOpenDisposition::NEW_PICTURE_IN_PICTURE;
      case ui::mojom::WindowOpenDisposition::NEW_POPUP:
        return WindowOpenDisposition::NEW_POPUP;
      case ui::mojom::WindowOpenDisposition::NEW_WINDOW:
        return WindowOpenDisposition::NEW_WINDOW;
      case ui::mojom::WindowOpenDisposition::SAVE_TO_DISK:
        return WindowOpenDisposition::SAVE_TO_DISK;
      case ui::mojom::WindowOpenDisposition::OFF_THE_RECORD:
        return WindowOpenDisposition::OFF_THE_RECORD;
      case ui::mojom::WindowOpenDisposition::IGNORE_ACTION:
        return WindowOpenDisposition::IGNORE_ACTION;
      case ui::mojom::WindowOpenDisposition::SWITCH_TO_TAB:
        return WindowOpenDisposition::SWITCH_TO_TAB;
      case ui::mojom::WindowOpenDisposition::NEW_SPLIT_VIEW:
        return WindowOpenDisposition::NEW_SPLIT_VIEW;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // UI_BASE_MOJOM_WINDOW_OPEN_DISPOSITION_MOJOM_TRAITS_H_
