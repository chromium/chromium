// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Dumps PartitionAlloc's heap into a file.

#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>

#include "base/bits.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/page_size.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/strings/stringprintf.h"
#include "base/thread_annotations.h"
#include "base/values.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/in_slot_metadata.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/partition_page.h"
#include "partition_alloc/partition_root.h"
#include "partition_alloc/thread_cache.h"
#include "third_party/snappy/src/snappy.h"
#include "tools/memory/partition_allocator/inspect_utils.h"

namespace partition_alloc::tools {

using partition_alloc::internal::kInvalidBucketSize;
using partition_alloc::internal::kSuperPageSize;
using partition_alloc::internal::MetadataKind;
using partition_alloc::internal::PartitionPageSize;
template <MetadataKind kind>
using PartitionPageMetadata =
    partition_alloc::internal::PartitionPageMetadata<kind>;
template <MetadataKind kind>
using PartitionSuperPageExtentEntry =
    partition_alloc::internal::PartitionSuperPageExtentEntry<kind>;
using partition_alloc::internal::SystemPageSize;

// See https://www.kernel.org/doc/Documentation/vm/pagemap.txt.
struct PageMapEntry {
  uint64_t pfn_or_swap : 55;
  uint64_t soft_dirty : 1;
  uint64_t exclusively_mapped : 1;
  uint64_t unused : 4;
  uint64_t file_mapped_or_shared_anon : 1;
  uint64_t swapped : 1;
  uint64_t present : 1;
};
static_assert(sizeof(PageMapEntry) == sizeof(uint64_t), "Wrong bitfield size");

std::optional<PageMapEntry> EntryAtAddress(int pagemap_fd, uintptr_t address) {
  constexpr size_t kPageShift = 12;
  off_t offset = (address >> kPageShift) * sizeof(PageMapEntry);
  if (lseek(pagemap_fd, offset, SEEK_SET) != offset)
    return std::nullopt;

  PageMapEntry entry;
  if (read(pagemap_fd, &entry, sizeof(PageMapEntry)) != sizeof(PageMapEntry))
    return std::nullopt;

  return {entry};
}

class HeapDumper {
 public:
  HeapDumper(pid_t pid, int pagemap_fd)
      : pagemap_fd_(pagemap_fd), reader_(pid) {}
  ~HeapDumper() {
    for (const auto& p : super_pages_) {
      munmap(p.second, kSuperPageSize);
    }
    if (local_root_copy_mapping_base_) {
      munmap(local_root_copy_mapping_base_, local_root_copy_mapping_size_);
    }
  }

  bool FindRoot() {
    root_address_ = FindRootAddress(reader_);
    CHECK(root_address_);
    auto root =
        RawBuffer<PartitionRoot>::ReadFromProcessMemory(reader_, root_address_);
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
    size_t size_to_map = ::base::bits::AlignUp(
        sizeof(PartitionRoot) + SystemPageSize(), SystemPageSize());
    uintptr_t address_to_map =
        ::base::bits::AlignDown(root_address_, SystemPageSize());
    char* local_memory = CreateMappingAtAddress(address_to_map, size_to_map);
    if (!local_memory) {
      LOG(WARNING) << base::StringPrintf(
          "Cannot map memory at %lx",
          reinterpret_cast<uintptr_t>(address_to_map));
      return false;
    }
    local_root_copy_ = local_memory;

    memcpy(reinterpret_cast<void*>(root_address_), root_.get(),
           sizeof(PartitionRoot));
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
          RawBuffer<PartitionSuperPageExtentEntry<MetadataKind::kReadOnly>>::
              ReadFromProcessMemory(reader_, extent_address);
      uintptr_t first_super_page_address = SuperPagesBeginFromExtent(
          reinterpret_cast<
              PartitionSuperPageExtentEntry<MetadataKind::kReadOnly>*>(
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
          reader_.ReadAtSameAddressInLocalMemory(super_page, kSuperPageSize);
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

  base::Value::List Dump() const {
    auto partition_page_to_value = [](uintptr_t offset, const char* data) {
      base::Value::Dict ret;
      std::string value;
      if (offset == 0) {
        value = "metadata";
      } else if (offset == kSuperPageSize - PartitionPageSize()) {
        value = "guard";
      } else {
        value = "payload";
      }
      ret.Set("type", value);

      if (value != "metadata" && value != "guard") {
        const auto* page_metadata =
            PartitionPageMetadata<MetadataKind::kReadOnly>::FromAddr(
                reinterpret_cast<uintptr_t>(data + offset));
        ret.Set("page_index_in_span", page_metadata->slot_span_metadata_offset);
        if (page_metadata->slot_span_metadata_offset == 0 &&
            page_metadata->slot_span_metadata.bucket) {
          const auto& slot_span_metadata = page_metadata->slot_span_metadata;
          ret.Set("slot_size",
                  static_cast<int>(slot_span_metadata.bucket->slot_size));
          ret.Set("is_active", slot_span_metadata.is_active());
          ret.Set("is_full", slot_span_metadata.is_full());
          ret.Set("is_empty", slot_span_metadata.is_empty());
          ret.Set("is_decommitted", slot_span_metadata.is_decommitted());
          ret.Set("slots_per_span",
                  static_cast<int>(
                      slot_span_metadata.bucket->get_slots_per_span()));
          ret.Set(
              "num_system_pages_per_slot_span",
              static_cast<int>(
                  slot_span_metadata.bucket->num_system_pages_per_slot_span));
          ret.Set("num_allocated_slots",
                  slot_span_metadata.num_allocated_slots);
          ret.Set("num_unprovisioned_slots",
                  slot_span_metadata.num_unprovisioned_slots);
        }
      }

      bool all_zeros = true;
      for (size_t i = 0; i < PartitionPageSize(); i++) {
        if (data[offset + i]) {
          all_zeros = false;
          break;
        }
      }
      ret.Set("all_zeros", all_zeros);

      return ret;
    };
    auto super_page_to_value = [&](uintptr_t address, const char* data) {
      base::Value::Dict ret;
      ret.Set("address", base::StringPrintf("0x%lx", address));

      base::Value::List partition_pages;
      for (uintptr_t offset = 0; offset < kSuperPageSize;
           offset += PartitionPageSize()) {
        partition_pages.Append(partition_page_to_value(offset, data));
      }
      ret.Set("partition_pages", std::move(partition_pages));

      base::Value::List page_sizes;
      // Looking at how well the heap would compress.
      const size_t page_size = base::GetPageSize();
      for (uintptr_t page_address = address;
           page_address < address + partition_alloc::internal::kSuperPageSize;
           page_address += page_size) {
        auto maybe_pagemap_entry = EntryAtAddress(pagemap_fd_, page_address);
        size_t uncompressed_size = 0, compressed_size = 0;

        bool all_zeros = true;
        for (size_t i = 0; i < page_size; i++) {
          if (reinterpret_cast<unsigned char*>(page_address)[i]) {
            all_zeros = false;
            break;
          }
        }

        bool should_report;
        if (!maybe_pagemap_entry) {
          // We cannot tell whether a page has been decommitted, but all-zero
          // likely indicates that. Only report data for pages that the other
          // pages.
          should_report = !all_zeros;
        } else {
          // If it's not in memory and not in swap, only the PTE exists.
          should_report =
              maybe_pagemap_entry->present || maybe_pagemap_entry->swapped;
        }

        if (should_report) {
          std::string compressed;
          uncompressed_size = page_size;
          // Use snappy to approximate what a fast compression algorithm
          // operating with a page granularity would do. This is not the
          // algorithm used in either Linux or macOS, but should give some
          // indication.
          compressed_size =
              snappy::Compress(reinterpret_cast<const char*>(page_address),
                               page_size, &compressed);
        }

        base::Value::Dict page_size_dict;
        page_size_dict.Set("uncompressed", static_cast<int>(uncompressed_size));
        page_size_dict.Set("compressed", static_cast<int>(compressed_size));
        page_sizes.Append(std::move(page_size_dict));
      }
      ret.Set("page_sizes", std::move(page_sizes));

      return ret;
    };

    base::Value::List super_pages_value;
    for (const auto& address_data : super_pages_) {
      super_pages_value.Append(
          super_page_to_value(address_data.first, address_data.second));
    }

    return super_pages_value;
  }

#if PA_CONFIG(IN_SLOT_METADATA_STORE_REQUESTED_SIZE)
  base::Value::List DumpAllocatedSizes() {
    // Note: Here and below, it is safe to follow pointers into the super page,
    // or to the root or buckets, since they share the same address in the this
    // process as in the Chromium process.

    // Since there is no tracking of full slot spans, the way to enumerate all
    // allocated memory is to walk the heap itself.
    base::Value::List ret;

    for (const auto& address_data : super_pages_) {
      const char* data = address_data.second;
      // Exclude the first and last partition pagers: metadata and guard,
      // respectively.
      size_t partition_page_index = 1;
      while (partition_page_index < kSuperPageSize / PartitionPageSize() - 1) {
        uintptr_t slot_span_start = reinterpret_cast<uintptr_t>(
            data + partition_page_index * PartitionPageSize());
        const auto* partition_page = PartitionPage::FromAddr(slot_span_start);
        // No bucket for PartitionPages that were never provisioned.
        if (!partition_page->slot_span_metadata.bucket) {
          partition_page_index++;
          continue;
        }

        const auto& metadata = partition_page->slot_span_metadata;
        if (metadata.is_decommitted() || metadata.is_empty()) {
          // Skip this entire slot span, since it doesn't hold live allocations.
          partition_page_index += metadata.bucket->get_pages_per_slot_span();
          continue;
        }

        base::Value::Dict slot_span_value;
        slot_span_value.Set("start_address",
                            base::StringPrintf("0x%lx", slot_span_start));
        slot_span_value.Set("slot_size",
                            static_cast<int>(metadata.bucket->slot_size));

        // There is no tracking of allocated slots, need to reconstruct
        // these as everything which is not in the freelist.
        std::vector<bool> free_slots(metadata.bucket->get_slots_per_span());
        auto* head = metadata.get_freelist_head();
        while (head) {
          size_t offset_in_slot_span =
              reinterpret_cast<uintptr_t>(head) - slot_span_start;
          size_t slot_number =
              metadata.bucket->GetSlotNumber(offset_in_slot_span);
          free_slots[slot_number] = true;
          head = head->GetNext(0);
        }

        base::Value::List allocated_sizes_value;
        for (size_t slot_index = 0; slot_index < free_slots.size();
             slot_index++) {
          // Skip unprovisioned slots, which are always at the end of the slot
          // span.
          if (free_slots[slot_index] ||
              slot_index >= (metadata.bucket->get_slots_per_span() -
                             metadata.num_unprovisioned_slots)) {
            continue;
          }
          uintptr_t slot_start =
              slot_span_start + slot_index * metadata.bucket->slot_size;
          auto* ref_count =
              PartitionRoot::InSlotMetadataPointerFromSlotStartAndSize(
                  slot_start, metadata.bucket->slot_size);
          uint32_t requested_size = ref_count->requested_size();

          // Address space dumping is not synchronized with allocation, meaning
          // that we can observe the heap in an inconsistent state. Skip
          // obviously-wrong entries.
          if (requested_size > metadata.bucket->slot_size || !requested_size)
            continue;

          allocated_sizes_value.Append(static_cast<int>(requested_size));
        }
        slot_span_value.Set("allocated_sizes",
                            std::move(allocated_sizes_value));

        ret.Append(std::move(slot_span_value));
        partition_page_index += metadata.bucket->get_pages_per_slot_span();
      }
    }

    return ret;
  }
#endif  // PA_CONFIG(IN_SLOT_METADATA_STORE_REQUESTED_SIZE)

  base::Value::List DumpBuckets() {
    base::Value::List ret;
    for (const auto& bucket : root_.get()->buckets) {
      if (bucket.slot_size == kInvalidBucketSize)
        continue;

      base::Value::Dict bucket_value;
      bucket_value.Set("slot_size", static_cast<int>(bucket.slot_size));
      ret.Append(std::move(bucket_value));
    }

    return ret;
  }

 private:
  static uintptr_t FindRootAddress(RemoteProcessMemoryReader& reader)
      NO_THREAD_SAFETY_ANALYSIS {
    uintptr_t tcache_registry_address = IndexThreadCacheNeedleArray(reader, 1);
    auto registry = RawBuffer<ThreadCacheRegistry>::ReadFromProcessMemory(
        reader, tcache_registry_address);
    if (!registry)
      return 0;

    auto tcache_address =
        reinterpret_cast<uintptr_t>(registry->get()->list_head_);
    if (!tcache_address)
      return 0;

    auto tcache =
        RawBuffer<ThreadCache>::ReadFromProcessMemory(reader, tcache_address);
    if (!tcache)
      return 0;

    auto root_address = reinterpret_cast<uintptr_t>(tcache->get()->root_);
    return root_address;
  }

  const int pagemap_fd_;
  uintptr_t root_address_ = 0;
  RemoteProcessMemoryReader reader_;
  RawBuffer<PartitionRoot> root_ = {};
  std::map<uintptr_t, char*> super_pages_ = {};

  char* local_root_copy_ = nullptr;

  // This field is not a raw_ptr<> because it always points to a mmap'd
  // region of memory outside of the PA heap. Thus, there would be overhead
  // involved with using a raw_ptr<> but no safety gains.
  RAW_PTR_EXCLUSION void* local_root_copy_mapping_base_ = nullptr;
  size_t local_root_copy_mapping_size_ = 0;
};

}  // namespace partition_alloc::tools

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch("pid") || !command_line->HasSwitch("json")) {
    LOG(ERROR) << "Usage:" << argv[0] << " --pid=<PID> --json=<FILENAME>";
    return 1;
  }

  int pid = atoi(command_line->GetSwitchValueASCII("pid").c_str());
  LOG(WARNING) << "PID = " << pid;

  auto pagemap_fd = partition_alloc::tools::OpenPagemap(pid);
  partition_alloc::tools::HeapDumper dumper{pid, pagemap_fd.get()};

  {
    partition_alloc::tools::ScopedSigStopper stopper{pid};
    if (!dumper.FindRoot()) {
      LOG(WARNING) << "Cannot find (or copy) the root";
      return 1;
    }
    if (!dumper.DumpSuperPages()) {
      LOG(WARNING) << "Cannot dump (or copy) super pages.";
    }
  }

  base::Value::Dict overall_dump;
  overall_dump.Set("superpages", dumper.Dump());

#if PA_CONFIG(IN_SLOT_METADATA_STORE_REQUESTED_SIZE)
  overall_dump.Set("allocated_sizes", dumper.DumpAllocatedSizes());
#endif  // PA_CONFIG(IN_SLOT_METADATA_STORE_REQUESTED_SIZE)

  overall_dump.Set("buckets", dumper.DumpBuckets());

  std::string json_string;
  bool ok = base::JSONWriter::WriteWithOptions(
      overall_dump, base::JSONWriter::Options::OPTIONS_PRETTY_PRINT,
      &json_string);

  if (ok) {
    base::FilePath json_filename = command_line->GetSwitchValuePath("json");
    auto f = base::File(json_filename, base::File::Flags::FLAG_CREATE_ALWAYS |
                                           base::File::Flags::FLAG_WRITE);
    if (f.IsValid()) {
      f.WriteAtCurrentPos(base::as_byte_span(json_string));
      LOG(WARNING) << "\n\nDumped JSON to " << json_filename;
      return 0;
    }
  }

  return 1;
}
