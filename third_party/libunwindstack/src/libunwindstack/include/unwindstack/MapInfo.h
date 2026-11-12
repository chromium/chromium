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

#pragma once

#include <stdint.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#include <unwindstack/Elf.h>
#include <unwindstack/SharedString.h>

namespace unwindstack {

class MemoryFileAtOffset;

// Represents virtual memory map (as obtained from /proc/*/maps).
//
// Note that we have to be surprisingly careful with memory usage here,
// since in system-wide profiling this data can take considerable space.
// (for example, 400 process * 400 maps * 128 bytes = 20 MB + string data).
class MapInfo {
 public:
  MapInfo(std::shared_ptr<MapInfo>& prev_map, uint64_t start, uint64_t end, uint64_t offset,
          uint64_t flags, SharedString name)
      : start_(start),
        end_(end),
        offset_(offset),
        flags_(static_cast<uint16_t>(flags)),
        name_(name),
        elf_fields_(nullptr),
        prev_map_(prev_map) {}
  MapInfo(uint64_t start, uint64_t end, uint64_t offset, uint64_t flags, SharedString name)
      : start_(start),
        end_(end),
        offset_(offset),
        flags_(static_cast<uint16_t>(flags)),
        name_(name),
        elf_fields_(nullptr) {}

  static inline std::shared_ptr<MapInfo> Create(std::shared_ptr<MapInfo>& prev_map,
                                                uint64_t start, uint64_t end, uint64_t offset,
                                                uint64_t flags, SharedString name) {
    auto map_info = std::make_shared<MapInfo>(prev_map, start, end, offset, flags, name);
    if (prev_map) {
      prev_map->next_map_ = map_info;
    }
    return map_info;
  }
  static inline std::shared_ptr<MapInfo> Create(uint64_t start, uint64_t end, uint64_t offset,
                                                uint64_t flags, SharedString name) {
    return std::make_shared<MapInfo>(start, end, offset, flags, name);
  }

  ~MapInfo();

  // Cached data for mapped ELF files.
  // We allocate this structure lazily since there are much fewer ELFs than maps.
  struct ElfFields {
    ElfFields() : load_bias_(UINT64_MAX), build_id_(0) {}

    std::shared_ptr<Elf> elf_;
    // The offset of the beginning of this mapping to the beginning of the
    // ELF file.
    // elf_offset == offset - elf_start_offset.
    // This value is only non-zero if the offset is non-zero but there is
    // no elf signature found at that offset.
    uint64_t elf_offset_ = 0;
    // This value is the offset into the file of the map in memory that is the
    // start of the elf. This is not equal to offset when the linker splits
    // shared libraries into a read-only and read-execute map.
    uint64_t elf_start_offset_ = 0;

    std::atomic_uint64_t load_bias_;

    // This is a pointer to a new'd std::string.
    // Using an atomic value means that we don't need to lock and will
    // make it easier to move to a fine grained lock in the future.
    std::atomic<SharedString*> build_id_;

    // Set to true if the elf file data is coming from memory.
    bool memory_backed_elf_ = false;

    // Protect the creation of the elf object.
    std::mutex elf_mutex_;
  };

  // True if the file named by this map is not actually readable and the
  // elf is using the data in memory.
  bool ElfFileNotReadable();

  // This is the previous map with the same name that is not empty and with
  // a 0 offset. For example, this set of maps:
  //  1000-2000  r--p 000000 00:00 0 libc.so
  //  2000-3000  ---p 000000 00:00 0
  //  3000-4000  r-xp 003000 00:00 0 libc.so
  // The last map's prev_map would point to the 2000-3000 map, while
  // GetPrevRealMap() would point to the 1000-2000 map.
  // NOTE: If a map is encountered that has a non-zero offset, or has a
  //       a name different from the current map, then GetPrevRealMap()
  //       returns nullptr.
  std::shared_ptr<MapInfo> GetPrevRealMap();
  // This is the next map with the same name that is not empty and with
  // a 0 offset. For the example above, the first map's GetNextRealMap()
  // would be the 3000-4000 map.
  // NOTE: If a map is encountered that has a non-zero offset, or has a
  //       a name different from the current map, then GetNextRealMap()
  //       returns nullptr.
  std::shared_ptr<MapInfo> GetNextRealMap();

  // This is guaranteed to give out the Elf object associated with the
  // object. The invariant is that once the Elf object is set under the
  // lock in a MapInfo object it never changes and is not freed until
  // the MapInfo object is destructed.
  inline Elf* GetElfObj() {
    std::lock_guard<std::mutex> guard(elf_mutex());
    return elf().get();
  }

  inline uint64_t start() const { return start_; }
  inline void set_start(uint64_t value) { start_ = value; }

  inline uint64_t end() const { return end_; }
  inline void set_end(uint64_t value) { end_ = value; }

  inline uint64_t offset() const { return offset_; }
  inline void set_offset(uint64_t value) { offset_ = value; }

  inline uint16_t flags() const { return flags_; }
  inline void set_flags(uint16_t value) { flags_ = value; }

  inline SharedString& name() { return name_; }
  inline void set_name(SharedString& value) { name_ = value; }
  inline void set_name(const char* value) { name_ = value; }

  inline std::shared_ptr<Elf>& elf() { return GetElfFields().elf_; }
  inline void set_elf(std::shared_ptr<Elf>& value) { GetElfFields().elf_ = value; }
  inline void set_elf(Elf* value) { GetElfFields().elf_.reset(value); }

  inline uint64_t elf_offset() { return GetElfFields().elf_offset_; }
  inline void set_elf_offset(uint64_t value) { GetElfFields().elf_offset_ = value; }

  inline uint64_t elf_start_offset() { return GetElfFields().elf_start_offset_; }
  inline void set_elf_start_offset(uint64_t value) { GetElfFields().elf_start_offset_ = value; }

  inline std::atomic_uint64_t& load_bias() { return GetElfFields().load_bias_; }
  inline void set_load_bias(uint64_t value) { GetElfFields().load_bias_ = value; }

  inline std::atomic<SharedString*>& build_id() { return GetElfFields().build_id_; }
  inline void set_build_id(SharedString* value) { GetElfFields().build_id_ = value; }

  inline bool memory_backed_elf() { return GetElfFields().memory_backed_elf_; }
  inline void set_memory_backed_elf(bool value) { GetElfFields().memory_backed_elf_ = value; }

  inline std::shared_ptr<MapInfo> prev_map() const { return prev_map_.lock(); }
  inline void set_prev_map(std::shared_ptr<MapInfo>& value) { prev_map_ = value; }

  inline std::shared_ptr<MapInfo> next_map() const { return next_map_.lock(); }
  inline void set_next_map(std::shared_ptr<MapInfo>& value) { next_map_ = value; }

  // This function guarantees it will never return nullptr.
  Elf* GetElf(const std::shared_ptr<Memory>& process_memory, ArchEnum expected_arch);

  // Guaranteed to return the proper value if GetElf() has been called.
  uint64_t GetLoadBias();

  // Will get the proper value even if GetElf() hasn't been called.
  uint64_t GetLoadBias(const std::shared_ptr<Memory>& process_memory);

  // This returns the name of the map plus the soname if this particular
  // map represents an elf file that is contained inside of another file.
  // The format of this soname embedded name is:
  //   file.apk!libutils.so
  // Otherwise, this function only returns the name of the map.
  std::string GetFullName();

  std::shared_ptr<Memory> CreateMemory(const std::shared_ptr<Memory>& process_memory);

  bool GetFunctionName(uint64_t addr, SharedString* name, uint64_t* func_offset);

  // Returns the raw build id read from the elf data.
  SharedString GetBuildID();

  // Used internally, and by tests. It sets the value only if it was not already set.
  SharedString SetBuildID(std::string&& new_build_id);

  // Returns the printable version of the build id (hex dump of raw data).
  std::string GetPrintableBuildID();

  inline bool IsBlank() { return offset() == 0 && flags() == 0 && name().empty(); }

  // Returns elf_fields_. It will create the object if it is null.
  ElfFields& GetElfFields();

 private:
  MapInfo(const MapInfo&) = delete;
  void operator=(const MapInfo&) = delete;

  std::shared_ptr<Memory> CreateFileMemory();
  bool InitFileMemoryFromPreviousReadOnlyMap(MemoryFileAtOffset* memory);

  // Protect the creation of the elf object.
  std::mutex& elf_mutex() { return GetElfFields().elf_mutex_; }

  uint64_t start_ = 0;
  uint64_t end_ = 0;
  uint64_t offset_ = 0;
  uint16_t flags_ = 0;
  SharedString name_;

  std::atomic<ElfFields*> elf_fields_;

  std::weak_ptr<MapInfo> prev_map_;
  std::weak_ptr<MapInfo> next_map_;
};

}  // namespace unwindstack
