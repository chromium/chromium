// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/safe_math.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ipcz {
namespace {

template <typename T>
uint64_t AsUint64(T value) {
  return static_cast<uint64_t>(value);
}

TEST(SafeMathTest, SaturatedCast) {
  const uint32_t kMaxUint32 = 0xffffffff;
  const uint64_t kSmallUint64 = 0x12345678;
  const uint64_t kLargeUint64 = 0x123456789abcull;

  // Casting to a smaller type within its range yields the same value.
  EXPECT_EQ(kSmallUint64, AsUint64(saturated_cast<uint32_t>(kSmallUint64)));

  // Casting to a smaller type outside of its range yields the max value for
  // the destination type.
  EXPECT_EQ(kMaxUint32, saturated_cast<uint32_t>(kLargeUint64));

  // Casting to a larger type always yields the same value.
  EXPECT_EQ(AsUint64(kMaxUint32), saturated_cast<uint64_t>(kMaxUint32));
}

}  // namespace
}  // namespace ipcz
