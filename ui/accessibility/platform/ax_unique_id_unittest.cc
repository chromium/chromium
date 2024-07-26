// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/accessibility/platform/ax_unique_id.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

TEST(AXPlatformUniqueIdTest, IdsAreUnique) {
  AXUniqueId id1 = AXUniqueId::Create();
  AXUniqueId id2 = AXUniqueId::Create();
  EXPECT_FALSE(id1 == id2);
  EXPECT_GT(id2.Get(), id1.Get());
}

TEST(AXPlatformUniqueIdTest, IdsAreMovable) {
  AXUniqueId id1 = AXUniqueId::Create();
  auto id_value = id1.Get();

  AXUniqueId id2 = std::move(id1);
  EXPECT_EQ(id2.Get(), id_value);

  AXUniqueId id3(std::move(id2));
  EXPECT_EQ(id3.Get(), id_value);
}

namespace {

constexpr int32_t kMaxId = 100;

AXUniqueId CreateSmallBankUniqueId() {
  return AXUniqueId::CreateForTest(kMaxId);
}

}  // namespace

TEST(AXPlatformUniqueIdTest, UnassignedIdsAreReused) {
  // Create a bank of ids that uses up all available ids.
  // Then remove an id and replace with a new one. Since it's the only
  // slot available, the id will end up having the same value, rather than
  // starting over at 1.
  std::unique_ptr<AXUniqueId> ids[kMaxId];

  for (auto& id : ids) {
    id = std::make_unique<AXUniqueId>(CreateSmallBankUniqueId());
  }

  static int kIdToReplace = 10;
  int32_t expected_id = ids[kIdToReplace]->Get();

  // Delete one of the ids and replace with a new one.
  ids[kIdToReplace] = nullptr;
  ids[kIdToReplace] = std::make_unique<AXUniqueId>(CreateSmallBankUniqueId());

  // Expect that the original Id gets reused.
  EXPECT_EQ(ids[kIdToReplace]->Get(), expected_id);
}

TEST(AXPlatformUniqueIdTest, DoesCreateCorrectId) {
  constexpr int kLargerThanMaxId = kMaxId * 2;
  std::unique_ptr<AXUniqueId> ids[kLargerThanMaxId];
  // Creates and releases to fill up the internal static counter.
  for (int i = 0; i < kLargerThanMaxId; i++) {
    ids[i] = std::make_unique<AXUniqueId>(AXUniqueId::Create());
  }
  for (int i = 0; i < kLargerThanMaxId; i++) {
    ids[i].reset(nullptr);
  }
  // Creates an unique id whose max value is less than the internal
  // static counter.
  std::unique_ptr<AXUniqueId> unique_id =
      std::make_unique<AXUniqueId>(CreateSmallBankUniqueId());

  EXPECT_LE(unique_id->Get(), kMaxId);
}

TEST(AXPlatformUniqueIdTest, DefaultPlatformNodeIdIsInvalid) {
  AXPlatformNodeId default_id;
  ASSERT_EQ(default_id, kInvalidAXNodeID);
}

}  // namespace ui
