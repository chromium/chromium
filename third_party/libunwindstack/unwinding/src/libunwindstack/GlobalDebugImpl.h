/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include <stdint.h>
#include <string.h>
#include <sys/mman.h>

#include <memory>
#include <vector>

#include <unwindstack/Global.h>
#include <unwindstack/Maps.h>

#include "Check.h"
#include "GlobalDebugInterface.h"
#include "MemoryCache.h"
#include "MemoryRange.h"

// This implements the JIT Compilation Interface.
// See https://sourceware.org/gdb/onlinedocs/gdb/JIT-Interface.html
//
// We use it to get in-memory ELF files created by the ART compiler,
// but we also use it to get list of DEX files used by the runtime.

namespace unwindstack {

// Implementation templated for ELF/DEX and for different architectures.
template <typename Symfile, typename Uintptr_T, typename Uint64_T>
class GlobalDebugImpl : public GlobalDebugInterface<Symfile>, public Global {
 public:
  static constexpr int kMaxRaceRetries = 16;
  static constexpr int kMaxHeadRetries = 16;
  static constexpr uint8_t kMagic[8] = {'A', 'n', 'd', 'r', 'o', 'i', 'd', '2'};

  struct JITCodeEntry {
    Uintptr_T next;
    Uintptr_T prev;
    Uintptr_T symfile_addr;
    Uint64_T symfile_size;
    // Android-specific fields:
    Uint64_T timestamp;
    uint32_t seqlock;
  };

  static constexpr size_t kSizeOfCodeEntryV1 = offsetof(JITCodeEntry, timestamp);
  static constexpr size_t kSizeOfCodeEntryV2 = sizeof(JITCodeEntry);

  struct JITDescriptor {
    uint32_t version;
    uint32_t action_flag;
    Uintptr_T relevant_entry;
    Uintptr_T first_entry;
    // Android-specific fields:
    uint8_t magic[8];
    uint32_t flags;
    uint32_t sizeof_descriptor;
    uint32_t sizeof_entry;
    uint32_t seqlock;
    Uint64_T timestamp;
  };

  static constexpr size_t kSizeOfDescriptorV1 = offsetof(JITDescriptor, magic);
  static constexpr size_t kSizeOfDescriptorV2 = sizeof(JITDescriptor);

  // This uniquely identifies entry in presence of concurrent modifications.
  // Each (address,seqlock) pair is unique for each newly created JIT entry.
  struct UID {
    uint64_t address;  // Address of JITCodeEntry in memory.
    uint32_t seqlock;  // This servers as "version" for the given address.

    bool operator<(const UID& other) const {
      return std::tie(address, seqlock) < std::tie(other.address, other.seqlock);
    }
  };

  GlobalDebugImpl(ArchEnum arch, std::shared_ptr<Memory>& memory,
                  std::vector<std::string>& search_libs, const char* global_variable_name)
      : Global(memory, search_libs), global_variable_name_(global_variable_name) {
    SetArch(arch);
  }

  bool ReadDescriptor(uint64_t addr) {
    JITDescriptor desc{};
    // Try to read the full descriptor including Android-specific fields.
    if (!this->memory_->ReadFully(addr, &desc, kSizeOfDescriptorV2)) {
      // Fallback to just the minimal descriptor.
      // This will make the magic check below fail.
      if (!this->memory_->ReadFully(addr, &desc, kSizeOfDescriptorV1)) {
        return false;
      }
    }

    if (desc.version != 1 || desc.first_entry == 0) {
      // Either unknown version, or no jit entries.
      return false;
    }

    // Check if there are extra Android-specific fields.
    if (memcmp(desc.magic, kMagic, sizeof(kMagic)) == 0) {
      jit_entry_size_ = kSizeOfCodeEntryV2;
      seqlock_offset_ = offsetof(JITCodeEntry, seqlock);
    } else {
      jit_entry_size_ = kSizeOfCodeEntryV1;
      seqlock_offset_ = 0;
    }
    descriptor_addr_ = addr;
    return true;
  }

  void ProcessArch() {}

  bool ReadVariableData(uint64_t ptr) { return ReadDescriptor(ptr); }

  // Invoke callback for all symfiles that contain the given PC.
  // Returns true if any callback returns true (which also aborts the iteration).
  template <typename Callback /* (Symfile*) -> bool */>
  bool ForEachSymfile(Maps* maps, uint64_t pc, Callback callback) {
    // Use a single lock, this object should be used so infrequently that
    // a fine grain lock is unnecessary.
    std::lock_guard<std::mutex> guard(lock_);
    if (descriptor_addr_ == 0) {
      FindAndReadVariable(maps, global_variable_name_);
      if (descriptor_addr_ == 0) {
        return false;
      }
    }

    // Try to find the entry in already loaded symbol files.
    for (auto& it : entries_) {
      Symfile* symfile = it.second.get();
      // Check seqlock to make sure that entry is still valid (it may be very old).
      if (symfile->IsValidPc(pc) && CheckSeqlock(it.first) && callback(symfile)) {
        return true;
      }
    }

    // Update all entries and retry.
    ReadAllEntries(maps);
    for (auto& it : entries_) {
      Symfile* symfile = it.second.get();
      // Note that the entry could become invalid since the ReadAllEntries above,
      // but that is ok.  We don't want to fail or refresh the entries yet again.
      // This is as if we found the entry in time and it became invalid after return.
      // This is relevant when ART moves/packs JIT entries. That is, the entry is
      // technically deleted, but only because it was copied into merged uber-entry.
      // So the JIT method is still alive and the deleted data is still correct.
      if (symfile->IsValidPc(pc) && callback(symfile)) {
        return true;
      }
    }

    return false;
  }

  bool GetFunctionName(Maps* maps, uint64_t pc, SharedString* name, uint64_t* offset) {
    // NB: If symfiles overlap in PC ranges, this will check all of them.
    return ForEachSymfile(maps, pc, [pc, name, offset](Symfile* file) {
      return file->GetFunctionName(pc, name, offset);
    });
  }

  Symfile* Find(Maps* maps, uint64_t pc) {
    // NB: If symfiles overlap in PC ranges (which can happen for both ELF and DEX),
    // this will check all of them and return one that also has a matching function.
    Symfile* result = nullptr;
    bool found = ForEachSymfile(maps, pc, [pc, &result](Symfile* file) {
      result = file;
      SharedString name;
      uint64_t offset;
      return file->GetFunctionName(pc, &name, &offset);
    });
    if (found) {
      return result;  // Found symfile with symbol that also matches the PC.
    }
    // There is no matching symbol, so return any symfile for which the PC is valid.
    // This is a useful fallback for tests, which often have symfiles with no functions.
    return result;
  }

  // Read all entries from the process and cache them locally.
  // The linked list might be concurrently modified. We detect races and retry.
  bool ReadAllEntries(Maps* maps) {
    for (int i = 0; i < kMaxRaceRetries; i++) {
      bool race = false;
      if (!ReadAllEntries(maps, &race)) {
        if (race) {
          continue;  // Retry due to concurrent modification of the linked list.
        }
        return false;  // Failed to read entries.
      }
      return true;  // Success.
    }
    return false;  // Too many retries.
  }

  // Read all JIT entries while assuming there might be concurrent modifications.
  // If there is a race, the method will fail and the caller should retry the call.
  bool ReadAllEntries(Maps* maps, bool* race) {
    // New entries might be added while we iterate over the linked list.
    // In particular, an entry could be effectively moved from end to start due to
    // the ART repacking algorithm, which groups smaller entries into a big one.
    // Therefore keep reading the most recent entries until we reach a fixed point.
    std::map<UID, std::shared_ptr<Symfile>> entries;
    for (size_t i = 0; i < kMaxHeadRetries; i++) {
      size_t old_size = entries.size();
      if (!ReadNewEntries(maps, &entries, race)) {
        return false;
      }
      if (entries.size() == old_size) {
        entries_.swap(entries);
        return true;
      }
    }
    return false;  // Too many retries.
  }

  // Read new JIT entries (head of linked list) until we find one that we have seen before.
  // This method uses seqlocks extensively to ensure safety in case of concurrent modifications.
  bool ReadNewEntries(Maps* maps, std::map<UID, std::shared_ptr<Symfile>>* entries, bool* race) {
    // Read the address of the head entry in the linked list.
    UID uid;
    if (!ReadNextField(descriptor_addr_ + offsetof(JITDescriptor, first_entry), &uid, race)) {
      return false;
    }

    // Follow the linked list.
    while (uid.address != 0) {
      // Check if we have reached an already cached entry (we restart from head repeatedly).
      if (entries->contains(uid)) {
        return true;
      }

      // Read the entry.
      JITCodeEntry data{};
      if (!memory_->ReadFully(uid.address, &data, jit_entry_size_)) {
        return false;
      }
      data.symfile_addr = StripAddressTag(data.symfile_addr);

      // Check the seqlock to verify the symfile_addr and symfile_size.
      if (!CheckSeqlock(uid, race)) {
        return false;
      }

      // Copy and load the symfile.
      auto it = entries_.find(uid);
      if (it != entries_.end()) {
        // The symfile was already loaded - just copy the reference.
        entries->emplace(uid, it->second);
      } else if (data.symfile_addr != 0) {
        std::shared_ptr<Symfile> symfile;
        bool ok = this->Load(maps, memory_, data.symfile_addr, data.symfile_size.value, symfile);
        // Check seqlock first because load can fail due to race (so we want to trigger retry).
        // TODO: Extract the memory copy code before the load, so that it is immune to races.
        if (!CheckSeqlock(uid, race)) {
          return false;  // The ELF/DEX data was removed before we loaded it.
        }
        // Exclude symbol files that fail to load (but continue loading other files).
        if (ok) {
          entries->emplace(uid, symfile);
        }
      }

      // Go to next entry.
      UID next_uid;
      if (!ReadNextField(uid.address + offsetof(JITCodeEntry, next), &next_uid, race)) {
        return false;  // The next pointer was modified while we were reading it.
      }
      if (!CheckSeqlock(uid, race)) {
        return false;  // This entry was deleted before we moved to the next one.
      }
      uid = next_uid;
    }

    return true;
  }

  // Read the address and seqlock of entry from the next field of linked list.
  // This is non-trivial since they need to be consistent (as if we read both atomically).
  //
  // We're reading pointers, which can point at heap-allocated structures (the
  // case for the __dex_debug_descriptor pointers at the time of writing).
  // On 64 bit systems, the target process might have top-byte heap pointer
  // tagging enabled, so we need to mask out the tag. We also know that the
  // address must point to userspace, so the top byte of the address must be
  // zero on both x64 and aarch64 without tagging. Therefore the masking can be
  // done unconditionally.
  bool ReadNextField(uint64_t next_field_addr, UID* uid, bool* race) {
    Uintptr_T address[2]{0, 0};
    uint32_t seqlock[2]{0, 0};
    // Read all data twice: address[0], seqlock[0], address[1], seqlock[1].
    for (int i = 0; i < 2; i++) {
      std::atomic_thread_fence(std::memory_order_acquire);
      if (!(memory_->ReadFully(next_field_addr, &address[i], sizeof(address[i])))) {
        return false;
      }
      address[i] = StripAddressTag(address[i]);
      if (seqlock_offset_ == 0) {
        // There is no seqlock field.
        *uid = UID{.address = address[0], .seqlock = 0};
        return true;
      }
      if (address[i] != 0) {
        std::atomic_thread_fence(std::memory_order_acquire);
        if (!memory_->ReadFully(address[i] + seqlock_offset_, &seqlock[i], sizeof(seqlock[i]))) {
          return false;
        }
      }
    }
    // Check that both reads returned identical values, and that the entry is live.
    if (address[0] != address[1] || seqlock[0] != seqlock[1] || (seqlock[0] & 1) == 1) {
      *race = true;
      return false;
    }
    // Since address[1] is sandwiched between two seqlock reads, we know that
    // at the time of address[1] read, the entry had the given seqlock value.
    *uid = UID{.address = address[1], .seqlock = seqlock[1]};
    return true;
  }

  // Check that the given entry has not been deleted (or replaced by new entry at same address).
  bool CheckSeqlock(UID uid, bool* race = nullptr) {
    if (seqlock_offset_ == 0) {
      // There is no seqlock field.
      return true;
    }
    // This is required for memory synchronization if the we are working with local memory.
    // For other types of memory (e.g. remote) this is no-op and has no significant effect.
    std::atomic_thread_fence(std::memory_order_acquire);
    uint32_t seen_seqlock;
    if (!memory_->Read32(uid.address + seqlock_offset_, &seen_seqlock)) {
      return false;
    }
    if (seen_seqlock != uid.seqlock) {
      if (race != nullptr) {
        *race = true;
      }
      return false;
    }
    return true;
  }

  // AArch64 has Address tagging (aka Top Byte Ignore) feature, which is used by
  // HWASAN and MTE to store metadata in the address. We need to remove the tag.
  Uintptr_T StripAddressTag(Uintptr_T addr) {
    if (arch() == ARCH_ARM64) {
      // Make the value signed so it will be sign extended if necessary.
      return static_cast<Uintptr_T>((static_cast<int64_t>(addr) << 8) >> 8);
    }
    return addr;
  }

 private:
  const char* global_variable_name_ = nullptr;
  uint64_t descriptor_addr_ = 0;  // Non-zero if we have found (non-empty) descriptor.
  uint32_t jit_entry_size_ = 0;
  uint32_t seqlock_offset_ = 0;
  std::map<UID, std::shared_ptr<Symfile>> entries_;  // Cached loaded entries.

  std::mutex lock_;
};

// uint64_t values on x86 are not naturally aligned,
// but uint64_t values on ARM are naturally aligned.
struct Uint64_P {
  uint64_t value;
} __attribute__((packed));
struct Uint64_A {
  uint64_t value;
} __attribute__((aligned(8)));

template <typename Symfile>
std::unique_ptr<GlobalDebugInterface<Symfile>> CreateGlobalDebugImpl(
    ArchEnum arch, std::shared_ptr<Memory>& memory, std::vector<std::string> search_libs,
    const char* global_variable_name) {
  CHECK(arch != ARCH_UNKNOWN);

  // The interface needs to see real-time changes in memory for synchronization with the
  // concurrently running ART JIT compiler. Skip caching and read the memory directly.
  std::shared_ptr<Memory> jit_memory;
  MemoryCacheBase* cached_memory = memory->AsMemoryCacheBase();
  if (cached_memory != nullptr) {
    jit_memory = cached_memory->UnderlyingMemory();
  } else {
    jit_memory = memory;
  }

  switch (arch) {
    case ARCH_X86: {
      using Impl = GlobalDebugImpl<Symfile, uint32_t, Uint64_P>;
      static_assert(offsetof(typename Impl::JITCodeEntry, symfile_size) == 12, "layout");
      static_assert(offsetof(typename Impl::JITCodeEntry, seqlock) == 28, "layout");
      static_assert(sizeof(typename Impl::JITCodeEntry) == 32, "layout");
      static_assert(sizeof(typename Impl::JITDescriptor) == 48, "layout");
      return std::make_unique<Impl>(arch, jit_memory, search_libs, global_variable_name);
    }
    case ARCH_ARM: {
      using Impl = GlobalDebugImpl<Symfile, uint32_t, Uint64_A>;
      static_assert(offsetof(typename Impl::JITCodeEntry, symfile_size) == 16, "layout");
      static_assert(offsetof(typename Impl::JITCodeEntry, seqlock) == 32, "layout");
      static_assert(sizeof(typename Impl::JITCodeEntry) == 40, "layout");
      static_assert(sizeof(typename Impl::JITDescriptor) == 48, "layout");
      return std::make_unique<Impl>(arch, jit_memory, search_libs, global_variable_name);
    }
    case ARCH_ARM64:
    case ARCH_X86_64:
    case ARCH_RISCV64: {
      using Impl = GlobalDebugImpl<Symfile, uint64_t, Uint64_A>;
      static_assert(offsetof(typename Impl::JITCodeEntry, symfile_size) == 24, "layout");
      static_assert(offsetof(typename Impl::JITCodeEntry, seqlock) == 40, "layout");
      static_assert(sizeof(typename Impl::JITCodeEntry) == 48, "layout");
      static_assert(sizeof(typename Impl::JITDescriptor) == 56, "layout");
      return std::make_unique<Impl>(arch, jit_memory, search_libs, global_variable_name);
    }
    default:
      abort();
  }
}

}  // namespace unwindstack
