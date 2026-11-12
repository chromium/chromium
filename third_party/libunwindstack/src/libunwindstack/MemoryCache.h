/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <pthread.h>
#include <stdint.h>

#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>

#include <unwindstack/Memory.h>

namespace unwindstack {

class MemoryCacheBase : public Memory {
 public:
  MemoryCacheBase(Memory* memory) : impl_(memory) {}
  virtual ~MemoryCacheBase() = default;

  MemoryCacheBase* AsMemoryCacheBase() override { return this; }

  const std::shared_ptr<Memory>& UnderlyingMemory() { return impl_; }

  size_t Read(uint64_t addr, void* dst, size_t size) override {
    // Only look at the cache for small reads.
    if (size > 64) {
      return impl_->Read(addr, dst, size);
    }
    return CachedRead(addr, dst, size);
  }

  long ReadTag(uint64_t addr) override { return impl_->ReadTag(addr); }

 protected:
  constexpr static size_t kCacheBits = 12;
  constexpr static size_t kCacheMask = (1 << kCacheBits) - 1;
  constexpr static size_t kCacheSize = 1 << kCacheBits;

  using CacheDataType = std::unordered_map<uint64_t, uint8_t[kCacheSize]>;

  virtual size_t CachedRead(uint64_t addr, void* dst, size_t size) = 0;

  size_t InternalCachedRead(uint64_t addr, void* dst, size_t size, CacheDataType* cache);

  std::shared_ptr<Memory> impl_;
};

class MemoryCache : public MemoryCacheBase {
 public:
  MemoryCache(Memory* memory) : MemoryCacheBase(memory) {}
  virtual ~MemoryCache() = default;

  size_t CachedRead(uint64_t addr, void* dst, size_t size) override;

  void Clear() override;

 protected:
  CacheDataType cache_;

  std::mutex cache_lock_;
};

class MemoryThreadCache : public MemoryCacheBase {
 public:
  MemoryThreadCache(Memory* memory);
  virtual ~MemoryThreadCache();

  size_t CachedRead(uint64_t addr, void* dst, size_t size) override;

  void Clear() override;

 protected:
  std::optional<pthread_key_t> thread_cache_;
};

}  // namespace unwindstack
