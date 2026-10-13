// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_LEVELDATABASE_SHARED_BLOCK_CACHES_H_
#define THIRD_PARTY_LEVELDATABASE_SHARED_BLOCK_CACHES_H_

#include <cstddef>
#include <memory>
#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/memory_coordinator/async_memory_consumer_registration.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "leveldb/cache.h"
#include "leveldb/export.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace leveldb_chrome {

// Owns the block caches shared by Chrome's leveldb databases, and adapts them
// to the memory limit assigned by the memory coordinator.
//
// When `base::kStatefulMemoryPressure` is enabled, the capacity of the caches
// follows the memory limit. Otherwise, the caches are pruned on memory
// pressure.
//
// The caches can be used from any sequence. Memory coordinator notifications
// are handled on `memory_consumer_task_runner`.
class LEVELDB_EXPORT SharedBlockCaches : public base::MemoryConsumer {
 public:
  // `default_capacity` is the capacity of each cache at the default memory
  // limit. If `use_separate_web_cache` is false, web APIs share the browser
  // block cache. If `memory_consumer_task_runner` is null, this instance does
  // not register with the memory coordinator. Otherwise, this instance must be
  // destroyed on `memory_consumer_task_runner`.
  SharedBlockCaches(
      size_t default_capacity,
      bool use_separate_web_cache,
      scoped_refptr<base::SequencedTaskRunner> memory_consumer_task_runner);

  SharedBlockCaches(const SharedBlockCaches&) = delete;
  SharedBlockCaches& operator=(const SharedBlockCaches&) = delete;

  ~SharedBlockCaches() override;

  // The block cache used by databases that are not accessible to web APIs.
  leveldb::Cache* browser_block_cache() const {
    return browser_block_cache_.get();
  }

  // The block cache used by databases accessible to web APIs. Same as the
  // browser block cache if `use_separate_web_cache` was false.
  leveldb::Cache* web_block_cache() const {
    return web_block_cache_ ? web_block_cache_.get() : browser_block_cache();
  }

 private:
  // Registers with the memory coordinator. Runs on the memory consumer task
  // runner so that notifications are delivered there.
  void RegisterMemoryConsumer();

  // base::MemoryConsumer:
  void OnUpdateMemoryLimit() override;
  void OnReleaseMemory() override;

  // Sets the capacity of the caches, evicting entries if needed.
  void SetCapacity(size_t capacity);

  // Removes all the entries of the caches that are not actively in use.
  void Prune();

  const std::unique_ptr<leveldb::Cache> browser_block_cache_;  // Never null.
  const std::unique_ptr<leveldb::Cache> web_block_cache_;      // May be null.

  const size_t default_capacity_;

  // Bound to the memory consumer task runner.
  SEQUENCE_CHECKER(sequence_checker_);

  std::optional<base::AsyncMemoryConsumerRegistration> registration_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<SharedBlockCaches> weak_ptr_factory_{this};
};

}  // namespace leveldb_chrome

#endif  // THIRD_PARTY_LEVELDATABASE_SHARED_BLOCK_CACHES_H_
