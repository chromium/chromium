// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/parcel_queue.h"

#include <vector>

#include "ipcz/ipcz.h"
#include "ipcz/sequence_number.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/span.h"

namespace ipcz {
namespace {

using ParcelQueueTest = testing::Test;

TEST(ParcelQueueTest, Consume) {
  ParcelQueue q;

  std::vector<Ref<APIObject>> objects(2);

  Parcel p0;
  p0.SetInlinedData(std::vector<uint8_t>({0, 1, 2, 3}));
  p0.SetObjects(std::move(objects));
  q.Push(SequenceNumber(0), std::move(p0));

  Parcel p1;
  p1.SetInlinedData(std::vector<uint8_t>({2, 3, 5}));
  q.Push(SequenceNumber(1), std::move(p1));

  EXPECT_EQ(2u, q.GetNumAvailableElements());
  EXPECT_EQ(7u, q.GetTotalAvailableElementSize());

  // Consume a single byte.
  EXPECT_TRUE(q.Consume(1, {}));
  EXPECT_EQ(2u, q.GetNumAvailableElements());
  EXPECT_EQ(6u, q.GetTotalAvailableElementSize());

  // Consume the remainder of the first parcel, except for one of the two
  // handles it holds.
  IpczHandle handle;
  EXPECT_TRUE(q.Consume(3, {&handle, 1}));
  EXPECT_EQ(2u, q.GetNumAvailableElements());
  EXPECT_EQ(3u, q.GetTotalAvailableElementSize());

  // Finally consume the first of the first parcel.
  EXPECT_TRUE(q.Consume(0, {&handle, 1}));
  EXPECT_EQ(1u, q.GetNumAvailableElements());
  EXPECT_EQ(3u, q.GetTotalAvailableElementSize());

  // Consume the whole second parcel.
  EXPECT_TRUE(q.Consume(3, {}));
  EXPECT_EQ(0u, q.GetNumAvailableElements());
  EXPECT_EQ(0u, q.GetTotalAvailableElementSize());

  EXPECT_FALSE(q.Consume(0, {}));
}

}  // namespace
}  // namespace ipcz
