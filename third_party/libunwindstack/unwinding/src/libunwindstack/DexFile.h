/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <unwindstack/SharedString.h>

#include <art_api/dex_file_support.h>

namespace unwindstack {

class MapInfo;
class Memory;

class DexFile {
  struct Info {
    uint32_t offset;  // Symbol start offset (relative to start of dex file).
    SharedString name;
  };

 public:
  bool IsValidPc(uint64_t dex_pc) {
    return base_addr_ <= dex_pc && (dex_pc - base_addr_) < file_size_;
  }

  bool GetFunctionName(uint64_t dex_pc, SharedString* method_name, uint64_t* method_offset);

  static std::shared_ptr<DexFile> Create(uint64_t base_addr, uint64_t file_size, Memory* memory,
                                         MapInfo* info);

 private:
  // The underlying C API. It might be shared by multiple DexFiles (with different base_addr).
  struct DexFileApi {
    std::unique_ptr<art_api::dex::DexFile> dex_;
    std::shared_ptr<Memory> memory_;  // Keep alive the memory object backing the dex file data.
    std::mutex lock_;                 // The C API is not thread-safe so we need to lock it.
  };

  static std::shared_ptr<DexFile> CreateFromDisk(uint64_t addr, uint64_t size, MapInfo* map);

  DexFile(uint64_t base_addr, uint64_t file_size, std::shared_ptr<DexFileApi>&& dex_api)
      : base_addr_(base_addr), file_size_(file_size), dex_api_(std::move(dex_api)) {}

  uint64_t base_addr_ = 0;               // Absolute address where this DEX file is in memory.
  uint64_t file_size_ = 0;               // Total number of bytes in the dex file.
  std::shared_ptr<DexFileApi> dex_api_;  // Loaded underling dex object.

  std::map<uint32_t, Info> symbols_;  // Cache of read symbols (keyed by *end* offset).

  // The same file can be mapped many times in system-wide profiling (once per process).
  // Furthermore, the ART side of the API will create expensive PC lookup table for it.
  // Therefore, we maintain cache to avoid loading the same file (sub-range) many times.
  // The cache is weak: It will not keep DexFiles alive (the weak_ptr will become null).
  using MappedFileKey = std::tuple<std::string, uint64_t, uint64_t>;  // (path, offset, size).
};

}  // namespace unwindstack
