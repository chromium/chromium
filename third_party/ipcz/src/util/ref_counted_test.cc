// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/ref_counted.h"

#include <atomic>
#include <thread>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/synchronization/notification.h"

namespace ipcz {
namespace {

using RefCountedTest = testing::Test;

class TestObject : public RefCounted<TestObject> {
 public:
  explicit TestObject(bool& destruction_flag)
      : destruction_flag_(destruction_flag) {}

  size_t count() const { return count_.load(std::memory_order_acquire); }

  void Increment() { count_.fetch_add(1, std::memory_order_relaxed); }

 private:
  friend class RefCounted<TestObject>;

  ~TestObject() { destruction_flag_ = true; }

  bool& destruction_flag_;
  std::atomic<size_t> count_{0};
};

TEST_F(RefCountedTest, NullRef) {
  Ref<TestObject> ref;
  EXPECT_FALSE(ref);

  ref.reset();
  EXPECT_FALSE(ref);

  Ref<TestObject> other1 = ref;
  EXPECT_FALSE(ref);
  EXPECT_FALSE(other1);

  Ref<TestObject> other2 = std::move(ref);
  EXPECT_FALSE(ref);
  EXPECT_FALSE(other2);

  ref = other1;
  EXPECT_FALSE(ref);
  EXPECT_FALSE(other1);

  ref = std::move(other2);
  EXPECT_FALSE(ref);
  EXPECT_FALSE(other2);
}

TEST_F(RefCountedTest, SimpleRef) {
  bool destroyed = false;
  auto ref = MakeRefCounted<TestObject>(destroyed);
  EXPECT_TRUE(ref);
  EXPECT_FALSE(destroyed);
  ref.reset();
  EXPECT_FALSE(ref);
  EXPECT_TRUE(destroyed);
}

TEST_F(RefCountedTest, Copy) {
  bool destroyed1 = false;
  auto ref1 = MakeRefCounted<TestObject>(destroyed1);
  Ref<TestObject> other1 = ref1;
  EXPECT_TRUE(other1);
  EXPECT_FALSE(destroyed1);
  ref1.reset();
  EXPECT_FALSE(ref1);
  EXPECT_FALSE(destroyed1);
  other1.reset();
  EXPECT_FALSE(other1);
  EXPECT_TRUE(destroyed1);

  destroyed1 = false;
  bool destroyed2 = false;
  ref1 = MakeRefCounted<TestObject>(destroyed1);
  auto ref2 = MakeRefCounted<TestObject>(destroyed2);
  EXPECT_FALSE(destroyed1);
  EXPECT_FALSE(destroyed2);
  ref2 = ref1;
  EXPECT_TRUE(ref1);
  EXPECT_TRUE(ref2);
  EXPECT_EQ(ref1, ref2);
  EXPECT_FALSE(destroyed1);
  EXPECT_TRUE(destroyed2);
  ref1.reset();
  EXPECT_FALSE(ref1);
  EXPECT_FALSE(destroyed1);
  EXPECT_TRUE(destroyed2);
  ref2.reset();
  EXPECT_FALSE(ref2);
  EXPECT_TRUE(destroyed1);
}

TEST_F(RefCountedTest, Move) {
  bool destroyed1 = false;
  auto ref1 = MakeRefCounted<TestObject>(destroyed1);
  Ref<TestObject> other1 = std::move(ref1);
  EXPECT_TRUE(other1);
  EXPECT_FALSE(ref1);
  EXPECT_FALSE(destroyed1);
  other1.reset();
  EXPECT_TRUE(destroyed1);

  destroyed1 = false;
  bool destroyed2 = false;
  ref1 = MakeRefCounted<TestObject>(destroyed1);
  auto ref2 = MakeRefCounted<TestObject>(destroyed2);
  EXPECT_FALSE(destroyed1);
  EXPECT_FALSE(destroyed2);
  ref2 = std::move(ref1);
  EXPECT_FALSE(ref1);
  EXPECT_TRUE(ref2);
  EXPECT_FALSE(destroyed1);
  EXPECT_TRUE(destroyed2);
  ref2.reset();
  EXPECT_TRUE(destroyed1);
}

TEST_F(RefCountedTest, ThreadSafe) {
  bool destroyed = false;
  auto counter = MakeRefCounted<TestObject>(destroyed);

  constexpr size_t kIncrementsPerThread = 10000;
  constexpr size_t kNumThreads = 64;
  auto incrementer = [](Ref<TestObject> ref) {
    for (size_t i = 0; i < kIncrementsPerThread; ++i) {
      Ref<TestObject> copy = ref;
      copy->Increment();
    }
  };

  std::vector<std::thread> threads;
  for (size_t i = 0; i < kNumThreads; ++i) {
    threads.emplace_back(incrementer, counter);
  }

  for (std::thread& thread : threads) {
    thread.join();
  }

  EXPECT_FALSE(destroyed);
  EXPECT_EQ(kNumThreads * kIncrementsPerThread, counter->count());
  counter.reset();
  EXPECT_TRUE(destroyed);
}

}  // namespace
}  // namespace ipcz
