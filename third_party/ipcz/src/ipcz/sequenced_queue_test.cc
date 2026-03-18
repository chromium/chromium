// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ipcz/sequence_number.h"
#include "ipcz/sequenced_queue.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "util/unsafe_buffers.h"

namespace ipcz {

template <typename T, typename ElementTraits>
struct SequencedQueueTestAccessor {
  using Queue = SequencedQueue<T, ElementTraits>;

  static auto& entries(Queue& queue) { return queue.entries_; }
  static size_t front_index(const Queue& queue) { return queue.front_index_; }
};

namespace {

struct TestQueueTraits {
  static size_t GetElementSize(const std::string& s) { return s.size(); }
};

using testing::ElementsAre;
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

TEST(SequencedQueueTest, ForceTerminateSequence) {
  TestQueue q;
  q.SetFinalSequenceLength(SequenceNumber(3));
  EXPECT_TRUE(q.ExpectsMoreElements());
  EXPECT_FALSE(q.HasNextElement());

  // Push elements 0 and 2, leaving the current sequence length at 1, due to the
  // gap in element 1.
  EXPECT_TRUE(q.Push(SequenceNumber(0), "woot."));
  EXPECT_TRUE(q.Push(SequenceNumber(2), "woot!"));
  EXPECT_TRUE(q.ExpectsMoreElements());
  EXPECT_TRUE(q.HasNextElement());
  EXPECT_EQ(SequenceNumber(1), q.GetCurrentSequenceLength());

  // We can't normally change the final sequence length once set.
  EXPECT_FALSE(q.SetFinalSequenceLength(SequenceNumber(4)));
  EXPECT_FALSE(q.SetFinalSequenceLength(SequenceNumber(0)));
  EXPECT_FALSE(q.SetFinalSequenceLength(SequenceNumber(1)));

  // But we can still force it to terminate at its current length. Now the gap
  // at element 1 is irrelevant, and element 0 alone is the complete sequence.
  std::vector<std::string> removed_elements = q.ForceTerminateSequence();
  EXPECT_THAT(removed_elements, ElementsAre("woot!"));
  EXPECT_FALSE(q.ExpectsMoreElements());
  EXPECT_TRUE(q.HasNextElement());
  EXPECT_FALSE(q.Push(SequenceNumber(1), "woot?"));
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
    IPCZ_UNSAFE_TODO(
        EXPECT_TRUE(q.Push(SequenceNumber(n), kEntries[n.value()])));
    std::string s;
    while (q.Pop(s)) {
      EXPECT_EQ(*next_expected_pop, s);
      IPCZ_UNSAFE_TODO(++next_expected_pop);
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

TEST(SequencedQueueTest, SkipElement) {
  TestQueueWithSize q;
  const std::string kEntry = "woot";

  // Skipping an element should update accounting appropriately.
  EXPECT_TRUE(q.SkipElement(SequenceNumber(0)));
  EXPECT_EQ(0u, q.GetTotalAvailableElementSize());

  // We can't skip or push an element that's already been skipped.
  EXPECT_FALSE(q.SkipElement(SequenceNumber(0)));
  EXPECT_FALSE(q.Push(SequenceNumber(0), kEntry));

  // And we can't skip an element that's already been pushed.
  EXPECT_TRUE(q.Push(SequenceNumber(1), kEntry));
  EXPECT_FALSE(q.SkipElement(SequenceNumber(1)));
  EXPECT_EQ(kEntry.size(), q.GetTotalAvailableElementSize());

  std::string s;
  EXPECT_TRUE(q.Pop(s));
  EXPECT_EQ(0u, q.GetTotalAvailableElementSize());

  // Skip ahead past SequenceNumber 2 and 3.
  EXPECT_TRUE(q.SkipElement(SequenceNumber(2)));
  EXPECT_EQ(0u, q.GetTotalAvailableElementSize());
  EXPECT_TRUE(q.SkipElement(SequenceNumber(3)));
  EXPECT_EQ(0u, q.GetTotalAvailableElementSize());

  // SequenceNumber 4 can now be pushed while 2 and 3 cannot.
  EXPECT_FALSE(q.Push(SequenceNumber(2), kEntry));
  EXPECT_FALSE(q.Push(SequenceNumber(3), kEntry));
  EXPECT_TRUE(q.Push(SequenceNumber(4), kEntry));
  EXPECT_FALSE(q.SkipElement(SequenceNumber(4)));
  EXPECT_EQ(kEntry.size(), q.GetTotalAvailableElementSize());

  // Cap the sequence at 6 elements and verify that accounting remains intact
  // when we skip the last element.
  EXPECT_TRUE(q.SetFinalSequenceLength(SequenceNumber(6)));
  EXPECT_FALSE(q.IsSequenceFullyConsumed());
  EXPECT_TRUE(q.Pop(s));
  EXPECT_FALSE(q.IsSequenceFullyConsumed());
  EXPECT_TRUE(q.SkipElement(SequenceNumber(5)));
  EXPECT_EQ(0u, q.GetTotalAvailableElementSize());
  EXPECT_TRUE(q.IsSequenceFullyConsumed());

  // Fully consumed queue: skipping must fail.
  EXPECT_FALSE(q.SkipElement(SequenceNumber(6)));
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

TEST(SequencedQueueTest, GrowWithCompactionWithoutCompaction) {
  TestQueue q;
  using Accessor =
      SequencedQueueTestAccessor<std::string,
                                 DefaultSequencedQueueTraits<std::string>>;

  // `front_index_ == 0`, so growth cannot reclaim any front slack. Pushing an
  // entry just beyond the current logical extent must grow storage without
  // entering either compaction path.
  Accessor::entries(q).reserve(4);
  EXPECT_TRUE(q.Push(SequenceNumber(3), "3"));

  const size_t initial_capacity = Accessor::entries(q).capacity();
  const size_t initial_size = Accessor::entries(q).size();
  EXPECT_EQ(0u, Accessor::front_index(q));
  EXPECT_EQ(4u, Accessor::entries(q).size());

  const auto old_data = Accessor::entries(q).data();
  EXPECT_TRUE(q.Push(SequenceNumber(4), "4"));

  EXPECT_EQ(0u, Accessor::front_index(q));
  EXPECT_EQ(initial_size + 1, Accessor::entries(q).size());
  EXPECT_NE(old_data, Accessor::entries(q).data());
  EXPECT_GT(Accessor::entries(q).capacity(), initial_capacity);

  std::string popped;
  EXPECT_FALSE(q.Pop(popped));
  EXPECT_TRUE(q.Push(SequenceNumber(0), "0"));
  EXPECT_TRUE(q.Pop(popped));
  EXPECT_EQ("0", popped);
}

TEST(SequencedQueueTest, GrowWithCompactionInPlace) {
  TestQueue q;
  using Accessor =
      SequencedQueueTestAccessor<std::string,
                                 DefaultSequencedQueueTraits<std::string>>;

  // Pop once to create front slack, then push far enough to require growth.
  // After compaction the required size still fits in the existing capacity, so
  // `GrowWithCompaction()` should compact in place.
  Accessor::entries(q).reserve(8);
  ASSERT_TRUE(q.Push(SequenceNumber(0), "0"));
  ASSERT_TRUE(q.Push(SequenceNumber(5), "5"));

  std::string popped;
  ASSERT_TRUE(q.Pop(popped));
  EXPECT_EQ("0", popped);
  EXPECT_EQ(1u, Accessor::front_index(q));
  const size_t old_capacity = Accessor::entries(q).capacity();
  const auto old_data = Accessor::entries(q).data();

  ASSERT_TRUE(q.Push(SequenceNumber(7), "7"));

  EXPECT_EQ(old_data, Accessor::entries(q).data());
  EXPECT_EQ(old_capacity, Accessor::entries(q).capacity());
  EXPECT_GT(Accessor::entries(q).size(), 6u);

  EXPECT_FALSE(q.Push(SequenceNumber(0), "again"));
  EXPECT_TRUE(q.Push(SequenceNumber(1), "1"));
  EXPECT_TRUE(q.Pop(popped));
  EXPECT_EQ("1", popped);
}

TEST(SequencedQueueTest, GrowWithCompactionWithoutCompactionWithFrontSlack) {
  TestQueue q;
  using Accessor =
      SequencedQueueTestAccessor<std::string,
                                 DefaultSequencedQueueTraits<std::string>>;

  // Pop once to create front slack, but keep enough spare capacity that the
  // next push only needs `resize(required)`. This should bypass compaction even
  // though `front_index_ > 0`.
  Accessor::entries(q).reserve(8);
  ASSERT_TRUE(q.Push(SequenceNumber(0), "0"));
  ASSERT_TRUE(q.Push(SequenceNumber(5), "5"));

  std::string popped;
  ASSERT_TRUE(q.Pop(popped));
  EXPECT_EQ("0", popped);
  EXPECT_EQ(1u, Accessor::front_index(q));

  const auto old_data = Accessor::entries(q).data();
  const size_t old_capacity = Accessor::entries(q).capacity();
  const size_t old_size = Accessor::entries(q).size();

  ASSERT_TRUE(q.Push(SequenceNumber(6), "6"));

  EXPECT_EQ(1u, Accessor::front_index(q));
  EXPECT_EQ(old_data, Accessor::entries(q).data());
  EXPECT_EQ(old_capacity, Accessor::entries(q).capacity());
  EXPECT_EQ(old_size + 1, Accessor::entries(q).size());

  EXPECT_TRUE(q.Push(SequenceNumber(1), "1"));
  EXPECT_TRUE(q.Pop(popped));
  EXPECT_EQ("1", popped);
}

TEST(SequencedQueueTest, GrowWithCompactionToNewBuffer) {
  TestQueue q;
  using Accessor =
      SequencedQueueTestAccessor<std::string,
                                 DefaultSequencedQueueTraits<std::string>>;

  // Pop once to create front slack, then push far enough that compaction alone
  // still cannot satisfy the required size. This must take the new-buffer
  // compaction path.
  Accessor::entries(q).reserve(4);
  ASSERT_TRUE(q.Push(SequenceNumber(0), "0"));
  ASSERT_TRUE(q.Push(SequenceNumber(3), "3"));

  std::string popped;
  ASSERT_TRUE(q.Pop(popped));
  EXPECT_EQ("0", popped);
  EXPECT_EQ(1u, Accessor::front_index(q));

  const size_t full_capacity = Accessor::entries(q).capacity();

  const auto old_data = Accessor::entries(q).data();
  ASSERT_TRUE(q.Push(SequenceNumber(5), "5"));

  EXPECT_EQ(0u, Accessor::front_index(q));
  EXPECT_NE(old_data, Accessor::entries(q).data());
  EXPECT_EQ(5u, Accessor::entries(q).size());
  EXPECT_GT(Accessor::entries(q).capacity(), full_capacity);

  EXPECT_FALSE(q.Push(SequenceNumber(0), "again"));
  EXPECT_TRUE(q.Push(SequenceNumber(1), "1"));
  EXPECT_TRUE(q.Push(SequenceNumber(2), "2"));
  EXPECT_TRUE(q.Pop(popped));
  EXPECT_EQ("1", popped);
  EXPECT_TRUE(q.Pop(popped));
  EXPECT_EQ("2", popped);
  EXPECT_TRUE(q.Pop(popped));
  EXPECT_EQ("3", popped);
}

}  // namespace
}  // namespace ipcz
