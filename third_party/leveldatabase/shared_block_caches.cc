// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/leveldatabase/shared_block_caches.h"

#include <algorithm>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory_coordinator/memory_coordinator_features.h"
#include "base/memory_coordinator/memory_limit.h"
#include "base/memory_coordinator/traits.h"
#include "base/task/sequenced_task_runner.h"

namespace leveldb_chrome {

namespace {

constexpr base::MemoryConsumerTraits kSharedBlockCachesTraits(
    // At most two block caches of a few MBs each.
    base::MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
    // Eviction walks the LRU list, reading the blocks being freed.
    base::MemoryConsumerTraits::ReleaseMemoryDataAccess::kReads,
    // Evicted blocks are read again from disk when needed.
    base::MemoryConsumerTraits::InformationRetention::kLossless,
    // Registered with AsyncMemoryConsumerRegistration.
    base::MemoryConsumerTraits::ExecutionType::kAsynchronous);

}  // namespace

SharedBlockCaches::SharedBlockCaches(
    size_t default_capacity,
    bool use_separate_web_cache,
    scoped_refptr<base::SequencedTaskRunner> memory_consumer_task_runner)
    : browser_block_cache_(leveldb::NewLRUCache(default_capacity)),
      web_block_cache_(use_separate_web_cache
                           ? leveldb::NewLRUCache(default_capacity)
                           : nullptr),
      default_capacity_(default_capacity) {
  // This instance can be created on any sequence, but the memory consumer state
  // is bound to the memory consumer task runner.
  DETACH_FROM_SEQUENCE(sequence_checker_);

  if (memory_consumer_task_runner) {
    memory_consumer_task_runner->PostTask(
        FROM_HERE, base::BindOnce(&SharedBlockCaches::RegisterMemoryConsumer,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

SharedBlockCaches::~SharedBlockCaches() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SharedBlockCaches::RegisterMemoryConsumer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  registration_.emplace(
      "SharedBlockCaches", kSharedBlockCachesTraits, this,
      // Leaked when owned by the leveldb_chrome globals.
      base::AsyncMemoryConsumerRegistration::CheckUnregister::kDisabled);
}

void SharedBlockCaches::OnUpdateMemoryLimit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::FeatureList::IsEnabled(base::kStatefulMemoryPressure)) {
    return;
  }
  // Grow the caches to the target capacity, or prevent them from growing past
  // their current usage if the target is lower. Evicting entries to reach the
  // target is deferred to OnReleaseMemory().
  //
  // leveldb::Cache splits its capacity evenly between internal shards, so a
  // few entries may still be evicted from the shards that are fuller than
  // average. This is accepted, since the shards are balanced by hashing.
  const size_t target_capacity = memory_limit().Scale(default_capacity_);
  auto limit_growth = [target_capacity](leveldb::Cache* cache) {
    cache->SetCapacity(std::max(target_capacity, cache->TotalCharge()));
  };
  limit_growth(browser_block_cache_.get());
  if (web_block_cache_) {
    limit_growth(web_block_cache_.get());
  }
}

void SharedBlockCaches::OnReleaseMemory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (base::FeatureList::IsEnabled(base::kStatefulMemoryPressure)) {
    SetCapacity(memory_limit().Scale(default_capacity_));
    return;
  }
  // Legacy behavior: prune on moderate and critical memory pressure.
  if (memory_limit() <= base::MemoryLimit::ModeratePressureThreshold()) {
    Prune();
  }
}

void SharedBlockCaches::SetCapacity(size_t capacity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  browser_block_cache_->SetCapacity(capacity);
  if (web_block_cache_) {
    web_block_cache_->SetCapacity(capacity);
  }
}

void SharedBlockCaches::Prune() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  browser_block_cache_->Prune();
  if (web_block_cache_) {
    web_block_cache_->Prune();
  }
}

}  // namespace leveldb_chrome
