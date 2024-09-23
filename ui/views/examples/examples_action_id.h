// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_EXAMPLES_ACTION_ID_H_
#define UI_VIEWS_EXAMPLES_EXAMPLES_ACTION_ID_H_

#include "ui/actions/action_id.h"

// clang-format off
#define EXAMPLES_ACTION_IDS \
  E(kActionTest1, , kActionExamplesStart, ExamplesActionIds) \
  E(kActionTest2) \
  E(kActionTest3) \
  E(kActionAssignAction) \
  E(kActionCreateControl)
// clang-format on

namespace views::examples {

#include "ui/actions/action_id_macros.inc"

// clang-format off
enum ExamplesActionIds : actions::ActionId {
  // This should move the example action ids out of the range of any production
  // ids.
  kActionExamplesStart = actions::kActionsEnd + 0x8000,

  EXAMPLES_ACTION_IDS

  kActionExamplesEnd,
};
// clang-format on

// Note that this second include is not redundant. The second inclusion of the
// .inc file serves to undefine the macros the first inclusion defined.
#include "ui/actions/action_id_macros.inc"  // NOLINT(build/include)

}  // namespace views::examples

#endif  // UI_VIEWS_EXAMPLES_EXAMPLES_ACTION_ID_H_
