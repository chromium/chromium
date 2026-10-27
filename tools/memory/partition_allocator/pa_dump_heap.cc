// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Dumps PartitionAlloc's heap into a file.

#include <sys/types.h>
#include <unistd.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/bits.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/page_size.h"
#include "base/strings/stringprintf.h"
#include "base/thread_annotations.h"
#include "base/values.h"
#include "partition_alloc/bucket_lookup.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/in_slot_metadata.h"
#include "partition_alloc/internal/partition_page_internal.h"  // nogncheck
#include "partition_alloc/internal/partition_root_internal.h"  // nogncheck
#include "partition_alloc/internal/thread_cache_internal.h"    // nogncheck
#include "partition_alloc/partition_alloc_config.h"
#include "third_party/snappy/src/snappy.h"
#include "tools/memory/partition_allocator/inspect_utils.h"

namespace partition_alloc::tools {

using partition_alloc::internal::kSuperPageSize;
using partition_alloc::internal::PartitionPageMetadata;
using partition_alloc::internal::PartitionPageSize;
using partition_alloc::internal::PartitionSuperPageExtentEntry;
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
  if (pagemap_fd < 0) {
    return std::nullopt;
  }
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
  ~HeapDumper() = default;

  bool FindRoot() {
    root_address_ = FindRootAddress(reader_);
    if (!root_address_) {
      return false;
    }
    auto root =
        RawBuffer<PartitionRoot>::ReadFromProcessMemory(reader_, root_address_);
    if (!root) {
      return false;
    }
    root_ = *root;
    return true;
  }

  bool DumpSuperPages() {
    std::vector<uintptr_t> super_pages;
    // There is no list of super page, only a list of extents. Walk the extent
    // list to get all superpages.
    const std::ptrdiff_t metadata_offset =
        internal::GetMetadataOffset(root_.get());
    uintptr_t extent_address =
        reinterpret_cast<uintptr_t>(root_.get()->first_extent_);
    while (extent_address) {
      auto extent =
          RawBuffer<PartitionSuperPageExtentEntry>::ReadFromProcessMemory(
              reader_, extent_address);
      if (!extent) {
        LOG(WARNING) << base::StringPrintf("Cannot read from extent 0x%lx",
                                           extent_address);
        return false;
      }
      uintptr_t first_super_page_address =
          ::base::bits::AlignDown(internal::PartitionMetadataPageToSuperPage(
                                      extent_address, metadata_offset),
                                  internal::kSuperPageAlignment);
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
      auto local_super_page = std::make_unique<char[]>(kSuperPageSize);
      if (!reader_.ReadMemory(super_page, kSuperPageSize,
                              local_super_page.get())) {
        LOG(WARNING) << base::StringPrintf("Cannot read from super page 0x%lx",
                                           super_page);
        continue;
      }
      char* local_metadata_page = local_super_page.get() + SystemPageSize();
      if (metadata_offset != static_cast<std::ptrdiff_t>(SystemPageSize())) {
        uintptr_t metadata_page = internal::PartitionSuperPageToMetadataPage(
            super_page, metadata_offset);
        if (!reader_.ReadMemory(metadata_page, SystemPageSize(),
                                local_metadata_page)) {
          LOG(WARNING) << base::StringPrintf(
              "Cannot read from metadata page 0x%lx", metadata_page);
          continue;
        }
      }

      // Relocate bucket pointers in the local metadata copy to point into our
      // local copy of PartitionRoot (`root_`).
      const uintptr_t buckets_begin =
          root_address_ + offsetof(PartitionRoot, buckets_);
      const uintptr_t buckets_end =
          buckets_begin + sizeof(root_.get()->buckets_);
      auto* metadata_array =
          reinterpret_cast<PartitionPageMetadata*>(local_metadata_page);
      for (size_t i = 1; i < kSuperPageSize / PartitionPageSize() - 1; ++i) {
        auto& slot_span = metadata_array[i].slot_span_metadata;
        if (metadata_array[i].slot_span_metadata_offset == 0 &&
            slot_span.bucket) {
          uintptr_t bucket_addr = reinterpret_cast<uintptr_t>(slot_span.bucket);
          bool valid =
              bucket_addr >= buckets_begin && bucket_addr < buckets_end &&
              (bucket_addr - buckets_begin) % sizeof(PartitionRoot::Bucket) ==
                  0;
          const_cast<PartitionRoot::Bucket*&>(slot_span.bucket) =
              valid ? &const_cast<PartitionRoot*>(root_.get())
                           ->buckets_[(bucket_addr - buckets_begin) /
                                      sizeof(PartitionRoot::Bucket)]
                    : nullptr;
        }
      }

      super_pages_.emplace(super_page, std::move(local_super_page));
    }

    uintptr_t table_addr = reinterpret_cast<uintptr_t>(
        root_.get()->GetReservationOffsetTable().GetData());
    reservation_offset_table_.resize(internal::kMaxSuperPagesInPool);
    if (!table_addr ||
        !reader_.ReadMemory(
            table_addr, internal::kMaxSuperPagesInPool * sizeof(uint16_t),
            reinterpret_cast<char*>(reservation_offset_table_.data()))) {
      LOG(WARNING) << base::StringPrintf(
          "Cannot read ReservationOffsetTable at 0x%lx", table_addr);
      reservation_offset_table_.clear();
    }

    LOG(WARNING) << "Read all super pages";
    return !super_pages_.empty();
  }

  base::ListValue Dump() const {
    auto partition_page_to_value = [this](uintptr_t address, uintptr_t offset,
                                          const char* data) {
      base::DictValue ret;
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
        const auto* page_metadata = GetPartitionPageMetadata(address + offset);
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
      base::DictValue ret;
      ret.Set("address", base::StringPrintf("0x%lx", address));

      base::ListValue partition_pages;
      for (uintptr_t offset = 0; offset < kSuperPageSize;
           offset += PartitionPageSize()) {
        partition_pages.Append(partition_page_to_value(address, offset, data));
      }
      ret.Set("partition_pages", std::move(partition_pages));

      base::ListValue page_sizes;
      // Looking at how well the heap would compress.
      const size_t page_size = base::GetPageSize();
      for (uintptr_t page_address = address;
           page_address < address + partition_alloc::internal::kSuperPageSize;
           page_address += page_size) {
        auto maybe_pagemap_entry = EntryAtAddress(pagemap_fd_, page_address);
        size_t uncompressed_size = 0, compressed_size = 0;
        const char* page_data = data + (page_address - address);

        bool all_zeros = true;
        for (size_t i = 0; i < page_size; i++) {
          if (reinterpret_cast<const unsigned char*>(page_data)[i]) {
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
          compressed_size = snappy::Compress(page_data, page_size, &compressed);
        }

        base::DictValue page_size_dict;
        page_size_dict.Set("uncompressed", static_cast<int>(uncompressed_size));
        page_size_dict.Set("compressed", static_cast<int>(compressed_size));
        page_sizes.Append(std::move(page_size_dict));
      }
      ret.Set("page_sizes", std::move(page_sizes));

      return ret;
    };

    base::ListValue super_pages_value;
    for (const auto& address_data : super_pages_) {
      super_pages_value.Append(
          super_page_to_value(address_data.first, address_data.second.get()));
    }

    return super_pages_value;
  }

#if PA_CONFIG(IN_SLOT_METADATA_STORE_REQUESTED_SIZE)
  base::ListValue DumpAllocatedSizes() {
    // Since there is no tracking of full slot spans, the way to enumerate all
    // allocated memory is to walk the heap itself.
    base::ListValue ret;

    for (const auto& address_data : super_pages_) {
      uintptr_t super_page = address_data.first;
      const char* data = address_data.second.get();
      // Exclude the first and last partition pagers: metadata and guard,
      // respectively.
      size_t partition_page_index = 1;
      while (partition_page_index < kSuperPageSize / PartitionPageSize() - 1) {
        uintptr_t slot_span_start =
            super_page + partition_page_index * PartitionPageSize();
        const auto* partition_page = GetPartitionPageMetadata(slot_span_start);
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

        base::DictValue slot_span_value;
        slot_span_value.Set("start_address",
                            base::StringPrintf("0x%lx", slot_span_start));
        slot_span_value.Set("slot_size",
                            static_cast<int>(metadata.bucket->slot_size));

        // There is no tracking of allocated slots, need to reconstruct
        // these as everything which is not in the freelist.
        std::vector<bool> free_slots(metadata.bucket->get_slots_per_span());
        const uintptr_t slot_span_end =
            std::min(slot_span_start + metadata.bucket->get_bytes_per_span(),
                     super_page + kSuperPageSize - PartitionPageSize());
        auto* head = metadata.get_freelist_head();
        for (size_t steps = 0; head && steps < free_slots.size(); ++steps) {
          uintptr_t head_address = reinterpret_cast<uintptr_t>(head);
          if (head_address < slot_span_start || head_address >= slot_span_end) {
            break;
          }
          size_t offset_in_slot_span = head_address - slot_span_start;
          size_t slot_number =
              metadata.bucket->GetSlotNumber(offset_in_slot_span);
          if (slot_number < free_slots.size()) {
            free_slots[slot_number] = true;
          }
          const auto* local_head =
              reinterpret_cast<const internal::FreelistEntry*>(
                  data + (head_address - super_page));
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
          head = local_head->GetNextForThreadCache(
              metadata.bucket->slot_size, root_.get()->GetOffsetLookup());
#else
          head = local_head->GetNextForThreadCache(metadata.bucket->slot_size);
#endif
        }

        base::ListValue allocated_sizes_value;
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
          uintptr_t in_slot_metadata_address = reinterpret_cast<uintptr_t>(
              PartitionRoot::InSlotMetadataPointerFromSlotStartAndSize(
                  internal::UntaggedSlotStart::Unchecked(slot_start),
                  metadata.bucket->slot_size));
          const auto* in_slot_metadata =
              reinterpret_cast<const internal::InSlotMetadata*>(
                  data + (in_slot_metadata_address - super_page));
          uint32_t requested_size = in_slot_metadata->requested_size();

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

  base::ListValue DumpBuckets() {
    base::ListValue ret;
    for (const auto& bucket : root_.get()->buckets_) {
      base::DictValue bucket_value;
      bucket_value.Set("slot_size", static_cast<int>(bucket.slot_size));
      ret.Append(std::move(bucket_value));
    }

    return ret;
  }

  base::DictValue DumpGigacage() const {
    base::DictValue ret;
    if (super_pages_.empty() || reservation_offset_table_.empty()) {
      return ret;
    }
    uintptr_t pool_base =
        super_pages_.begin()->first & ~(internal::kPoolMaxSize - 1);

    base::ListValue superpages;
    for (uint16_t val : reservation_offset_table_) {
      superpages.Append(
          val == internal::ReservationOffsetTable::kOffsetTagNotAllocated
              ? "free"
          : val == internal::ReservationOffsetTable::kOffsetTagNormalBuckets
              ? "normal"
              : "direct_mapped");
    }
    ret.Set("pool_base", base::StringPrintf("0x%lx", pool_base));
    ret.Set("superpages", std::move(superpages));
    return ret;
  }

 private:
  const PartitionPageMetadata* GetPartitionPageMetadata(
      uintptr_t address) const {
    uintptr_t super_page = address & internal::kSuperPageBaseMask;
    uintptr_t partition_page_index =
        (address & internal::kSuperPageOffsetMask) >>
        internal::PartitionPageShift();
    const char* metadata_area =
        super_pages_.at(super_page).get() + SystemPageSize();
    return reinterpret_cast<const PartitionPageMetadata*>(metadata_area) +
           partition_page_index;
  }

  static uintptr_t FindRootAddress(RemoteProcessMemoryReader& reader)
      NO_THREAD_SAFETY_ANALYSIS {
    uintptr_t tls_registry_address = IndexThreadCacheNeedleArray(reader, 1);
    auto registry =
        RawBuffer<internal::PartitionTlsRegistry>::ReadFromProcessMemory(
            reader, tls_registry_address);
    if (!registry)
      return 0;

    auto tls_address =
        reinterpret_cast<uintptr_t>(registry->get()->list_head());
    while (tls_address) {
      auto tls = RawBuffer<internal::PartitionTls>::ReadFromProcessMemory(
          reader, tls_address);
      if (!tls) {
        return 0;
      }

      auto* tcache =
          tls->get()->GetThreadCache(internal::kDefaultRootThreadCacheIndex);
      if (internal::ThreadCache::IsValid(tcache)) {
        return reinterpret_cast<uintptr_t>(tcache->root_);
      }
      tls_address = reinterpret_cast<uintptr_t>(tls->get()->next_);
    }
    return 0;
  }

  const int pagemap_fd_;
  uintptr_t root_address_ = 0;
  RemoteProcessMemoryReader reader_;
  RawBuffer<PartitionRoot> root_ = {};
  std::map<uintptr_t, std::unique_ptr<char[]>> super_pages_ = {};
  std::vector<uint16_t> reservation_offset_table_ = {};
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
      return 1;
    }
  }

  base::DictValue overall_dump;
  overall_dump.Set("superpages", dumper.Dump());
  overall_dump.Set("gigacage", dumper.DumpGigacage());

#if PA_CONFIG(IN_SLOT_METADATA_STORE_REQUESTED_SIZE)
  overall_dump.Set("allocated_sizes", dumper.DumpAllocatedSizes());
#endif  // PA_CONFIG(IN_SLOT_METADATA_STORE_REQUESTED_SIZE)

  overall_dump.Set("buckets", dumper.DumpBuckets());

  std::string json_string;
  bool ok = base::JSONWriter::WriteWithOptions(
      overall_dump, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json_string);

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
