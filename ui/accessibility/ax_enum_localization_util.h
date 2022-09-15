// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_ENUM_LOCALIZATION_UTIL_H_
#define UI_ACCESSIBILITY_AX_ENUM_LOCALIZATION_UTIL_H_

#include <string>

#include "ui/accessibility/ax_base_export.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"

namespace ui {

// Returns a localized string that corresponds to the name of the given action.
AX_BASE_EXPORT std::string ToLocalizedString(
    ax::mojom::DefaultActionVerb action_verb);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_ENUM_LOCALIZATION_UTIL_H_
