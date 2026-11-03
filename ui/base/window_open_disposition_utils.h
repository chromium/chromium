// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WINDOW_OPEN_DISPOSITION_UTILS_H_
#define UI_BASE_WINDOW_OPEN_DISPOSITION_UTILS_H_

#include "base/component_export.h"
#include "ui/base/mojom/window_open_disposition.mojom-shared.h"

namespace ui {

// Translates event flags from a click on a link into the user's desired window
// disposition.  For example, a middle click would mean to open a background
// tab.  `disposition_for_current_tab` is the disposition to return if the flags
// suggest opening in the current tab; for example, a caller could set this to
// `kNewForegroundTab` to prevent a click from overwriting the current tab by
// default.
COMPONENT_EXPORT(UI_BASE)
ui::mojom::WindowOpenDisposition DispositionFromClick(
    bool middle_button,
    bool alt_key,
    bool ctrl_key,
    bool meta_key,
    bool shift_key,
    ui::mojom::WindowOpenDisposition disposition_for_current_tab =
        ui::mojom::WindowOpenDisposition::kCurrentTab);

// As with DispositionFromClick(), but using |event_flags| as in ui::MouseEvent.
COMPONENT_EXPORT(UI_BASE)
ui::mojom::WindowOpenDisposition DispositionFromEventFlags(
    int event_flags,
    ui::mojom::WindowOpenDisposition disposition_for_current_tab =
        ui::mojom::WindowOpenDisposition::kCurrentTab);

}  // namespace ui

#endif  // UI_BASE_WINDOW_OPEN_DISPOSITION_UTILS_H_
