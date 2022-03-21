// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Dumps PartitionAlloc's heap into a file.

#include <sys/mman.h>
#include <cstdlib>
#include <cstring>
#include <string>

#include "base/allocator/partition_allocator/partition_root.h"
#include "base/allocator/partition_allocator/thread_cache.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/thread_annotations.h"
#include "base/values.h"
#include "tools/memory/partition_allocator/inspect_utils.h"

namespace partition_alloc::internal::tools {

class HeapDumper {
 public:
  HeapDumper(pid_t pid, int mem_fd) : pid_(pid), mem_fd_(mem_fd) {}
  ~HeapDumper() {
    for (const auto& p : super_pages_) {
      munmap(p.second, kSuperPageSize);
    }
    if (local_root_copy_mapping_base_) {
      munmap(local_root_copy_mapping_base_, local_root_copy_mapping_size_);
    }
  }

  bool FindRoot() {
    root_address_ = FindRootAddress(pid_, mem_fd_);
    CHECK(root_address_);
    auto root = RawBuffer<PartitionRoot<ThreadSafe>>::ReadFromMemFd(
        mem_fd_, root_address_);
    CHECK(root);
    root_ = *root;

    // Since the heap if full of pointers, copying the data to the local address
    // space doesn't allow to follow the pointers, or to call most member
    // functions on the local objects.
    //
    // To make it easier to work with, we copy some objects in the local address
    // space at the *same* address used in the remote process. This is not
    // guaranteed to work though, since the addresses can already be mapped in
    // the local process. However, since we are targeting 64 bit Linux, with
    // ASLR executing again should solve the problem in most cases.
    //
    // Copy at the same address as in the remote process. Since the root is not
    // page-aligned in the remote process, need to pad the mapping a bit.
    size_t size_to_map = base::bits::AlignUp(
        sizeof(PartitionRoot<ThreadSafe>) + SystemPageSize(), SystemPageSize());
    uintptr_t address_to_map =
        base::bits::AlignDown(root_address_, SystemPageSize());
    char* local_memory = CreateMappingAtAddress(address_to_map, size_to_map);
    if (!local_memory) {
      LOG(WARNING) << base::StringPrintf(
          "Cannot map memory at %lx",
          reinterpret_cast<uintptr_t>(address_to_map));
      return false;
    }
    local_root_copy_ = local_memory;

    memcpy(reinterpret_cast<void*>(root_address_), root_.get(),
           sizeof(PartitionRoot<ThreadSafe>));
    local_root_copy_mapping_base_ = reinterpret_cast<void*>(address_to_map);
    local_root_copy_mapping_size_ = size_to_map;

    return true;
  }

  bool DumpSuperPages() {
    std::vector<uintptr_t> super_pages;
    // There is no list of super page, only a list of extents. Walk the extent
    // list to get all superpages.
    uintptr_t extent_address =
        reinterpret_cast<uintptr_t>(root_.get()->first_extent);
    while (extent_address) {
      auto extent =
          RawBuffer<PartitionSuperPageExtentEntry<ThreadSafe>>::ReadFromMemFd(
              mem_fd_, extent_address);
      uintptr_t first_super_page_address = SuperPagesBeginFromExtent(
          reinterpret_cast<PartitionSuperPageExtentEntry<ThreadSafe>*>(
              extent_address));
      for (uintptr_t super_page = first_super_page_address;
           super_page < first_super_page_address +
                            extent->get()->number_of_consecutive_super_pages *
                                kSuperPageSize;
           super_page += kSuperPageSize) {
        super_pages.push_back(super_page);
      }
      extent_address = reinterpret_cast<uintptr_t>(extent->get()->next);
    }

    LOG(WARNING) << "Found " << super_pages.size() << std::hex
                 << " super pages.";
    for (uintptr_t super_page : super_pages) {
      char* local_super_page =
          ReadAtSameAddressInLocalMemory(mem_fd_, super_page, kSuperPageSize);
      if (!local_super_page) {
        LOG(WARNING) << base::StringPrintf("Cannot read from super page 0x%lx",
                                           super_page);
        continue;
      }
      super_pages_.emplace(super_page, local_super_page);
    }
    LOG(WARNING) << "Read all super pages";
    return true;
  }

  base::Value Dump() const {
    auto partition_page_to_value = [](uintptr_t offset,
                                      const char* data) -> base::Value {
      auto ret = base::Value(base::Value::Type::DICTIONARY);
      std::string value;
      if (offset == 0) {
        value = "metadata";
      } else if (offset == kSuperPageSize - PartitionPageSize()) {
        value = "guard";
      } else {
        value = "payload";
      }
      ret.SetKey("type", base::Value{value});

      if (value != "metadata" && value != "guard") {
        const auto* partition_page = PartitionPage<ThreadSafe>::FromAddr(
            reinterpret_cast<uintptr_t>(data + offset));
        ret.SetKey("page_index_in_span",
                   base::Value{partition_page->slot_span_metadata_offset});
        if (partition_page->slot_span_metadata_offset == 0 &&
            partition_page->slot_span_metadata.bucket) {
          const auto& slot_span_metadata = partition_page->slot_span_metadata;
          ret.SetKey("slot_size", base::Value{static_cast<int>(
                                      slot_span_metadata.bucket->slot_size)});
          ret.SetKey("is_active", base::Value{slot_span_metadata.is_active()});
          ret.SetKey("is_full", base::Value{slot_span_metadata.is_full()});
          ret.SetKey("is_empty", base::Value{slot_span_metadata.is_empty()});
          ret.SetKey("is_decommitted",
                     base::Value{slot_span_metadata.is_decommitted()});
          ret.SetKey("slots_per_span",
                     base::Value{static_cast<int>(
                         slot_span_metadata.bucket->get_slots_per_span())});
          ret.SetKey(
              "num_system_pages_per_slot_span",
              base::Value{static_cast<int>(
                  slot_span_metadata.bucket->num_system_pages_per_slot_span)});
          ret.SetKey("num_allocated_slots",
                     base::Value{slot_span_metadata.num_allocated_slots});
          ret.SetKey("num_unprovisioned_slots",
                     base::Value{slot_span_metadata.num_unprovisioned_slots});
        }
      }

      bool all_zeros = true;
      for (size_t i = 0; i < PartitionPageSize(); i++) {
        if (data[offset + i]) {
          all_zeros = false;
          break;
        }
      }
      ret.SetKey("all_zeros", base::Value{all_zeros});
      return ret;
    };
    auto super_page_to_value = [&](uintptr_t address,
                                   const char* data) -> base::Value {
      auto ret = base::Value(base::Value::Type::DICTIONARY);
      ret.SetKey("address", base::Value{base::StringPrintf("0x%lx", address)});

      auto partition_pages = base::Value(base::Value::Type::LIST);
      for (uintptr_t offset = 0; offset < kSuperPageSize;
           offset += PartitionPageSize()) {
        partition_pages.Append(partition_page_to_value(offset, data));
      }
      ret.SetKey("partition_pages", std::move(partition_pages));

      return ret;
    };

    auto super_pages_value = base::Value(base::Value::Type::LIST);
    for (const auto& address_data : super_pages_) {
      super_pages_value.Append(
          super_page_to_value(address_data.first, address_data.second));
    }

    return super_pages_value;
  }

 private:
  static uintptr_t FindRootAddress(pid_t pid,
                                   int mem_fd) NO_THREAD_SAFETY_ANALYSIS {
    uintptr_t tcache_registry_address =
        IndexThreadCacheNeedleArray(pid, mem_fd, 1);
    auto registry =
        RawBuffer<base::internal::ThreadCacheRegistry>::ReadFromMemFd(
            mem_fd, tcache_registry_address);
    if (!registry)
      return 0;

    auto tcache_address =
        reinterpret_cast<uintptr_t>(registry->get()->list_head_);
    if (!tcache_address)
      return 0;

    auto tcache = RawBuffer<base::internal::ThreadCache>::ReadFromMemFd(
        mem_fd, tcache_address);
    if (!tcache)
      return 0;

    auto root_address = reinterpret_cast<uintptr_t>(tcache->get()->root_);
    return root_address;
  }

  const pid_t pid_;
  const int mem_fd_;
  uintptr_t root_address_ = 0;
  RawBuffer<PartitionRoot<ThreadSafe>> root_ = {};
  std::map<uintptr_t, char*> super_pages_ = {};

  char* local_root_copy_ = nullptr;

  void* local_root_copy_mapping_base_ = nullptr;
  size_t local_root_copy_mapping_size_ = 0;
};

}  // namespace partition_alloc::internal::tools

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch("pid") || !command_line->HasSwitch("json")) {
    LOG(ERROR) << "Usage:" << argv[0] << " --pid=<PID> --json=<FILENAME>";
    return 1;
  }

  int pid = atoi(command_line->GetSwitchValueASCII("pid").c_str());
  LOG(WARNING) << "PID = " << pid;

  auto mem_fd = partition_alloc::internal::tools::OpenProcMem(pid);
  partition_alloc::internal::tools::HeapDumper dumper{pid, mem_fd.get()};

  {
    partition_alloc::internal::tools::ScopedSigStopper stopper{pid};
    if (!dumper.FindRoot()) {
      LOG(WARNING) << "Cannot find (or copy) the root";
      return 1;
    }
    if (!dumper.DumpSuperPages()) {
      LOG(WARNING) << "Cannot dump (or copy) super pages.";
    }
  }

  auto dump = dumper.Dump();
  std::string json_string;
  bool ok = base::JSONWriter::WriteWithOptions(
      dump, base::JSONWriter::Options::OPTIONS_PRETTY_PRINT, &json_string);

  if (ok) {
    base::FilePath json_filename = command_line->GetSwitchValuePath("json");
    auto f = base::File(json_filename, base::File::Flags::FLAG_OPEN_ALWAYS |
                                           base::File::Flags::FLAG_WRITE);
    if (f.IsValid()) {
      f.WriteAtCurrentPos(json_string.c_str(), json_string.size());
      LOG(WARNING) << "\n\nDumped JSON to " << json_filename;
      return 0;
    }
  }

  return 1;
}
