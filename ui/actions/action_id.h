// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACTIONS_ACTION_ID_H_
#define UI_ACTIONS_ACTION_ID_H_

// clang-format off
#define CROSS_PLATFORM_ACTION_IDS \
  E(kActionCut, kActionsStart, kActionsStart, actions) \
  E(kActionCopy, , actions) \
  E(kActionPaste, , actions)

#define PLATFORM_SPECIFIC_ACTION_IDS

#define ACTION_IDS \
  CROSS_PLATFORM_ACTION_IDS \
  PLATFORM_SPECIFIC_ACTION_IDS
// clang-format on

namespace actions {

#include "ui/actions/action_id_macros.inc"

using ActionId = int;
// clang-format off
enum ActionIds : ActionId {
  kActionsStart = 0,

  ACTION_IDS

  // Embedders must start action IDs from this value.
  kActionsEnd,
};
// clang-format on

// Note that this second include is not redundant. The second inclusion of the
// .inc file serves to undefine the macros the first inclusion defined.
#include "ui/actions/action_id_macros.inc"  // NOLINT(build/include)

}  // namespace actions

#endif  // UI_ACTIONS_ACTION_ID_H_
