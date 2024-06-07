// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ash/top_row_action_keys.h"

#include "base/containers/fixed_flat_map.h"
#include "base/test/metrics/histogram_enum_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace {

constexpr auto kTopRowActionKeyToName =
    base::MakeFixedFlatMap<TopRowActionKey, const char*>({
#define TOP_ROW_ACTION_KEYS_ENTRY(action) {TopRowActionKey::k##action, #action},
#define TOP_ROW_ACTION_KEYS_LAST_ENTRY(action) \
  {TopRowActionKey::k##action, #action},
        TOP_ROW_ACTION_KEYS
#undef TOP_ROW_ACTION_KEYS_LAST_ENTRY
#undef TOP_ROW_ACTION_KEYS_ENTRY
    });

}  // namespace

TEST(TopRowActionKeysTest, CheckHistogramEnum) {
  const auto enums =
      base::ReadEnumFromEnumsXml("KeyboardTopRowActionKeys", "chromeos");
  ASSERT_TRUE(enums);
  // The number of enums in the histogram entry should be equal to the number of
  // enums in the C++ file.
  EXPECT_EQ(enums->size(), kTopRowActionKeyToName.size());

  for (const auto& entry : *enums) {
    // Check that the C++ file has a definition equal to the histogram file.
    EXPECT_EQ(entry.second, kTopRowActionKeyToName.at(
                                static_cast<TopRowActionKey>(entry.first)))
        << "Enum entry name: " << entry.second
        << " in enums.xml is different from enum entry name: "
        << kTopRowActionKeyToName.at(static_cast<TopRowActionKey>(entry.first))
        << " in C++ file";
  }
}

}  // namespace ui
