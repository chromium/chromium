// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACTIONS_ACTION_UTILS_H_
#define UI_ACTIONS_ACTION_UTILS_H_

#include "ui/base/metadata/metadata_utils.h"

namespace actions {

template <typename A>
bool IsActionItemClass(
    actions::ActionItem* action_item) {
  return ui::metadata::IsClass<A, actions::ActionItem>(action_item);
}

template <typename A>
std::unique_ptr<A> ToActionItemClass(
    std::unique_ptr<actions::ActionItem> action_item) {
  CHECK(IsActionItemClass<A>(action_item.get()));
  return std::unique_ptr<A>(static_cast<A*>(action_item.release()));
}

}  // namespace actions

#endif  // UI_ACTIONS_ACTION_UTILS_H_
