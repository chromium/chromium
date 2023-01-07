// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_enum_localization_util.h"

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ax_strings.h"

namespace ui {

std::string ToLocalizedString(ax::mojom::DefaultActionVerb action_verb) {
  switch (action_verb) {
    case ax::mojom::DefaultActionVerb::kNone:
      return "";
    case ax::mojom::DefaultActionVerb::kActivate:
      return l10n_util::GetStringUTF8(IDS_AX_ACTIVATE_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kCheck:
      return l10n_util::GetStringUTF8(IDS_AX_CHECK_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kClick:
      return l10n_util::GetStringUTF8(IDS_AX_CLICK_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kClickAncestor:
      return l10n_util::GetStringUTF8(IDS_AX_CLICK_ANCESTOR_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kJump:
      return l10n_util::GetStringUTF8(IDS_AX_JUMP_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kOpen:
      return l10n_util::GetStringUTF8(IDS_AX_OPEN_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kPress:
      return l10n_util::GetStringUTF8(IDS_AX_PRESS_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kSelect:
      return l10n_util::GetStringUTF8(IDS_AX_SELECT_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kUncheck:
      return l10n_util::GetStringUTF8(IDS_AX_UNCHECK_ACTION_VERB);
  }

  return "";
}

}  // namespace ui
