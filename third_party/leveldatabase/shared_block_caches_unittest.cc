// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/leveldatabase/shared_block_caches.h"

#include <optional>
#include <string>

#include "base/memory_coordinator/memory_coordinator_features.h"
#include "base/memory_coordinator/memory_limit.h"
#include "base/memory_coordinator/test_memory_consumer_registry.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/include/leveldb/cache.h"

namespace leveldb_chrome {

namespace {

// Every entry is charged 1 unit, so the capacity is a number of entries.
// leveldb's LRU cache splits its capacity evenly between 16 shards, so a
// multiple of 16 makes the expected charges exact.
constexpr size_t kDefaultCapacity = 1600;

// Inserting this many distinct keys is enough to fill every shard of a cache
// with a capacity of at most `kDefaultCapacity`.
constexpr int kNumEntriesToFill = 20 * kDefaultCapacity;

void NoOpDeleter(const leveldb::Slice& key, void* value) {}

// Inserts `kNumEntriesToFill` new entries in `cache`, starting at `first_key`.
// Returns the key following the last inserted entry.
int FillCache(leveldb::Cache* cache, int first_key) {
  const int end_key = first_key + kNumEntriesToFill;
  for (int key = first_key; key < end_key; ++key) {
    const std::string key_string = base::NumberToString(key);
    cache->Release(cache->Insert(key_string, /*value=*/nullptr, /*charge=*/1,
                                 &NoOpDeleter));
  }
  return end_key;
}

class SharedBlockCachesTest : public testing::Test {
 protected:
  explicit SharedBlockCachesTest(bool stateful_memory_pressure) {
    feature_list_.InitWithFeatureState(base::kStatefulMemoryPressure,
                                       stateful_memory_pressure);
  }

  // Creates the shared block caches and waits until they are registered with
  // the memory coordinator.
  void CreateSharedBlockCaches(bool use_separate_web_cache = true) {
    shared_block_caches_.emplace(
        kDefaultCapacity, use_separate_web_cache,
        base::SequencedTaskRunner::GetCurrentDefault());
    ASSERT_TRUE(base::test::RunUntil([&]() { return registry_.size() == 1u; }));
  }

  leveldb::Cache* browser_block_cache() {
    return shared_block_caches_->browser_block_cache();
  }

  leveldb::Cache* web_block_cache() {
    return shared_block_caches_->web_block_cache();
  }

  // Updates the memory limit and waits until it was handled. Notifications are
  // handled in order, so this also guarantees that all the previous
  // notifications were handled.
  void UpdateMemoryLimit(base::MemoryLimit memory_limit) {
    registry_.NotifyUpdateMemoryLimit(memory_limit);
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return shared_block_caches_->memory_limit() == memory_limit;
    }));
  }

  // Sends a release memory notification. It is handled asynchronously, so
  // callers must wait for its effect.
  void ReleaseMemory() { registry_.NotifyReleaseMemory(); }

  // Waits until the total charge of `cache` is `expected_total_charge`.
  [[nodiscard]] bool WaitForTotalCharge(leveldb::Cache* cache,
                                        size_t expected_total_charge) {
    return base::test::RunUntil(
        [&]() { return cache->TotalCharge() == expected_total_charge; });
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  base::TestMemoryConsumerRegistry registry_;
  std::optional<SharedBlockCaches> shared_block_caches_;
};

class SharedBlockCachesStatefulTest : public SharedBlockCachesTest {
 protected:
  SharedBlockCachesStatefulTest()
      : SharedBlockCachesTest(/*stateful_memory_pressure=*/true) {}
};

class SharedBlockCachesStatelessTest : public SharedBlockCachesTest {
 protected:
  SharedBlockCachesStatelessTest()
      : SharedBlockCachesTest(/*stateful_memory_pressure=*/false) {}
};

}  // namespace

// Lowering the memory limit does not evict entries until memory is released.
TEST_F(SharedBlockCachesStatefulTest, ReleaseShrinksCapacity) {
  ASSERT_NO_FATAL_FAILURE(CreateSharedBlockCaches());
  FillCache(browser_block_cache(), /*first_key=*/0);
  ASSERT_EQ(browser_block_cache()->TotalCharge(), kDefaultCapacity);

  ASSERT_NO_FATAL_FAILURE(
      UpdateMemoryLimit(base::MemoryLimit::FromPercent(50)));
  EXPECT_EQ(browser_block_cache()->TotalCharge(), kDefaultCapacity);

  ReleaseMemory();
  EXPECT_TRUE(WaitForTotalCharge(browser_block_cache(), kDefaultCapacity / 2));
}

// Lowering the memory limit prevents the cache from growing past the target
// capacity, even before memory is released.
TEST_F(SharedBlockCachesStatefulTest, LoweringLimitStopsGrowthAtTarget) {
  ASSERT_NO_FATAL_FAILURE(CreateSharedBlockCaches());

  ASSERT_NO_FATAL_FAILURE(
      UpdateMemoryLimit(base::MemoryLimit::FromPercent(50)));
  FillCache(browser_block_cache(), /*first_key=*/0);
  EXPECT_EQ(browser_block_cache()->TotalCharge(), kDefaultCapacity / 2);
}

// Lowering the memory limit below the current usage prevents the cache from
// growing past its current usage, even before memory is released.
TEST_F(SharedBlockCachesStatefulTest, LoweringLimitStopsGrowthAtUsage) {
  ASSERT_NO_FATAL_FAILURE(CreateSharedBlockCaches());
  // A quarter of the capacity, well below the capacity of every shard.
  constexpr size_t kUsage = kDefaultCapacity / 4;
  for (size_t key = 0; key < kUsage; ++key) {
    browser_block_cache()->Release(browser_block_cache()->Insert(
        base::NumberToString(key), /*value=*/nullptr, /*charge=*/1,
        &NoOpDeleter));
  }
  ASSERT_EQ(browser_block_cache()->TotalCharge(), kUsage);

  // The target capacity is below the current usage.
  ASSERT_NO_FATAL_FAILURE(
      UpdateMemoryLimit(base::MemoryLimit::FromPercent(10)));

  // The capacity is frozen at the usage. Some entries may have been evicted
  // from the shards that are fuller than average, but filling the cache brings
  // the usage back to exactly `kUsage`.
  FillCache(browser_block_cache(), /*first_key=*/static_cast<int>(kUsage));
  EXPECT_EQ(browser_block_cache()->TotalCharge(), kUsage);
}

// The reduced capacity persists after memory is released.
TEST_F(SharedBlockCachesStatefulTest, ReducedCapacityPersists) {
  ASSERT_NO_FATAL_FAILURE(CreateSharedBlockCaches());
  int next_key = FillCache(browser_block_cache(), /*first_key=*/0);

  ASSERT_NO_FATAL_FAILURE(
      UpdateMemoryLimit(base::MemoryLimit::FromPercent(50)));
  ReleaseMemory();
  ASSERT_TRUE(WaitForTotalCharge(browser_block_cache(), kDefaultCapacity / 2));

  FillCache(browser_block_cache(), next_key);
  EXPECT_EQ(browser_block_cache()->TotalCharge(), kDefaultCapacity / 2);
}

// Raising the memory limit restores the capacity without a release.
TEST_F(SharedBlockCachesStatefulTest, RaisingLimitRestoresCapacity) {
  ASSERT_NO_FATAL_FAILURE(CreateSharedBlockCaches());
  int next_key = FillCache(browser_block_cache(), /*first_key=*/0);

  ASSERT_NO_FATAL_FAILURE(
      UpdateMemoryLimit(base::MemoryLimit::FromPercent(50)));
  ReleaseMemory();
  ASSERT_TRUE(WaitForTotalCharge(browser_block_cache(), kDefaultCapacity / 2));

  ASSERT_NO_FATAL_FAILURE(UpdateMemoryLimit(base::MemoryLimit::Default()));
  FillCache(browser_block_cache(), next_key);
  EXPECT_EQ(browser_block_cache()->TotalCharge(), kDefaultCapacity);
}

// A critical memory limit empties the cache.
TEST_F(SharedBlockCachesStatefulTest, CriticalLimitEmptiesCache) {
  ASSERT_NO_FATAL_FAILURE(CreateSharedBlockCaches());
  int next_key = FillCache(browser_block_cache(), /*first_key=*/0);

  ASSERT_NO_FATAL_FAILURE(
      UpdateMemoryLimit(base::MemoryLimit::CriticalPressureThreshold()));
  ReleaseMemory();
  ASSERT_TRUE(WaitForTotalCharge(browser_block_cache(), 0u));

  FillCache(browser_block_cache(), next_key);
  EXPECT_EQ(browser_block_cache()->TotalCharge(), 0u);
}

// Both the browser and the web block caches are resized.
TEST_F(SharedBlockCachesStatefulTest, SeparateWebCache) {
  ASSERT_NO_FATAL_FAILURE(
      CreateSharedBlockCaches(/*use_separate_web_cache=*/true));
  ASSERT_NE(web_block_cache(), browser_block_cache());
  FillCache(browser_block_cache(), /*first_key=*/0);
  FillCache(web_block_cache(), /*first_key=*/0);

  ASSERT_NO_FATAL_FAILURE(
      UpdateMemoryLimit(base::MemoryLimit::FromPercent(50)));
  ReleaseMemory();
  EXPECT_TRUE(WaitForTotalCharge(browser_block_cache(), kDefaultCapacity / 2));
  EXPECT_TRUE(WaitForTotalCharge(web_block_cache(), kDefaultCapacity / 2));
}

// Web APIs share the browser block cache when there is no separate web cache.
TEST_F(SharedBlockCachesStatefulTest, SharedWebCache) {
  ASSERT_NO_FATAL_FAILURE(
      CreateSharedBlockCaches(/*use_separate_web_cache=*/false));
  ASSERT_EQ(web_block_cache(), browser_block_cache());
  FillCache(web_block_cache(), /*first_key=*/0);

  ASSERT_NO_FATAL_FAILURE(
      UpdateMemoryLimit(base::MemoryLimit::FromPercent(50)));
  ReleaseMemory();
  EXPECT_TRUE(WaitForTotalCharge(web_block_cache(), kDefaultCapacity / 2));
}

// Without stateful memory pressure, the caches are pruned on moderate pressure
// and their capacity is never changed.
TEST_F(SharedBlockCachesStatelessTest, PrunesOnModeratePressure) {
  ASSERT_NO_FATAL_FAILURE(CreateSharedBlockCaches());
  int next_key = FillCache(browser_block_cache(), /*first_key=*/0);
  ASSERT_EQ(browser_block_cache()->TotalCharge(), kDefaultCapacity);

  // Above the moderate pressure threshold, releasing memory does nothing.
  ASSERT_NO_FATAL_FAILURE(
      UpdateMemoryLimit(base::MemoryLimit::FromPercent(75)));
  ReleaseMemory();

  // Updating the memory limit does nothing either. Waiting for this update also
  // guarantees that the previous release was handled.
  ASSERT_NO_FATAL_FAILURE(
      UpdateMemoryLimit(base::MemoryLimit::ModeratePressureThreshold()));
  EXPECT_EQ(browser_block_cache()->TotalCharge(), kDefaultCapacity);

  ReleaseMemory();
  ASSERT_TRUE(WaitForTotalCharge(browser_block_cache(), 0u));

  // The capacity is unchanged.
  FillCache(browser_block_cache(), next_key);
  EXPECT_EQ(browser_block_cache()->TotalCharge(), kDefaultCapacity);
}

}  // namespace leveldb_chrome
