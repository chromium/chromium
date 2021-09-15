// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Connects to a running Chrome process, and outputs statistics about its thread
// caches.

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <cstring>
#include <ios>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "base/allocator/partition_allocator/thread_cache.h"
#include "base/check_op.h"
#include "base/debug/proc_maps_linux.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if defined(HAS_LOCAL_ELFUTILS)
#include "tools/memory/partition_allocator/lookup_symbol.h"
#endif

namespace {

// SIGSTOPs a process.
class ScopedSigStopper {
 public:
  explicit ScopedSigStopper(pid_t pid) : pid_(pid) { kill(pid_, SIGSTOP); }
  ~ScopedSigStopper() { kill(pid_, SIGCONT); }

 private:
  const pid_t pid_;
};

base::ScopedFD OpenProcMem(pid_t pid) {
  std::string path = base::StringPrintf("/proc/%d/mem", pid);
  int fd = open(path.c_str(), O_RDONLY);
  CHECK_NE(fd, -1)
      << "Do you have 0 set in /proc/sys/kernel/yama/ptrace_scope?";

  return base::ScopedFD(fd);
}

// Reads a remote process memory.
bool ReadMemory(int fd, unsigned long address, size_t size, char* buffer) {
  if (HANDLE_EINTR(pread(fd, buffer, size, address)) ==
      static_cast<ssize_t>(size)) {
    return true;
  }

  return false;
}

// Scans the process memory to look for the thread cache registry address. This
// does not need symbols.
uintptr_t FindThreadCacheRegistry(pid_t pid, int mem_fd) {
  std::vector<base::debug::MappedMemoryRegion> regions;

  {
    // Ensures that the mappings are not going to change.
    ScopedSigStopper stop{pid};

    // There are subtleties when trying to read this file, which we blissfully
    // ignore here. See //base/debug/proc_maps_linux.h for details. We don't use
    // it, since we don't read the maps for ourselves, and everything is already
    // extremely racy. At worst we have to retry.
    LOG(INFO) << "Opening /proc/PID/maps";
    std::string path = base::StringPrintf("/proc/%d/maps", pid);
    auto file = base::File(base::FilePath(path),
                           base::File::FLAG_OPEN | base::File::FLAG_READ);
    CHECK(file.IsValid());
    std::vector<char> data(1e7);
    int bytes_read =
        file.ReadAtCurrentPos(&data[0], static_cast<int>(data.size()) - 1);
    CHECK_GT(bytes_read, 0) << "Cannot read " << path;
    data[bytes_read] = '\0';
    std::string proc_maps(&data[0]);

    LOG(INFO) << "Parsing the maps";
    CHECK(base::debug::ParseProcMaps(proc_maps, &regions));
    LOG(INFO) << "Found " << regions.size() << " regions";
  }

  for (auto& region : regions) {
    using base::debug::MappedMemoryRegion;

    // The array is in .data, meaning that it's mapped from the executable, and
    // has rw-p permissions. For Chrome, .data is quite small, hence the size
    // limit.
    uint8_t expected_permissions = MappedMemoryRegion::Permission::READ |
                                   MappedMemoryRegion::Permission::WRITE |
                                   MappedMemoryRegion::Permission::PRIVATE;
    size_t region_size = region.end - region.start;
    if (region.permissions != expected_permissions || region_size > 1e7 ||
        region.path.empty()) {
      continue;
    }

    LOG(INFO) << "Found a candidate region between " << std::hex << region.start
              << " and " << region.end << std::dec
              << " (size = " << region.end - region.start
              << ") path = " << region.path;
    // Scan the region, looking for the needles.
    uintptr_t needle_array_candidate[3];
    for (uintptr_t address = region.start;
         address < region.end - sizeof(needle_array_candidate);
         address += sizeof(uintptr_t)) {
      bool ok = ReadMemory(mem_fd, reinterpret_cast<unsigned long>(address),
                           sizeof(needle_array_candidate),
                           reinterpret_cast<char*>(needle_array_candidate));
      if (!ok) {
        LOG(WARNING) << "Failed to read";
        continue;
      }

      if (needle_array_candidate[0] == base::internal::tools::kNeedle1 &&
          needle_array_candidate[2] == base::internal::tools::kNeedle2) {
        LOG(INFO) << "Got it! Address = 0x" << std::hex
                  << needle_array_candidate[1];
        return needle_array_candidate[1];
      }
    }
  }

  LOG(ERROR) << "Failed to find the address";
  return 0;
}

// Allows to access an object copied from remote memory "as if" it were
// local. Of course, dereferencing any pointer from within it will at best
// fault.
template <typename T>
class RawBuffer {
 public:
  RawBuffer() = default;
  const T* get() const { return reinterpret_cast<const T*>(buffer_); }
  char* get_buffer() { return buffer_; }

  static absl::optional<RawBuffer<T>> ReadFromMemFd(int mem_fd,
                                                    uintptr_t address) {
    RawBuffer<T> buf;
    bool ok = ReadMemory(mem_fd, reinterpret_cast<unsigned long>(address),
                         sizeof(T), buf.get_buffer());
    if (!ok)
      return absl::nullopt;

    return {buf};
  }

 private:
  alignas(T) char buffer_[sizeof(T)];
};

// List all thread names for a given PID.
std::map<base::PlatformThreadId, std::string> ThreadNames(pid_t pid) {
  std::map<base::PlatformThreadId, std::string> result;

  base::FilePath root_path =
      base::FilePath(base::StringPrintf("/proc/%d/task", pid));
  base::FileEnumerator enumerator{root_path, false,
                                  base::FileEnumerator::DIRECTORIES};

  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    auto stat_path = path.Append("stat");
    base::File stat_file{stat_path,
                         base::File::FLAG_OPEN | base::File::FLAG_READ};
    if (!stat_file.IsValid()) {
      LOG(WARNING) << "Invalid file: " << stat_path.value();
      continue;
    }

    char buffer[4096 + 1];
    int bytes_read = stat_file.ReadAtCurrentPos(buffer, 4096);
    if (bytes_read <= 0)
      continue;
    buffer[bytes_read] = '\0';

    int pid, ppid, pgrp;
    char name[256];
    char state;
    sscanf(buffer, "%d %s %c %d %d", &pid, name, &state, &ppid, &pgrp);
    result[base::PlatformThreadId(pid)] = std::string(name);
  }

  return result;
}

#if defined(HAS_LOCAL_ELFUTILS)
void* GetThreadRegistryAddress(pid_t pid) {
  LOG(INFO) << "Looking for the thread cache registry";
  Dwfl* dwfl = AddressLookupInit(pid);
  unsigned long thread_cache_bias;
  Dwarf_Die* thread_cache_cu = LookupCompilationUnit(
      dwfl, nullptr, "../../base/allocator/partition_allocator/thread_cache.cc",
      &thread_cache_bias);

  const char* namespace_path[] = {"base", "internal", nullptr};
  void* thread_cache_registry_addr = LookupVariable(
      thread_cache_cu, thread_cache_bias, namespace_path, 3, "g_instance");
  LOG(INFO) << "Address = " << thread_cache_registry_addr;
  AddressLookupFinish(dwfl);

  return thread_cache_registry_addr;
}
#endif  // defined(HAS_LOCAL_ELFUTILS)

}  // namespace

namespace base {
namespace internal {
namespace tools {

class ThreadCacheInspector {
 public:
  // Distinct from ThreadCache::Bucket because |count| is uint8_t.
  struct BucketStats {
    int count = 0;
    int per_thread_limit = 0;
    size_t size = 0;
  };

  ThreadCacheInspector(uintptr_t registry_addr, base::ScopedFD mem_fd);
  bool GetAllThreadCaches();
  size_t CachedMemory() const;

  const std::vector<RawBuffer<ThreadCache>>& thread_caches() const {
    return thread_caches_;
  }

  static bool should_purge(const RawBuffer<ThreadCache>& tcache) {
    return tcache.get()->should_purge_;
  }

  std::vector<BucketStats> AccumulateThreadCacheBuckets();
  std::uint8_t largest_active_bucket_index() {
    return registry_.get()->largest_active_bucket_index_;
  }

 private:
  uintptr_t registry_addr_;
  base::ScopedFD mem_fd_;
  RawBuffer<ThreadCacheRegistry> registry_;
  std::vector<RawBuffer<ThreadCache>> thread_caches_;
};

ThreadCacheInspector::ThreadCacheInspector(uintptr_t registry_addr,
                                           base::ScopedFD mem_fd)
    : registry_addr_(registry_addr), mem_fd_(std::move(mem_fd)) {}

// NO_THREAD_SAFETY_ANALYSIS: Well, reading a running process' memory is not
// really thread-safe.
bool ThreadCacheInspector::GetAllThreadCaches() NO_THREAD_SAFETY_ANALYSIS {
  thread_caches_.clear();

  auto registry = RawBuffer<ThreadCacheRegistry>::ReadFromMemFd(mem_fd_.get(),
                                                                registry_addr_);
  if (!registry.has_value())
    return false;

  registry_ = *registry;
  ThreadCache* head = registry_.get()->list_head_;
  while (head) {
    auto tcache = RawBuffer<ThreadCache>::ReadFromMemFd(
        mem_fd_.get(), reinterpret_cast<uintptr_t>(head));
    if (!tcache.has_value()) {
      LOG(WARNING) << "Failed to read a ThreadCache";
      return false;
    }
    thread_caches_.push_back(tcache.value());
    head = tcache->get()->next_;
  }
  return true;
}

size_t ThreadCacheInspector::CachedMemory() const {
  size_t total_memory = 0;

  for (auto& tcache : thread_caches_) {
    size_t cached_memory = tcache.get()->CachedMemory();
    total_memory += cached_memory;
  }

  return total_memory;
}

std::vector<ThreadCacheInspector::BucketStats>
ThreadCacheInspector::AccumulateThreadCacheBuckets() {
  std::vector<BucketStats> result(ThreadCache::kBucketCount);
  for (auto& tcache : thread_caches_) {
    for (int i = 0; i < ThreadCache::kBucketCount; i++) {
      result[i].count += tcache.get()->buckets_[i].count;
      result[i].per_thread_limit = tcache.get()->buckets_[i].limit;
    }
  }

  BucketIndexLookup lookup{};
  for (int i = 0; i < ThreadCache::kBucketCount; i++) {
    result[i].size = lookup.bucket_sizes()[i];
  }
  return result;
}

}  // namespace tools
}  // namespace internal
}  // namespace base

int main(int argc, char** argv) {
  if (argc < 2) {
    LOG(ERROR) << "Usage:" << argv[0] << " <PID> "
               << "[address. 0 to scan the process memory]";
    return 1;
  }

  int pid = atoi(argv[1]);
  uintptr_t registry_address = 0;

  auto mem_fd = OpenProcMem(pid);

  if (argc == 3) {
    uint64_t address;
    CHECK(base::StringToUint64(argv[2], &address));
    registry_address = static_cast<uintptr_t>(address);

    // Scan the memory.
    if (!registry_address) {
      registry_address = FindThreadCacheRegistry(pid, mem_fd.get());
    }
  } else {
#if defined(HAS_LOCAL_ELFUTILS)
    registry_address =
        reinterpret_cast<uintptr_t>(GetThreadRegistryAddress(pid));
#endif
  }

  CHECK(registry_address);

  LOG(INFO) << "Getting the thread cache registry";
  base::internal::tools::ThreadCacheInspector inspector{registry_address,
                                                        std::move(mem_fd)};

  std::map<base::PlatformThreadId, std::string> tid_to_name;

  while (true) {
    bool ok = inspector.GetAllThreadCaches();
    if (!ok)
      continue;

    for (const auto& tcache : inspector.thread_caches()) {
      // Note: this is not robust when TIDs are reused, but here this is fine,
      // as at worst we would display wrong data, and TID reuse is very unlikely
      // in normal scenarios.
      if (tid_to_name.find(tcache.get()->thread_id()) == tid_to_name.end()) {
        tid_to_name = ThreadNames(pid);
        break;
      }
    }

    constexpr const char* kClearScreen = "\033[2J\033[1;1H";
    std::cout << kClearScreen << "Found " << inspector.thread_caches().size()
              << " caches, total cached memory = "
              << inspector.CachedMemory() / 1024 << "kiB"
              << "\n";

    std::cout << "Per thread:\n"
              << "Thread Name         Size\tPurge\n"
              << std::string(80, '-') << "\n";
    base::ThreadCacheStats all_threads_stats = {0};
    for (const auto& tcache : inspector.thread_caches()) {
      base::ThreadCacheStats stats = {0};
      // No alloc stats, they reach into tcache->root_, which is not valid.
      tcache.get()->AccumulateStats(&stats);
      tcache.get()->AccumulateStats(&all_threads_stats);
      uint64_t count = stats.alloc_count;
      uint64_t hit_rate = (100 * stats.alloc_hits) / count;
      uint64_t too_large = (100 * stats.alloc_miss_too_large) / count;
      uint64_t empty = (100 * stats.alloc_miss_empty) / count;

      std::string thread_name = tid_to_name[tcache.get()->thread_id()];
      std::string padding(20 - thread_name.size(), ' ');
      std::cout << thread_name << padding << tcache.get()->CachedMemory() / 1024
                << "kiB\t" << (inspector.should_purge(tcache) ? 'X' : ' ')
                << "\tHit Rate = " << hit_rate << "%"
                << "\tToo Large = " << too_large << "%"
                << "\tEmpty = " << empty << "%"
                << "\t Count = " << count / 1000 << "k"
                << "\n";
    }

    uint64_t count = all_threads_stats.alloc_count;
    uint64_t hit_rate = (100 * all_threads_stats.alloc_hits) / count;
    uint64_t too_large = (100 * all_threads_stats.alloc_miss_too_large) / count;
    uint64_t empty = (100 * all_threads_stats.alloc_miss_empty) / count;
    std::cout << "\nALL THREADS:        "
              << all_threads_stats.bucket_total_memory / 1024 << "kiB"
              << "\t\tHit Rate = " << hit_rate << "%"
              << "\tToo Large = " << too_large << "%"
              << "\tEmpty = " << empty << "%"
              << "\t Count = " << count / 1000 << "k"
              << "\n";

    // Per-bucket stats.
    std::cout << "\n\n\nPer-bucket stats (All Threads):"
              << "\nBucket Size\tPer-thread Limit\tCount\tTotal Memory\n"
              << std::string(80, '-') << "\n";

    size_t total_memory = 0;
    auto bucket_stats = inspector.AccumulateThreadCacheBuckets();
    for (size_t index = 0; index < bucket_stats.size(); index++) {
      const auto& bucket = bucket_stats[index];
      size_t bucket_memory = bucket.size * bucket.count;

      std::cout << bucket.size << "\t\t" << bucket.per_thread_limit << "\t\t\t"
                << bucket.count << "\t" << bucket_memory / 1024 << "kiB";
      if (inspector.largest_active_bucket_index() == index)
        std::cout << "  <---- Limit";
      std::cout << "\n";

      total_memory += bucket_memory;
    }
    std::cout << "\nALL THREADS TOTAL: " << total_memory / 1024 << "kiB";

    std::cout << std::endl;
    usleep(100000);
  }
}
