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

#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <mutex>
#include <string>

#include <android-base/strings.h>

#include <unwindstack/Elf.h>
#include <unwindstack/MapInfo.h>
#include <unwindstack/Maps.h>
#include <unwindstack/Memory.h>

#include "MemoryFileAtOffset.h"
#include "MemoryRange.h"

namespace unwindstack {

bool MapInfo::ElfFileNotReadable() {
  const std::string& map_name = name();
  return memory_backed_elf() && !map_name.empty() && map_name[0] != '[' &&
         !android::base::StartsWith(map_name, "/memfd:");
}

std::shared_ptr<MapInfo> MapInfo::GetPrevRealMap() {
  if (name().empty()) {
    return nullptr;
  }

  for (auto prev = prev_map(); prev != nullptr; prev = prev->prev_map()) {
    if (!prev->IsBlank()) {
      if (prev->name() == name()) {
        return prev;
      }
      return nullptr;
    }
  }
  return nullptr;
}

std::shared_ptr<MapInfo> MapInfo::GetNextRealMap() {
  if (name().empty()) {
    return nullptr;
  }

  for (auto next = next_map(); next != nullptr; next = next->next_map()) {
    if (!next->IsBlank()) {
      if (next->name() == name()) {
        return next;
      }
      return nullptr;
    }
  }
  return nullptr;
}

bool MapInfo::InitFileMemoryFromPreviousReadOnlyMap(MemoryFileAtOffset* memory) {
  // One last attempt, see if the previous map is read-only with the
  // same name and stretches across this map.
  auto prev_real_map = GetPrevRealMap();
  if (prev_real_map == nullptr || prev_real_map->flags() != PROT_READ ||
      prev_real_map->offset() >= offset()) {
    return false;
  }

  uint64_t map_size = end() - prev_real_map->end();
  if (!memory->Init(name(), prev_real_map->offset(), map_size)) {
    return false;
  }

  uint64_t max_size;
  if (!Elf::GetInfo(memory, &max_size) || max_size < map_size) {
    return false;
  }

  if (!memory->Init(name(), prev_real_map->offset(), max_size)) {
    return false;
  }

  set_elf_offset(offset() - prev_real_map->offset());
  set_elf_start_offset(prev_real_map->offset());
  return true;
}

std::shared_ptr<Memory> MapInfo::CreateFileMemory() {
  // Fail on device maps.
  if (flags() & MAPS_FLAGS_DEVICE_MAP) {
    return nullptr;
  }

  auto file_memory = std::make_shared<MemoryFileAtOffset>();
  if (offset() == 0) {
    if (file_memory->Init(name(), 0)) {
      return file_memory;
    }
    return nullptr;
  }

  // These are the possibilities when the offset is non-zero.
  // - There is an elf file embedded in a file, and the offset is the
  //   the start of the elf in the file.
  // - There is an elf file embedded in a file, and the offset is the
  //   the start of the executable part of the file. The actual start
  //   of the elf is in the read-only segment preceeding this map.
  // - The whole file is an elf file, and the offset needs to be saved.
  //
  // Map in just the part of the file for the map. If this is not
  // a valid elf, then reinit as if the whole file is an elf file.
  // If the offset is a valid elf, then determine the size of the map
  // and reinit to that size. This is needed because the dynamic linker
  // only maps in a portion of the original elf, and never the symbol
  // file data.
  //
  // For maps with MAPS_FLAGS_JIT_SYMFILE_MAP, the map range is for a JIT function,
  // which can be smaller than elf header size. So make sure map_size is large enough
  // to read elf header.
  uint64_t map_size = std::max<uint64_t>(end() - start(), sizeof(ElfTypes64::Ehdr));
  if (!file_memory->Init(name(), offset(), map_size)) {
    return nullptr;
  }

  // Check if the start of this map is an embedded elf.
  uint64_t max_size = 0;
  if (Elf::GetInfo(file_memory.get(), &max_size)) {
    set_elf_start_offset(offset());
    if (max_size > map_size) {
      if (file_memory->Init(name(), offset(), max_size)) {
        return file_memory;
      }
      // Try to reinit using the default map_size.
      if (file_memory->Init(name(), offset(), map_size)) {
        return file_memory;
      }
      set_elf_start_offset(0);
      return nullptr;
    }
    return file_memory;
  }

  // No elf at offset, try to init as if the whole file is an elf.
  if (file_memory->Init(name(), 0) && Elf::IsValidElf(file_memory.get())) {
    set_elf_offset(offset());
    return file_memory;
  }

  // See if the map previous to this one contains a read-only map
  // that represents the real start of the elf data.
  if (InitFileMemoryFromPreviousReadOnlyMap(file_memory.get())) {
    return file_memory;
  }

  // Failed to find elf at start of file or at read-only map, return
  // file object from the current map.
  if (file_memory->Init(name(), offset(), map_size)) {
    return file_memory;
  }
  return nullptr;
}

std::shared_ptr<Memory> MapInfo::CreateMemory(const std::shared_ptr<Memory>& process_memory) {
  if (end() <= start()) {
    return nullptr;
  }

  set_elf_offset(0);

  // Fail on device maps.
  if (flags() & MAPS_FLAGS_DEVICE_MAP) {
    return nullptr;
  }

  // First try and use the file associated with the info.
  if (!name().empty()) {
    auto memory = CreateFileMemory();
    if (memory != nullptr) {
      return memory;
    }
  }

  if (process_memory == nullptr) {
    return nullptr;
  }

  set_memory_backed_elf(true);

  // Need to verify that this elf is valid. It's possible that
  // only part of the elf file to be mapped into memory is in the executable
  // map. In this case, there will be another read-only map that includes the
  // first part of the elf file. This is done if the linker rosegment
  // option is used.
  std::shared_ptr<Memory> memory_range(
      new MemoryRange(process_memory, start(), end() - start(), 0));
  if (Elf::IsValidElf(memory_range.get())) {
    set_elf_start_offset(offset());

    auto next_real_map = GetNextRealMap();

    // Might need to peek at the next map to create a memory object that
    // includes that map too.
    if (offset() != 0 || next_real_map == nullptr || offset() >= next_real_map->offset()) {
      return memory_range;
    }

    // There is a possibility that the elf object has already been created
    // in the next map. Since this should be a very uncommon path, just
    // redo the work. If this happens, the elf for this map will eventually
    // be discarded.
    MemoryRanges* ranges = new MemoryRanges;
    std::shared_ptr<Memory> memory_ranges(ranges);
    ranges->Insert(new MemoryRange(process_memory, start(), end() - start(), 0));
    ranges->Insert(new MemoryRange(process_memory, next_real_map->start(),
                                   next_real_map->end() - next_real_map->start(),
                                   next_real_map->offset() - offset()));

    return memory_ranges;
  }

  auto prev_real_map = GetPrevRealMap();

  // Find the read-only map by looking at the previous map. The linker
  // doesn't guarantee that this invariant will always be true. However,
  // if that changes, there is likely something else that will change and
  // break something.
  if (offset() == 0 || prev_real_map == nullptr || prev_real_map->offset() >= offset()) {
    set_memory_backed_elf(false);
    return nullptr;
  }

  // Make sure that relative pc values are corrected properly.
  set_elf_offset(offset() - prev_real_map->offset());
  // Use this as the elf start offset, otherwise, you always get offsets into
  // the r-x section, which is not quite the right information.
  set_elf_start_offset(prev_real_map->offset());

  MemoryRanges* ranges = new MemoryRanges;
  std::shared_ptr<Memory> memory_ranges(ranges);
  if (!ranges->Insert(new MemoryRange(process_memory, prev_real_map->start(),
                                      prev_real_map->end() - prev_real_map->start(), 0))) {
    return nullptr;
  }
  if (!ranges->Insert(new MemoryRange(process_memory, start(), end() - start(), elf_offset()))) {
    return nullptr;
  }
  return memory_ranges;
}

class ScopedElfCacheLock {
 public:
  ScopedElfCacheLock() {
    if (Elf::CachingEnabled()) Elf::CacheLock();
  }
  ~ScopedElfCacheLock() {
    if (Elf::CachingEnabled()) Elf::CacheUnlock();
  }
};

Elf* MapInfo::GetElf(const std::shared_ptr<Memory>& process_memory, ArchEnum expected_arch) {
  // Make sure no other thread is trying to add the elf to this map.
  std::lock_guard<std::mutex> guard(elf_mutex());

  if (elf().get() != nullptr) {
    return elf().get();
  }

  ScopedElfCacheLock elf_cache_lock;
  if (Elf::CachingEnabled() && !name().empty()) {
    if (Elf::CacheGet(this)) {
      return elf().get();
    }
  }

  auto elf_memory = CreateMemory(process_memory);
  elf().reset(new Elf(elf_memory));
  // If the init fails, keep the elf around as an invalid object so we
  // don't try to reinit the object.
  elf()->Init();
  if (elf()->valid() && expected_arch != elf()->arch()) {
    // Make the elf invalid, mismatch between arch and expected arch.
    elf()->Invalidate();
  }

  if (!elf()->valid()) {
    set_elf_start_offset(offset());
  } else if (auto prev_real_map = GetPrevRealMap(); prev_real_map != nullptr &&
                                                    prev_real_map->flags() == PROT_READ &&
                                                    prev_real_map->offset() < offset()) {
    // If there is a read-only map then a read-execute map that represents the
    // same elf object, make sure the previous map is using the same elf
    // object if it hasn't already been set. Locking this should not result
    // in a deadlock as long as the invariant that the code only ever tries
    // to lock the previous real map holds true.
    std::lock_guard<std::mutex> guard(prev_real_map->elf_mutex());
    if (prev_real_map->elf() == nullptr) {
      // Need to verify if the map is the previous read-only map.
      prev_real_map->set_elf(elf());
      prev_real_map->set_memory_backed_elf(memory_backed_elf());
      prev_real_map->set_elf_start_offset(elf_start_offset());
      prev_real_map->set_elf_offset(prev_real_map->offset() - elf_start_offset());
    } else if (prev_real_map->elf_start_offset() == elf_start_offset()) {
      // Discard this elf, and use the elf from the previous map instead.
      set_elf(prev_real_map->elf());
    }
  }

  // Cache the elf only after all of the above checks since we might
  // discard the original elf we created.
  if (Elf::CachingEnabled()) {
    Elf::CacheAdd(this);
  }
  return elf().get();
}

bool MapInfo::GetFunctionName(uint64_t addr, SharedString* name, uint64_t* func_offset) {
  {
    // Make sure no other thread is trying to update this elf object.
    std::lock_guard<std::mutex> guard(elf_mutex());
    if (elf() == nullptr) {
      return false;
    }
  }
  // No longer need the lock, once the elf object is created, it is not deleted
  // until this object is deleted.
  return elf()->GetFunctionName(addr, name, func_offset);
}

uint64_t MapInfo::GetLoadBias() {
  uint64_t cur_load_bias = load_bias().load();
  if (cur_load_bias != UINT64_MAX) {
    return cur_load_bias;
  }

  Elf* elf_obj = GetElfObj();
  if (elf_obj == nullptr) {
    return UINT64_MAX;
  }

  if (elf_obj->valid()) {
    cur_load_bias = elf_obj->GetLoadBias();
    set_load_bias(cur_load_bias);
    return cur_load_bias;
  }

  set_load_bias(0);
  return 0;
}

uint64_t MapInfo::GetLoadBias(const std::shared_ptr<Memory>& process_memory) {
  uint64_t cur_load_bias = GetLoadBias();
  if (cur_load_bias != UINT64_MAX) {
    return cur_load_bias;
  }

  // Call lightweight static function that will only read enough of the
  // elf data to get the load bias.
  auto memory = CreateMemory(process_memory);
  cur_load_bias = Elf::GetLoadBias(memory.get());
  set_load_bias(cur_load_bias);
  return cur_load_bias;
}

MapInfo::~MapInfo() {
  ElfFields* elf_fields = elf_fields_.load();
  if (elf_fields != nullptr) {
    delete elf_fields->build_id_.load();
    delete elf_fields;
  }
}

std::string MapInfo::GetFullName() {
  Elf* elf_obj = GetElfObj();
  if (elf_obj == nullptr || elf_start_offset() == 0 || name().empty()) {
    return name();
  }

  std::string soname = elf_obj->GetSoname();
  if (soname.empty()) {
    return name();
  }

  std::string full_name(name());
  full_name += '!';
  full_name += soname;
  return full_name;
}

SharedString MapInfo::GetBuildID() {
  SharedString* id = build_id().load();
  if (id != nullptr) {
    return *id;
  }

  // No need to lock, at worst if multiple threads do this at the same
  // time it should be detected and only one thread should win and
  // save the data.

  std::string result;
  Elf* elf_obj = GetElfObj();
  if (elf_obj != nullptr) {
    result = elf_obj->GetBuildID();
  } else {
    // This will only work if we can get the file associated with this memory.
    // If this is only available in memory, then the section name information
    // is not present and we will not be able to find the build id info.
    auto file_memory = CreateFileMemory();
    if (file_memory != nullptr) {
      result = Elf::GetBuildID(file_memory.get());
    }
  }
  return SetBuildID(std::move(result));
}

SharedString MapInfo::SetBuildID(std::string&& new_build_id) {
  std::unique_ptr<SharedString> new_build_id_ptr(new SharedString(std::move(new_build_id)));
  SharedString* expected_id = nullptr;
  // Strong version since we need to reliably return the stored pointer.
  if (build_id().compare_exchange_strong(expected_id, new_build_id_ptr.get())) {
    // Value saved, so make sure the memory is not freed.
    return *new_build_id_ptr.release();
  } else {
    // The expected value is set to the stored value on failure.
    return *expected_id;
  }
}

MapInfo::ElfFields& MapInfo::GetElfFields() {
  ElfFields* elf_fields = elf_fields_.load(std::memory_order_acquire);
  if (elf_fields != nullptr) {
    return *elf_fields;
  }
  // Allocate and initialize the field in thread-safe way.
  std::unique_ptr<ElfFields> desired(new ElfFields());
  ElfFields* expected = nullptr;
  // Strong version is reliable. Weak version might randomly return false.
  if (elf_fields_.compare_exchange_strong(expected, desired.get())) {
    return *desired.release();  // Success: we transferred the pointer ownership to the field.
  } else {
    return *expected;  // Failure: 'expected' is updated to the value set by the other thread.
  }
}

std::string MapInfo::GetPrintableBuildID() {
  std::string raw_build_id = GetBuildID();
  return Elf::GetPrintableBuildID(raw_build_id);
}

}  // namespace unwindstack
