// Copyright 2021 The Chromium Authors
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
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/debug/proc_maps_linux.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/scoped_file.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/page_size.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/thread_annotations.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "partition_alloc/partition_root.h"
#include "partition_alloc/partition_stats.h"
#include "partition_alloc/thread_cache.h"
#include "tools/memory/partition_allocator/inspect_utils.h"

namespace partition_alloc::tools {

using ::base::PlatformThreadId;
using partition_alloc::internal::BucketIndexLookup;
using partition_alloc::internal::MetadataKind;
using partition_alloc::internal::PartitionBucket;
template <MetadataKind kind>
using SlotSpanMetadata = partition_alloc::internal::SlotSpanMetadata<kind>;

namespace {

// Scans the process memory to look for the thread cache registry address. This
// does not need symbols.
uintptr_t FindThreadCacheRegistry(RemoteProcessMemoryReader& reader) {
  return IndexThreadCacheNeedleArray(reader, 1);
}

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

    int process_id, ppid, pgrp;
    char name[256];
    char state;
    sscanf(buffer, "%d %s %c %d %d", &process_id, name, &state, &ppid, &pgrp);

    auto status_path = path.Append("status");
    base::File status_file{status_path,
                           base::File::FLAG_OPEN | base::File::FLAG_READ};
    if (!status_file.IsValid()) {
      LOG(WARNING) << "Invalid file: " << status_path.value();
      continue;
    }
    bytes_read = status_file.ReadAtCurrentPos(buffer, 4096);
    if (bytes_read <= 0)
      continue;
    buffer[bytes_read] = '\0';
    auto lines = SplitString(buffer, "\n", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
    for (std::string_view sp : lines) {
      if (sp.rfind("NSpid:\t", 0) == 0) {
        auto line_parts = SplitString(sp, "\t", base::TRIM_WHITESPACE,
                                      base::SPLIT_WANT_NONEMPTY);
        std::string inner_pid = line_parts.back();
        sscanf(inner_pid.c_str(), "%d", &process_id);
      }
    }

    result[base::PlatformThreadId(process_id)] = std::string(name);
  }

  return result;
}

}  // namespace

class ThreadCacheInspector {
 public:
  // Distinct from ThreadCache::Bucket because |count| is uint8_t.
  struct BucketStats {
    int count = 0;
    int per_thread_limit = 0;
    size_t size = 0;
  };

  ThreadCacheInspector(uintptr_t registry_addr, pid_t pid);
  bool GetAllThreadCaches();
  size_t CachedMemory() const;
  uintptr_t GetRootAddress();

  const std::vector<RawBuffer<ThreadCache>>& thread_caches() const {
    return thread_caches_;
  }

  static bool should_purge(const RawBuffer<ThreadCache>& tcache) {
    return tcache.get()->should_purge_;
  }

  std::vector<BucketStats> AccumulateThreadCacheBuckets();
  std::uint16_t largest_active_bucket_index() {
    return registry_.get()->largest_active_bucket_index_;
  }

 private:
  uintptr_t registry_addr_;
  pid_t pid_;
  RemoteProcessMemoryReader reader_;
  RawBuffer<ThreadCacheRegistry> registry_;
  std::vector<RawBuffer<ThreadCache>> thread_caches_;
};

class PartitionRootInspector {
 public:
  struct BucketStats {
    size_t slot_size = 0;
    size_t allocated_slots = 0;
    size_t freelist_size = 0;

    PartitionBucket bucket;
    std::vector<size_t> freelist_sizes;
    // Flattened versions of the lists.
    std::vector<SlotSpanMetadata<MetadataKind::kReadOnly>> active_slot_spans;
    std::vector<SlotSpanMetadata<MetadataKind::kReadOnly>> empty_slot_spans;
    std::vector<SlotSpanMetadata<MetadataKind::kReadOnly>>
        decommitted_slot_spans;
  };

  PartitionRootInspector(uintptr_t root_addr, pid_t pid)
      : root_addr_(root_addr), pid_(pid), reader_(pid) {}
  // Returns true for success.
  bool GatherStatistics();
  const std::vector<BucketStats>& bucket_stats() const { return bucket_stats_; }
  const PartitionRoot* root() { return root_.get(); }

 private:
  void Update();

  uintptr_t root_addr_;
  pid_t pid_;
  RemoteProcessMemoryReader reader_;
  RawBuffer<PartitionRoot> root_;
  std::vector<BucketStats> bucket_stats_;
};

ThreadCacheInspector::ThreadCacheInspector(uintptr_t registry_addr, pid_t pid)
    : registry_addr_(registry_addr), pid_(pid), reader_(pid) {}

// NO_THREAD_SAFETY_ANALYSIS: Well, reading a running process' memory is not
// really thread-safe.
bool ThreadCacheInspector::GetAllThreadCaches() NO_THREAD_SAFETY_ANALYSIS {
  thread_caches_.clear();

  // This is going to take a while, make sure that the metadata don't change.
  ScopedSigStopper stopper{pid_};

  auto registry = RawBuffer<ThreadCacheRegistry>::ReadFromProcessMemory(
      reader_, registry_addr_);
  if (!registry.has_value())
    return false;

  registry_ = *registry;
  ThreadCache* head = registry_.get()->list_head_;
  while (head) {
    auto tcache = RawBuffer<ThreadCache>::ReadFromProcessMemory(
        reader_, reinterpret_cast<uintptr_t>(head));
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

uintptr_t ThreadCacheInspector::GetRootAddress() {
  CHECK(!thread_caches_.empty());
  return reinterpret_cast<uintptr_t>(thread_caches_[0].get()->root_);
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

void PartitionRootInspector::Update() {
  auto root =
      RawBuffer<PartitionRoot>::ReadFromProcessMemory(reader_, root_addr_);
  if (root.has_value())
    root_ = *root;
}

namespace {

bool CopySlotSpanList(
    std::vector<SlotSpanMetadata<MetadataKind::kReadOnly>>& list,
    uintptr_t head_address,
    RemoteProcessMemoryReader& reader) {
  std::optional<RawBuffer<SlotSpanMetadata<MetadataKind::kReadOnly>>> metadata;
  for (uintptr_t slot_span_address = head_address; slot_span_address;
       slot_span_address =
           reinterpret_cast<uintptr_t>(metadata->get()->next_slot_span)) {
    metadata = RawBuffer<SlotSpanMetadata<MetadataKind::kReadOnly>>::
        ReadFromProcessMemory(reader, slot_span_address);
    if (!metadata.has_value())
      return false;
    list.push_back(*metadata->get());
  }

  return true;
}

}  // namespace

bool PartitionRootInspector::GatherStatistics() {
  // This is going to take a while, make sure that the metadata don't change.
  ScopedSigStopper stopper{pid_};

  Update();
  bucket_stats_.clear();

  for (auto& bucket : root_.get()->buckets) {
    BucketStats stats;
    stats.slot_size = bucket.slot_size;
    stats.bucket = bucket;

    // Only look at the small buckets.
    if (bucket.slot_size > 4096)
      return true;

    bool ok = CopySlotSpanList(
        stats.active_slot_spans,
        reinterpret_cast<uintptr_t>(bucket.active_slot_spans_head), reader_);
    if (!ok)
      return false;

    ok = CopySlotSpanList(
        stats.empty_slot_spans,
        reinterpret_cast<uintptr_t>(bucket.empty_slot_spans_head), reader_);
    if (!ok)
      return false;

    ok = CopySlotSpanList(
        stats.decommitted_slot_spans,
        reinterpret_cast<uintptr_t>(bucket.decommitted_slot_spans_head),
        reader_);
    if (!ok)
      return false;

    for (const auto& active_slot_span : stats.active_slot_spans) {
      uint16_t allocated_slots = active_slot_span.num_allocated_slots;

      stats.allocated_slots += allocated_slots;
      size_t allocated_unprovisioned = active_slot_span.num_allocated_slots +
                                       active_slot_span.num_unprovisioned_slots;
      // Inconsistent data. This can happen since we stopped the process at an
      // arbitrary point.
      if (allocated_unprovisioned > bucket.get_slots_per_span())
        return false;

      size_t freelist_size =
          active_slot_span.is_decommitted()
              ? 0
              : (bucket.get_slots_per_span() - allocated_unprovisioned);

      stats.freelist_size += freelist_size;
      stats.freelist_sizes.push_back(freelist_size);
    }

    // Full slot spans are not in any list.
    stats.allocated_slots +=
        bucket.num_full_slot_spans * bucket.get_slots_per_span();
    bucket_stats_.push_back(stats);
  }

  // We should have found at least one bucket too large, and returned earlier.
  return false;
}

void DisplayBucket(const ThreadCacheInspector::BucketStats& bucket,
                   bool is_limit) {
  size_t bucket_memory = bucket.size * bucket.count;

  std::string line = base::StringPrintf(
      "% 4d\t% 4d\t% 4d\t% 4dkiB", static_cast<int>(bucket.size),
      static_cast<int>(bucket.per_thread_limit), static_cast<int>(bucket.count),
      static_cast<int>(bucket_memory / 1024));

  std::cout << (is_limit ? "*" : " ") << line;
}

void DisplayPerThreadData(
    ThreadCacheInspector& inspector,
    std::map<base::PlatformThreadId, std::string>& tid_to_name) {
  std::cout << "Found " << inspector.thread_caches().size()
            << " caches, total cached memory = "
            << inspector.CachedMemory() / 1024 << "kiB"
            << "\n";

  std::cout << "Per thread:\n"
            << "Thread Name         Size\tPurge\n"
            << std::string(80, '-') << "\n";
  ThreadCacheStats all_threads_stats = {0};
  for (const auto& tcache : inspector.thread_caches()) {
    ThreadCacheStats stats = {0};
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
}

void DisplayPerBucketData(ThreadCacheInspector& inspector) {
  std::cout << "Per-bucket stats (All Threads):"
            << "\nSize\tLimit\tCount\tMemory\t| Size\t\tLimit\tCount\tMemory\n"
            << std::string(80, '-') << "\n";

  size_t total_memory = 0;
  auto bucket_stats = inspector.AccumulateThreadCacheBuckets();
  for (size_t index = 0; index < bucket_stats.size() / 2; index++) {
    size_t bucket_index = index;
    auto& bucket = bucket_stats[bucket_index];
    total_memory += bucket.size * bucket.count;
    DisplayBucket(bucket,
                  inspector.largest_active_bucket_index() == bucket_index);

    std::cout << "\t| ";

    bucket_index = bucket_stats.size() / 2 + index;
    bucket = bucket_stats[bucket_index];
    total_memory += bucket.size * bucket.count;
    DisplayBucket(bucket_stats[bucket_index],
                  inspector.largest_active_bucket_index() == bucket_index);

    std::cout << "\n";
  }
  std::cout << "\nALL THREADS TOTAL: " << total_memory / 1024 << "kiB\n";
}

void DisplayRootData(PartitionRootInspector& root_inspector,
                     size_t detailed_bucket_index) {
  std::cout << "Per-bucket size / allocated slots / free slots / slot span "
               "count:\n";
  for (size_t i = 0; i < root_inspector.bucket_stats().size(); i++) {
    const auto& bucket_stats = root_inspector.bucket_stats()[i];

    std::string line = base::StringPrintf(
        "|% 5d % 6d % 6d % 4d|", static_cast<int>(bucket_stats.slot_size),
        static_cast<int>(bucket_stats.allocated_slots),
        static_cast<int>(bucket_stats.freelist_size),
        static_cast<int>(bucket_stats.active_slot_spans.size()));

    std::cout << line;
    if (i % 4 == 3)
      std::cout << "\n";
    else
      std::cout << "\t";
  }

  const auto& bucket_stats =
      root_inspector.bucket_stats()[detailed_bucket_index];
  std::cout << "\nFreelist size for active buckets of size = "
            << bucket_stats.slot_size << "\n";
  for (size_t freelist_size : bucket_stats.freelist_sizes)
    std::cout << freelist_size << " ";
  std::cout << "\n";

  auto* root = root_inspector.root();
  uint64_t syscall_count = root->syscall_count.load(std::memory_order_relaxed);
  uint64_t total_duration_ms =
      root->syscall_total_time_ns.load(std::memory_order_relaxed) / 1e6;

  uint64_t virtual_size =
      root->total_size_of_super_pages.load(std::memory_order_relaxed) +
      root->total_size_of_direct_mapped_pages.load(std::memory_order_relaxed);

  std::cout
      << "\n\nSyscall count = " << syscall_count
      << "\tTotal duration = " << total_duration_ms << "ms\n"
      << "Max committed size = "
      << root->max_size_of_committed_pages.load(std::memory_order_relaxed) /
             1024
      << "kiB\n"
      << "Allocated/Committed/Virtual = "
      << root->get_total_size_of_allocated_bytes() / 1024 << " / "
      << root->total_size_of_committed_pages.load(std::memory_order_relaxed) /
             1024
      << " / " << virtual_size / 1024 << " kiB\n";
  std::cout << "\nEmpty Slot Spans Dirty Size = "
            << TS_UNCHECKED_READ(root->empty_slot_spans_dirty_bytes) / 1024
            << "kiB";
}

base::Value::Dict Dump(PartitionRootInspector& root_inspector) {
  auto slot_span_to_value = [](const SlotSpanMetadata<MetadataKind::kReadOnly>&
                                   slot_span,
                               size_t slots_per_span) {
    base::Value::Dict result;

    result.Set("num_allocated_slots", slot_span.num_allocated_slots);
    result.Set("num_unprovisioned_slots", slot_span.num_unprovisioned_slots);
    result.Set("freelist_is_sorted", slot_span.freelist_is_sorted());
    result.Set("freelist_is_sorted", slot_span.freelist_is_sorted());
    size_t freelist_size =
        slot_span.is_decommitted()
            ? 0
            : (slots_per_span - slot_span.num_allocated_slots -
               slot_span.num_unprovisioned_slots);
    result.Set("freelist_size", static_cast<int>(freelist_size));
    result.Set("marked_full", slot_span.marked_full);
    result.Set("is_empty", slot_span.is_empty());
    result.Set("is_decommitted", slot_span.is_decommitted());
    return result;
  };

  auto bucket_to_value = [&](const PartitionRootInspector::BucketStats& stats) {
    base::Value::Dict result;
    const size_t kPageSize = base::GetPageSize();
    size_t slots_per_span =
        (stats.bucket.num_system_pages_per_slot_span * kPageSize) /
        stats.slot_size;

    result.Set("slot_size", static_cast<int>(stats.slot_size));
    result.Set("num_system_pages_per_slot_span",
               stats.bucket.num_system_pages_per_slot_span);
    result.Set("num_slots_per_span", static_cast<int>(slots_per_span));
    result.Set("num_full_slot_spans", stats.bucket.num_full_slot_spans);
    result.Set("allocated_slots", static_cast<int>(stats.allocated_slots));
    result.Set("freelist_size", static_cast<int>(stats.freelist_size));

    base::Value::List active_list;
    for (auto& slot_span : stats.active_slot_spans) {
      active_list.Append(slot_span_to_value(slot_span, slots_per_span));
    }
    result.Set("active_slot_spans", std::move(active_list));

    base::Value::List empty_list;
    for (auto& slot_span : stats.empty_slot_spans) {
      empty_list.Append(slot_span_to_value(slot_span, slots_per_span));
    }
    result.Set("empty_slot_spans", std::move(empty_list));

    base::Value::List decommitted_list;
    for (auto& slot_span : stats.decommitted_slot_spans) {
      decommitted_list.Append(slot_span_to_value(slot_span, slots_per_span));
    }
    result.Set("decommitted_slot_spans", std::move(decommitted_list));

    return result;
  };

  base::Value::List bucket_stats;
  for (const auto& stats : root_inspector.bucket_stats()) {
    bucket_stats.Append(bucket_to_value(stats));
  }

  base::Value::Dict result;
  result.Set("buckets", std::move(bucket_stats));
  return result;
}
}  // namespace partition_alloc::tools

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch("pid")) {
    LOG(ERROR) << "Usage:" << argv[0] << " --pid=<PID> [--json=<FILENAME>]";
    return 1;
  }

  int pid = atoi(base::CommandLine::ForCurrentProcess()
                     ->GetSwitchValueASCII("pid")
                     .c_str());
  LOG(WARNING) << "PID = " << pid;

  base::FilePath json_filename =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath("json");

  // Scan the memory.
  partition_alloc::tools::RemoteProcessMemoryReader reader{pid};
  uintptr_t registry_address =
      partition_alloc::tools::FindThreadCacheRegistry(reader);
  CHECK(registry_address);

  LOG(INFO) << "Getting the thread cache registry";
  partition_alloc::tools::ThreadCacheInspector thread_cache_inspector{
      registry_address, pid};
  std::map<base::PlatformThreadId, std::string> tid_to_name;

  size_t iter = 0;
  while (true) {
    constexpr const char* kClearScreen = "\033[2J\033[1;1H";
    std::cout << kClearScreen;
    std::cout.flush();

    base::TimeTicks tick = base::TimeTicks::Now();
    bool ok = thread_cache_inspector.GetAllThreadCaches();
    if (!ok)
      continue;

    partition_alloc::tools::PartitionRootInspector root_inspector{
        thread_cache_inspector.GetRootAddress(), pid};
    bool has_bucket_stats = root_inspector.GatherStatistics();

    for (const auto& tcache : thread_cache_inspector.thread_caches()) {
      // Note: this is not robust when TIDs are reused, but here this is fine,
      // as at worst we would display wrong data, and TID reuse is very unlikely
      // in normal scenarios.
      if (tid_to_name.find(tcache.get()->thread_id()) == tid_to_name.end()) {
        tid_to_name = partition_alloc::tools::ThreadNames(pid);
        break;
      }
    }
    int64_t gather_time_ms = (base::TimeTicks::Now() - tick).InMilliseconds();

    std::cout << "Time to gather data = " << gather_time_ms << "ms\n";
    DisplayPerThreadData(thread_cache_inspector, tid_to_name);

    std::cout << "\n\n";
    DisplayPerBucketData(thread_cache_inspector);

    if (has_bucket_stats) {
      std::cout << "\n\n";
      DisplayRootData(root_inspector,
                      (iter / 50) % root_inspector.bucket_stats().size());

      if (!json_filename.empty()) {
        base::Value::Dict dump = Dump(root_inspector);
        std::string json_string;
        ok = base::JSONWriter::WriteWithOptions(
            dump, base::JSONWriter::Options::OPTIONS_PRETTY_PRINT,
            &json_string);
        if (ok) {
          auto f =
              base::File(json_filename, base::File::Flags::FLAG_OPEN_ALWAYS |
                                            base::File::Flags::FLAG_WRITE);
          if (f.IsValid()) {
            f.WriteAtCurrentPos(base::as_byte_span(json_string));
            std::cout << "\n\nDumped JSON to " << json_filename << std::endl;
            return 0;
          }
        }
        std::cout << "\n\nFailed to dump JSON to " << json_filename
                  << std::endl;
        return 1;
      }
    }

    std::cout << std::endl;
    usleep(200'000);
    iter++;
  }
}
