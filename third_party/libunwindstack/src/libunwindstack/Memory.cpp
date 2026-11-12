/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <algorithm>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include <android-base/unique_fd.h>

#include <unwindstack/Log.h>
#include <unwindstack/Memory.h>

#include "MemoryBuffer.h"
#include "MemoryCache.h"
#include "MemoryFileAtOffset.h"
#include "MemoryLocal.h"
#include "MemoryLocalUnsafe.h"
#include "MemoryOffline.h"
#include "MemoryOfflineBuffer.h"
#include "MemoryRange.h"
#include "MemoryRemote.h"

namespace unwindstack {

static size_t ProcessVmRead(pid_t pid, uint64_t remote_src, void* dst, size_t len) {

  // Split up the remote read across page boundaries.
  // From the manpage:
  //   A partial read/write may result if one of the remote_iov elements points to an invalid
  //   memory region in the remote process.
  //
  //   Partial transfers apply at the granularity of iovec elements.  These system calls won't
  //   perform a partial transfer that splits a single iovec element.
  constexpr size_t kMaxIovecs = 64;
  struct iovec src_iovs[kMaxIovecs];

  uint64_t cur = remote_src;
  size_t total_read = 0;
  while (len > 0) {
    struct iovec dst_iov = {
        .iov_base = &reinterpret_cast<uint8_t*>(dst)[total_read], .iov_len = len,
    };

    size_t iovecs_used = 0;
    while (len > 0) {
      if (iovecs_used == kMaxIovecs) {
        break;
      }

      // struct iovec uses void* for iov_base.
      if (cur >= UINTPTR_MAX) {
        errno = EFAULT;
        return total_read;
      }

      src_iovs[iovecs_used].iov_base = reinterpret_cast<void*>(cur);

      uintptr_t misalignment = cur & (getpagesize() - 1);
      size_t iov_len = getpagesize() - misalignment;
      iov_len = std::min(iov_len, len);

      len -= iov_len;
      if (__builtin_add_overflow(cur, iov_len, &cur)) {
        errno = EFAULT;
        return total_read;
      }

      src_iovs[iovecs_used].iov_len = iov_len;
      ++iovecs_used;
    }

// process_vm_readv availability is gated on __ANDROID_API__ >= 23 in
// third_party/android_toolchain/ndk/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/sys/uio.h
// When not available set rc to 0
#if __ANDROID_API__ >= 23
    ssize_t rc = process_vm_readv(pid, &dst_iov, 1, src_iovs, iovecs_used, 0);
#else
    static_cast<void>(&dst_iov);
    ssize_t rc = 0;
#endif
    if (rc == -1) {
      return total_read;
    }
    total_read += rc;
  }
  return total_read;
}

static bool PtraceReadLong(pid_t pid, uint64_t addr, long* value) {
  // ptrace() returns -1 and sets errno when the operation fails.
  // To disambiguate -1 from a valid result, we clear errno beforehand.
  errno = 0;
  *value = ptrace(PTRACE_PEEKTEXT, pid, reinterpret_cast<void*>(addr), nullptr);
  if (*value == -1 && errno) {
    return false;
  }
  return true;
}

static size_t PtraceRead(pid_t pid, uint64_t addr, void* dst, size_t bytes) {
  // Make sure that there is no overflow.
  uint64_t max_size;
  if (__builtin_add_overflow(addr, bytes, &max_size)) {
    return 0;
  }

  size_t bytes_read = 0;
  long data;
  size_t align_bytes = addr & (sizeof(long) - 1);
  if (align_bytes != 0) {
    if (!PtraceReadLong(pid, addr & ~(sizeof(long) - 1), &data)) {
      return 0;
    }
    size_t copy_bytes = std::min(sizeof(long) - align_bytes, bytes);
    memcpy(dst, reinterpret_cast<uint8_t*>(&data) + align_bytes, copy_bytes);
    addr += copy_bytes;
    dst = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(dst) + copy_bytes);
    bytes -= copy_bytes;
    bytes_read += copy_bytes;
  }

  for (size_t i = 0; i < bytes / sizeof(long); i++) {
    if (!PtraceReadLong(pid, addr, &data)) {
      return bytes_read;
    }
    memcpy(dst, &data, sizeof(long));
    dst = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(dst) + sizeof(long));
    addr += sizeof(long);
    bytes_read += sizeof(long);
  }

  size_t left_over = bytes & (sizeof(long) - 1);
  if (left_over) {
    if (!PtraceReadLong(pid, addr, &data)) {
      return bytes_read;
    }
    memcpy(dst, &data, left_over);
    bytes_read += left_over;
  }
  return bytes_read;
}

bool Memory::ReadFully(uint64_t addr, void* dst, size_t size) {
  size_t rc = Read(addr, dst, size);
  return rc == size;
}

bool Memory::ReadString(uint64_t addr, std::string* dst, size_t max_read) {
  char buffer[256];  // Large enough for 99% of symbol names.
  size_t size = 0;   // Number of bytes which were read into the buffer.
  for (size_t offset = 0; offset < max_read; offset += size) {
    // Look for null-terminator first, so we can allocate string of exact size.
    // If we know the end of valid memory range, do the reads in larger blocks.
    size_t read = std::min(sizeof(buffer), max_read - offset);
    size = Read(addr + offset, buffer, read);
    if (size == 0) {
      return false;  // We have not found end of string yet and we can not read more data.
    }
    size_t length = strnlen(buffer, size);  // Index of the null-terminator.
    if (length < size) {
      // We found the null-terminator. Allocate the string and set its content.
      if (offset == 0) {
        // We did just single read, so the buffer already contains the whole string.
        dst->assign(buffer, length);
        return true;
      } else {
        // The buffer contains only the last block. Read the whole string again.
        dst->assign(offset + length, '\0');
        return ReadFully(addr, dst->data(), dst->size());
      }
    }
  }
  return false;
}

std::shared_ptr<Memory> Memory::CreateFileMemory(const std::string& path, uint64_t offset,
                                                 uint64_t size) {
  auto memory = std::make_shared<MemoryFileAtOffset>();

  if (memory->Init(path, offset, size)) {
    return memory;
  }

  return nullptr;
}

std::shared_ptr<Memory> Memory::CreateProcessMemoryLocalUnsafe() {
  return std::shared_ptr<Memory>(new MemoryLocalUnsafe());
}

std::unique_ptr<Memory> Memory::CreateLocalProcessMemory() {
  return std::unique_ptr<Memory>(new MemoryLocal());
}

std::shared_ptr<Memory> Memory::CreateProcessMemory(pid_t pid) {
  if (pid == getpid()) {
    return std::shared_ptr<Memory>(new MemoryLocal());
  }
  return std::shared_ptr<Memory>(new MemoryRemote(pid));
}

std::shared_ptr<Memory> Memory::CreateProcessMemoryCached(pid_t pid) {
  if (pid == getpid()) {
    return std::shared_ptr<Memory>(new MemoryCache(new MemoryLocal()));
  }
  return std::shared_ptr<Memory>(new MemoryCache(new MemoryRemote(pid)));
}

std::shared_ptr<Memory> Memory::CreateProcessMemoryThreadCached(pid_t pid) {
  if (pid == getpid()) {
    return std::shared_ptr<Memory>(new MemoryThreadCache(new MemoryLocal()));
  }
  return std::shared_ptr<Memory>(new MemoryThreadCache(new MemoryRemote(pid)));
}

std::shared_ptr<Memory> Memory::CreateOfflineMemory(const uint8_t* data, uint64_t start,
                                                    uint64_t end) {
  return std::shared_ptr<Memory>(new MemoryOfflineBuffer(data, start, end));
}

size_t MemoryBuffer::Read(uint64_t addr, void* dst, size_t size) {
  if (addr < offset_) {
    return 0;
  }
  addr -= offset_;
  size_t raw_size = raw_.size();
  if (addr >= raw_size) {
    return 0;
  }

  size_t bytes_left = raw_size - static_cast<size_t>(addr);
  size_t actual_len = std::min(bytes_left, size);
  memcpy(dst, &raw_[addr], actual_len);
  return actual_len;
}

uint8_t* MemoryBuffer::GetPtr(size_t addr) {
  if (addr < offset_) {
    return nullptr;
  }
  addr -= offset_;
  if (addr < raw_.size()) {
    return &raw_[addr];
  }
  return nullptr;
}

MemoryFileAtOffset::~MemoryFileAtOffset() {
  Clear();
}

void MemoryFileAtOffset::Clear() {
  if (data_) {
    munmap(&data_[-offset_], size_ + offset_);
    data_ = nullptr;
  }
}

bool MemoryFileAtOffset::Init(const std::string& file, uint64_t offset, uint64_t size) {
  // Clear out any previous data if it exists.
  Clear();

  android::base::unique_fd fd(TEMP_FAILURE_RETRY(open(file.c_str(), O_RDONLY | O_CLOEXEC)));
  if (fd == -1) {
    return false;
  }
  struct stat buf;
  if (fstat(fd, &buf) == -1) {
    return false;
  }
  if (offset >= static_cast<uint64_t>(buf.st_size)) {
    return false;
  }

  offset_ = offset & (getpagesize() - 1);
  uint64_t aligned_offset = offset & ~(getpagesize() - 1);
  if (aligned_offset > static_cast<uint64_t>(buf.st_size) ||
      offset > static_cast<uint64_t>(buf.st_size)) {
    return false;
  }

  size_ = buf.st_size - aligned_offset;
  uint64_t max_size;
  if (!__builtin_add_overflow(size, offset_, &max_size) && max_size < size_) {
    // Truncate the mapped size.
    size_ = max_size;
  }
  void* map = mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd, aligned_offset);
  if (map == MAP_FAILED) {
    return false;
  }

  data_ = &reinterpret_cast<uint8_t*>(map)[offset_];
  size_ -= offset_;

  return true;
}

size_t MemoryFileAtOffset::Read(uint64_t addr, void* dst, size_t size) {
  if (addr >= size_) {
    return 0;
  }

  size_t bytes_left = size_ - static_cast<size_t>(addr);
  const unsigned char* actual_base = static_cast<const unsigned char*>(data_) + addr;
  size_t actual_len = std::min(bytes_left, size);

  memcpy(dst, actual_base, actual_len);
  return actual_len;
}

size_t MemoryRemote::Read(uint64_t addr, void* dst, size_t size) {
#if !defined(__LP64__)
  // Cannot read an address greater than 32 bits in a 32 bit context.
  if (addr > UINT32_MAX) {
    return 0;
  }
#endif

  size_t (*read_func)(pid_t, uint64_t, void*, size_t) =
      reinterpret_cast<size_t (*)(pid_t, uint64_t, void*, size_t)>(read_redirect_func_.load());
  if (read_func != nullptr) {
    return read_func(pid_, addr, dst, size);
  } else {
    // Prefer process_vm_read, try it first. If it doesn't work, use the
    // ptrace function. If at least one of them returns at least some data,
    // set that as the permanent function to use.
    // This assumes that if process_vm_read works once, it will continue
    // to work.
    size_t bytes = ProcessVmRead(pid_, addr, dst, size);
    if (bytes > 0) {
      read_redirect_func_ = reinterpret_cast<uintptr_t>(ProcessVmRead);
      return bytes;
    }
    bytes = PtraceRead(pid_, addr, dst, size);
    if (bytes > 0) {
      read_redirect_func_ = reinterpret_cast<uintptr_t>(PtraceRead);
    }
    return bytes;
  }
}

size_t MemoryLocal::Read(uint64_t addr, void* dst, size_t size) {
  return ProcessVmRead(getpid(), addr, dst, size);
}

MemoryRange::MemoryRange(const std::shared_ptr<Memory>& memory, uint64_t begin, uint64_t length,
                         uint64_t offset)
    : memory_(memory), begin_(begin), length_(length), offset_(offset) {}

size_t MemoryRange::Read(uint64_t addr, void* dst, size_t size) {
  if (addr < offset_) {
    return 0;
  }

  uint64_t read_offset = addr - offset_;
  if (read_offset >= length_) {
    return 0;
  }

  uint64_t read_length = std::min(static_cast<uint64_t>(size), length_ - read_offset);
  uint64_t read_addr;
  if (__builtin_add_overflow(read_offset, begin_, &read_addr)) {
    return 0;
  }

  return memory_->Read(read_addr, dst, read_length);
}

bool MemoryRanges::Insert(MemoryRange* memory) {
  uint64_t last_addr;
  if (__builtin_add_overflow(memory->offset(), memory->length(), &last_addr)) {
    // This should never happen in the real world. However, it is possible
    // that an offset in a mapped in segment could be crafted such that
    // this value overflows. In that case, clamp the value to the max uint64
    // value.
    last_addr = UINT64_MAX;
  }
  auto entry = maps_.try_emplace(last_addr, memory);
  if (entry.second) {
    return true;
  }
  delete memory;
  return false;
}

size_t MemoryRanges::Read(uint64_t addr, void* dst, size_t size) {
  auto entry = maps_.upper_bound(addr);
  if (entry != maps_.end()) {
    return entry->second->Read(addr, dst, size);
  }
  return 0;
}

bool MemoryOffline::Init(const std::string& file, uint64_t offset) {
  auto memory_file = std::make_shared<MemoryFileAtOffset>();
  if (!memory_file->Init(file, offset)) {
    return false;
  }

  // The first uint64_t value is the start of memory.
  uint64_t start;
  if (!memory_file->ReadFully(0, &start, sizeof(start))) {
    return false;
  }

  uint64_t size = memory_file->Size();
  if (__builtin_sub_overflow(size, sizeof(start), &size)) {
    return false;
  }

  memory_ = std::make_unique<MemoryRange>(memory_file, sizeof(start), size, start);
  return true;
}

bool MemoryOffline::Init(const std::string& file, uint64_t offset, uint64_t start, uint64_t size) {
  auto memory_file = std::make_shared<MemoryFileAtOffset>();
  if (!memory_file->Init(file, offset)) {
    return false;
  }

  memory_ = std::make_unique<MemoryRange>(memory_file, 0, size, start);
  return true;
}

size_t MemoryOffline::Read(uint64_t addr, void* dst, size_t size) {
  if (!memory_) {
    return 0;
  }

  return memory_->Read(addr, dst, size);
}

MemoryOfflineBuffer::MemoryOfflineBuffer(const uint8_t* data, uint64_t start, uint64_t end)
    : data_(data), start_(start), end_(end) {}

void MemoryOfflineBuffer::Reset(const uint8_t* data, uint64_t start, uint64_t end) {
  data_ = data;
  start_ = start;
  end_ = end;
}

size_t MemoryOfflineBuffer::Read(uint64_t addr, void* dst, size_t size) {
  if (addr < start_ || addr >= end_) {
    return 0;
  }

  size_t read_length = std::min(size, static_cast<size_t>(end_ - addr));
  memcpy(dst, &data_[addr - start_], read_length);
  return read_length;
}

MemoryOfflineParts::~MemoryOfflineParts() {
  for (auto memory : memories_) {
    delete memory;
  }
}

size_t MemoryOfflineParts::Read(uint64_t addr, void* dst, size_t size) {
  if (memories_.empty()) {
    return 0;
  }

  // Do a read on each memory object, no support for reading across the
  // different memory objects.
  for (MemoryOffline* memory : memories_) {
    size_t bytes = memory->Read(addr, dst, size);
    if (bytes != 0) {
      return bytes;
    }
  }
  return 0;
}

size_t MemoryCacheBase::InternalCachedRead(uint64_t addr, void* dst, size_t size,
                                           CacheDataType* cache) {
  uint64_t addr_page = addr >> kCacheBits;
  auto entry = cache->find(addr_page);
  uint8_t* cache_dst;
  if (entry != cache->end()) {
    cache_dst = entry->second;
  } else {
    cache_dst = (*cache)[addr_page];
    if (!impl_->ReadFully(addr_page << kCacheBits, cache_dst, kCacheSize)) {
      // Erase the entry.
      cache->erase(addr_page);
      return impl_->Read(addr, dst, size);
    }
  }
  size_t max_read = ((addr_page + 1) << kCacheBits) - addr;
  if (size <= max_read) {
    memcpy(dst, &cache_dst[addr & kCacheMask], size);
    return size;
  }

  // The read crossed into another cached entry, since a read can only cross
  // into one extra cached page, duplicate the code rather than looping.
  memcpy(dst, &cache_dst[addr & kCacheMask], max_read);
  dst = &reinterpret_cast<uint8_t*>(dst)[max_read];
  addr_page++;

  entry = cache->find(addr_page);
  if (entry != cache->end()) {
    cache_dst = entry->second;
  } else {
    cache_dst = (*cache)[addr_page];
    if (!impl_->ReadFully(addr_page << kCacheBits, cache_dst, kCacheSize)) {
      // Erase the entry.
      cache->erase(addr_page);
      return impl_->Read(addr_page << kCacheBits, dst, size - max_read) + max_read;
    }
  }
  memcpy(dst, cache_dst, size - max_read);
  return size;
}

void MemoryCache::Clear() {
  std::lock_guard<std::mutex> lock(cache_lock_);
  cache_.clear();
}

size_t MemoryCache::CachedRead(uint64_t addr, void* dst, size_t size) {
  // Use a single lock since this object is not designed to be performant
  // for multiple object reading from multiple threads.
  std::lock_guard<std::mutex> lock(cache_lock_);

  return InternalCachedRead(addr, dst, size, &cache_);
}

MemoryThreadCache::MemoryThreadCache(Memory* memory) : MemoryCacheBase(memory) {
  thread_cache_ = std::make_optional<pthread_t>();
  if (pthread_key_create(&*thread_cache_, [](void* memory) {
        CacheDataType* cache = reinterpret_cast<CacheDataType*>(memory);
        delete cache;
      }) != 0) {
    Log::AsyncSafe("Failed to create pthread key.");
    thread_cache_.reset();
  }
}

MemoryThreadCache::~MemoryThreadCache() {
  if (thread_cache_) {
    CacheDataType* cache = reinterpret_cast<CacheDataType*>(pthread_getspecific(*thread_cache_));
    delete cache;
    pthread_key_delete(*thread_cache_);
  }
}

size_t MemoryThreadCache::CachedRead(uint64_t addr, void* dst, size_t size) {
  if (!thread_cache_) {
    return impl_->Read(addr, dst, size);
  }

  CacheDataType* cache = reinterpret_cast<CacheDataType*>(pthread_getspecific(*thread_cache_));
  if (cache == nullptr) {
    cache = new CacheDataType;
    pthread_setspecific(*thread_cache_, cache);
  }

  return InternalCachedRead(addr, dst, size, cache);
}

void MemoryThreadCache::Clear() {
  if (!thread_cache_) {
    return;
  }

  CacheDataType* cache = reinterpret_cast<CacheDataType*>(pthread_getspecific(*thread_cache_));
  if (cache != nullptr) {
    delete cache;
    pthread_setspecific(*thread_cache_, nullptr);
  }
}

size_t MemoryLocalUnsafe::Read(uint64_t addr, void* dst, size_t size) {
  void* raw_ptr = reinterpret_cast<void*>(addr);
  memcpy(dst, raw_ptr, size);
  return size;
}

}  // namespace unwindstack
