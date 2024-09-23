// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ash/top_row_action_keys.h"

#include <string>

#include "base/check_op.h"
#include "base/containers/fixed_flat_map.h"

namespace ui {

const char* GetTopRowActionKeyName(TopRowActionKey action) {
  // Define the mapping between an TopRowActionKey and its string name.
  // Example:
  //   TopRowActionKey::kDictation -> "Dictation".
  constexpr static auto kTopRowActionKeyToName =
      base::MakeFixedFlatMap<TopRowActionKey, const char*>({
#define TOP_ROW_ACTION_KEYS_ENTRY(action) {TopRowActionKey::k##action, #action},
#define TOP_ROW_ACTION_KEYS_LAST_ENTRY(action) \
  {TopRowActionKey::k##action, #action},
          TOP_ROW_ACTION_KEYS
#undef TOP_ROW_ACTION_KEYS_LAST_ENTRY
#undef TOP_ROW_ACTION_KEYS_ENTRY
      });
  auto iter = kTopRowActionKeyToName.find(action);
  CHECK(iter != kTopRowActionKeyToName.end());
  return iter->second;
}

}  // namespace ui
