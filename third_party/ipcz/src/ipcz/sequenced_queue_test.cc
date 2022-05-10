// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/sequenced_queue.h"

#include <string>

#include "ipcz/sequence_number.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/base/macros.h"

namespace ipcz {
namespace {

struct TestQueueTraits {
  static size_t GetElementSize(const std::string& s) { return s.size(); }
};

using TestQueue = SequencedQueue<std::string>;
using TestQueueWithSize = SequencedQueue<std::string, TestQueueTraits>;

using SequencedQueueTest = testing::Test;

TEST(SequencedQueueTest, Empty) {
  TestQueue q;
  EXPECT_TRUE(q.ExpectsMoreElements());
  EXPECT_FALSE(q.HasNextElement());

  std::string s;
  EXPECT_FALSE(q.Pop(s));
}

TEST(SequencedQueueTest, SetFinalSequenceLength) {
  TestQueue q;
  q.SetFinalSequenceLength(SequenceNumber(3));
  EXPECT_TRUE(q.ExpectsMoreElements());
  EXPECT_FALSE(q.HasNextElement());

  std::string s;
  EXPECT_FALSE(q.Pop(s));

  const std::string kEntries[] = {"zero", "one", "two"};
  EXPECT_TRUE(q.Push(SequenceNumber(2), kEntries[2]));
  EXPECT_FALSE(q.HasNextElement());
  EXPECT_FALSE(q.Pop(s));
  EXPECT_TRUE(q.ExpectsMoreElements());

  EXPECT_TRUE(q.Push(SequenceNumber(0), kEntries[0]));
  EXPECT_TRUE(q.HasNextElement());
  EXPECT_TRUE(q.ExpectsMoreElements());
  EXPECT_TRUE(q.Pop(s));
  EXPECT_EQ(kEntries[0], s);

  EXPECT_FALSE(q.HasNextElement());
  EXPECT_FALSE(q.Pop(s));
  EXPECT_TRUE(q.ExpectsMoreElements());

  EXPECT_TRUE(q.Push(SequenceNumber(1), kEntries[1]));
  EXPECT_FALSE(q.ExpectsMoreElements());
  EXPECT_TRUE(q.HasNextElement());
  EXPECT_TRUE(q.Pop(s));
  EXPECT_EQ(kEntries[1], s);
  EXPECT_FALSE(q.ExpectsMoreElements());
  EXPECT_TRUE(q.HasNextElement());
  EXPECT_TRUE(q.Pop(s));
  EXPECT_EQ(kEntries[2], s);
  EXPECT_FALSE(q.ExpectsMoreElements());
  EXPECT_FALSE(q.HasNextElement());
}

TEST(SequencedQueueTest, SequenceTooLow) {
  TestQueue q;

  const std::string kEntries[] = {"a", "b", "c"};

  std::string s;
  EXPECT_TRUE(q.Push(SequenceNumber(0), kEntries[0]));
  EXPECT_TRUE(q.Pop(s));
  EXPECT_EQ(kEntries[0], s);

  // We can't push another element for sequence number 0.
  EXPECT_FALSE(q.Push(SequenceNumber(0), kEntries[0]));

  // Out-of-order is of course fine.
  EXPECT_TRUE(q.Push(SequenceNumber(2), kEntries[2]));
  EXPECT_TRUE(q.Push(SequenceNumber(1), kEntries[1]));

  EXPECT_TRUE(q.Pop(s));
  EXPECT_EQ(kEntries[1], s);
  EXPECT_TRUE(q.Pop(s));
  EXPECT_EQ(kEntries[2], s);

  // But we can't revisit sequence number 1 or 2 either.
  EXPECT_FALSE(q.Push(SequenceNumber(2), kEntries[2]));
  EXPECT_FALSE(q.Push(SequenceNumber(1), kEntries[1]));
}

TEST(SequencedQueueTest, SequenceTooHigh) {
  TestQueue q;
  q.SetFinalSequenceLength(SequenceNumber(5));

  EXPECT_FALSE(q.Push(SequenceNumber(5), "doesn't matter"));
}

TEST(SequencedQueueTest, SparseSequence) {
  TestQueue q;

  // Push a sparse but eventually complete sequence of elements into a queue and
  // ensure that they can only be popped out in sequence-order.
  const std::string kEntries[] = {"0", "1", "2", "3", "4", "5", "6", "7",
                                  "8", "9", "a", "b", "c", "d", "e", "f"};
  const std::string* next_expected_pop = &kEntries[0];
  SequenceNumber kMessageSequence[] = {
      SequenceNumber(5),  SequenceNumber(2),  SequenceNumber(1),
      SequenceNumber(0),  SequenceNumber(4),  SequenceNumber(3),
      SequenceNumber(9),  SequenceNumber(6),  SequenceNumber(8),
      SequenceNumber(7),  SequenceNumber(10), SequenceNumber(11),
      SequenceNumber(12), SequenceNumber(15), SequenceNumber(13),
      SequenceNumber(14)};
  for (SequenceNumber n : kMessageSequence) {
    EXPECT_TRUE(q.Push(SequenceNumber(n), kEntries[n.value()]));
    std::string s;
    while (q.Pop(s)) {
      EXPECT_EQ(*next_expected_pop, s);
      ++next_expected_pop;
    }
  }

  EXPECT_EQ(std::end(kEntries), next_expected_pop);
}

TEST(SequencedQueueTest, FullyConsumed) {
  TestQueue empty_queue;
  EXPECT_FALSE(empty_queue.IsSequenceFullyConsumed());
  EXPECT_TRUE(empty_queue.SetFinalSequenceLength(SequenceNumber(0)));
  EXPECT_TRUE(empty_queue.IsSequenceFullyConsumed());

  TestQueue q;
  const std::string kEntries[] = {"a", "b", "c"};
  EXPECT_FALSE(q.IsSequenceFullyConsumed());

  EXPECT_TRUE(q.Push(SequenceNumber(0), kEntries[0]));
  EXPECT_TRUE(q.Push(SequenceNumber(1), kEntries[1]));
  EXPECT_TRUE(q.Push(SequenceNumber(2), kEntries[2]));

  EXPECT_TRUE(q.SetFinalSequenceLength(SequenceNumber(3)));
  EXPECT_FALSE(q.IsSequenceFullyConsumed());

  std::string s;
  EXPECT_TRUE(q.Pop(s));
  EXPECT_TRUE(q.Pop(s));
  EXPECT_TRUE(q.Pop(s));
  EXPECT_TRUE(q.IsSequenceFullyConsumed());
}

TEST(SequencedQueueTest, MaybeSkipSequenceNumber) {
  TestQueue q;
  const std::string kEntry = "woot";
  EXPECT_TRUE(q.MaybeSkipSequenceNumber(SequenceNumber(0)));
  EXPECT_FALSE(q.MaybeSkipSequenceNumber(SequenceNumber(0)));
  EXPECT_FALSE(q.Push(SequenceNumber(0), kEntry));
  EXPECT_TRUE(q.Push(SequenceNumber(1), kEntry));
  EXPECT_FALSE(q.MaybeSkipSequenceNumber(SequenceNumber(1)));

  std::string s;
  EXPECT_TRUE(q.Pop(s));

  // Skip ahead to SequenceNumber 4.
  EXPECT_TRUE(q.MaybeSkipSequenceNumber(SequenceNumber(2)));
  EXPECT_TRUE(q.MaybeSkipSequenceNumber(SequenceNumber(3)));
  EXPECT_FALSE(q.Push(SequenceNumber(2), kEntry));
  EXPECT_FALSE(q.Push(SequenceNumber(3), kEntry));
  EXPECT_TRUE(q.Push(SequenceNumber(4), kEntry));
  EXPECT_FALSE(q.MaybeSkipSequenceNumber(SequenceNumber(4)));

  EXPECT_TRUE(q.SetFinalSequenceLength(SequenceNumber(6)));
  EXPECT_FALSE(q.IsSequenceFullyConsumed());
  EXPECT_TRUE(q.Pop(s));
  EXPECT_FALSE(q.IsSequenceFullyConsumed());
  EXPECT_TRUE(q.MaybeSkipSequenceNumber(SequenceNumber(5)));
  EXPECT_TRUE(q.IsSequenceFullyConsumed());

  // Fully consumed queue: skipping must fail.
  EXPECT_FALSE(q.MaybeSkipSequenceNumber(SequenceNumber(6)));
}

TEST(SequencedQueueTest, Accounting) {
  TestQueueWithSize q;

  const std::string kEntries[] = {"hello", "world", "one", "two", "three"};

  // Elements not at the head of the queue are not considered to be available.
  EXPECT_TRUE(q.Push(SequenceNumber(3), kEntries[3]));
  EXPECT_EQ(0u, q.GetNumAvailableElements());
  EXPECT_EQ(0u, q.GetTotalAvailableElementSize());
  EXPECT_FALSE(q.HasNextElement());

  EXPECT_TRUE(q.Push(SequenceNumber(1), kEntries[1]));
  EXPECT_EQ(0u, q.GetNumAvailableElements());
  EXPECT_EQ(0u, q.GetTotalAvailableElementSize());
  EXPECT_FALSE(q.HasNextElement());

  // Now we'll insert at the head of the queue and we should be accounting for
  // elements 0 and 1, but still not element 3 yet.
  EXPECT_TRUE(q.Push(SequenceNumber(0), kEntries[0]));
  EXPECT_EQ(2u, q.GetNumAvailableElements());
  EXPECT_EQ(kEntries[0].size() + kEntries[1].size(),
            q.GetTotalAvailableElementSize());
  EXPECT_TRUE(q.HasNextElement());

  // Finally insert element 2, after which we should be accounting for all 4
  // elements in the queue.
  EXPECT_TRUE(q.Push(SequenceNumber(2), kEntries[2]));
  EXPECT_EQ(4u, q.GetNumAvailableElements());
  EXPECT_EQ(kEntries[0].size() + kEntries[1].size() + kEntries[2].size() +
                kEntries[3].size(),
            q.GetTotalAvailableElementSize());

  // Popping should also update the accounting properly.
  std::string s;
  EXPECT_TRUE(q.Pop(s));
  EXPECT_EQ(kEntries[0], s);
  EXPECT_EQ(3u, q.GetNumAvailableElements());
  EXPECT_EQ(kEntries[1].size() + kEntries[2].size() + kEntries[3].size(),
            q.GetTotalAvailableElementSize());

  EXPECT_TRUE(q.Pop(s));
  EXPECT_EQ(kEntries[1], s);
  EXPECT_EQ(2u, q.GetNumAvailableElements());
  EXPECT_EQ(kEntries[2].size() + kEntries[3].size(),
            q.GetTotalAvailableElementSize());

  // Insert another at the end after popping a few to verify below that pops
  // also update the tail of the leading span.
  EXPECT_TRUE(q.Push(SequenceNumber(4), kEntries[4]));

  EXPECT_TRUE(q.Pop(s));
  EXPECT_EQ(kEntries[2], s);
  EXPECT_EQ(2u, q.GetNumAvailableElements());
  EXPECT_EQ(kEntries[3].size() + kEntries[4].size(),
            q.GetTotalAvailableElementSize());

  EXPECT_TRUE(q.Pop(s));
  EXPECT_EQ(kEntries[3], s);
  EXPECT_EQ(1u, q.GetNumAvailableElements());
  EXPECT_EQ(kEntries[4].size(), q.GetTotalAvailableElementSize());

  EXPECT_TRUE(q.Pop(s));
  EXPECT_EQ(kEntries[4], s);
  EXPECT_EQ(0u, q.GetNumAvailableElements());
  EXPECT_EQ(0u, q.GetTotalAvailableElementSize());
}

}  // namespace
}  // namespace ipcz
