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

#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>

#include <android-base/unique_fd.h>
#include <art_api/dex_file_support.h>

#include <unwindstack/Log.h>
#include <unwindstack/MapInfo.h>
#include <unwindstack/Memory.h>

#include "DexFile.h"
#include "MemoryBuffer.h"

namespace unwindstack {

static bool CheckDexSupport() {
  if (std::string err_msg; !art_api::dex::TryLoadLibdexfile(&err_msg)) {
    Log::Error("Failed to initialize DEX file support: %s", err_msg.c_str());
    return false;
  }
  return true;
}

std::shared_ptr<DexFile> DexFile::CreateFromDisk(uint64_t addr, uint64_t size, MapInfo* map) {
  static auto& mapped_dex_files = *new std::map<MappedFileKey, std::weak_ptr<DexFileApi>>;
  static auto& lock = *new std::mutex;  // Guards the static cache above.

  if (map == nullptr || map->name().empty()) {
    return nullptr;  // MapInfo not backed by file.
  }
  if (!(map->start() <= addr && addr < map->end())) {
    return nullptr;  // addr is not in MapInfo range.
  }
  if (size > (map->end() - addr)) {
    return nullptr;  // size is past the MapInfo end.
  }
  uint64_t offset_in_file = (addr - map->start()) + map->offset();

  // Fast-path: Check if the dex file was already mapped from disk.
  std::lock_guard<std::mutex> guard(lock);
  MappedFileKey cache_key(map->name(), offset_in_file, size);
  std::weak_ptr<DexFileApi>& cache_entry = mapped_dex_files[cache_key];
  std::shared_ptr<DexFileApi> dex_api = cache_entry.lock();
  if (dex_api != nullptr) {
    return std::shared_ptr<DexFile>(new DexFile(addr, size, std::move(dex_api)));
  }

  // Load the file from disk and cache it.
  auto memory = Memory::CreateFileMemory(map->name(), offset_in_file, size);
  if (memory == nullptr) {
    return nullptr;  // failed to map the file.
  }
  std::unique_ptr<art_api::dex::DexFile> dex;
  art_api::dex::DexFile::Create(memory->GetPtr(), size, nullptr, map->name().c_str(), &dex);
  if (dex == nullptr) {
    return nullptr;  // invalid DEX file.
  }
  dex_api.reset(new DexFileApi{std::move(dex), memory, std::mutex()});
  cache_entry = dex_api;
  return std::shared_ptr<DexFile>(new DexFile(addr, size, std::move(dex_api)));
}

std::shared_ptr<DexFile> DexFile::Create(uint64_t base_addr, uint64_t file_size, Memory* memory,
                                         MapInfo* info) {
  static bool has_dex_support = CheckDexSupport();
  if (!has_dex_support || file_size == 0) {
    return nullptr;
  }

  // Do not try to open the DEX file if the file name ends with "(deleted)". It does not exist.
  // This happens when an app is background-optimized by ART and all of its files are replaced.
  // Furthermore, do NOT try to fallback to in-memory copy. It would work, but all apps tend to
  // be background-optimized at the same time, so it would lead to excessive memory use during
  // system-wide profiling (essentially copying all dex files for all apps: hundreds of MBs).
  // This will cause missing symbols in the backtrace, however, that outcome is inevitable
  // anyway, since we can not obtain mini-debug-info for the deleted .oat files.
  const std::string_view filename(info != nullptr ? info->name() : "");
  const std::string_view kDeleted("(deleted)");
  if (filename.size() >= kDeleted.size() &&
      filename.substr(filename.size() - kDeleted.size()) == kDeleted) {
    return nullptr;
  }

  std::shared_ptr<DexFile> dex_file = CreateFromDisk(base_addr, file_size, info);
  if (dex_file != nullptr) {
    return dex_file;
  }

  // Fallback: make copy in local buffer.
  std::unique_ptr<MemoryBuffer> copy(new MemoryBuffer(file_size));
  uint8_t* dst_ptr = copy->GetPtr(0);
  if (dst_ptr == nullptr || !memory->ReadFully(base_addr, dst_ptr, file_size)) {
    return nullptr;
  }
  std::unique_ptr<art_api::dex::DexFile> dex;
  art_api::dex::DexFile::Create(copy->GetPtr(0), file_size, nullptr, "", &dex);
  if (dex == nullptr) {
    return nullptr;
  }
  std::shared_ptr<DexFileApi> api(new DexFileApi{std::move(dex), std::move(copy), std::mutex()});
  return std::shared_ptr<DexFile>(new DexFile(base_addr, file_size, std::move(api)));
}

bool DexFile::GetFunctionName(uint64_t dex_pc, SharedString* method_name, uint64_t* method_offset) {
  uint64_t dex_offset = dex_pc - base_addr_;  // Convert absolute PC to file-relative offset.

  // Lookup the function in the cache.
  std::lock_guard<std::mutex> guard(dex_api_->lock_);  // Protect both the symbols and the C API.
  auto it = symbols_.upper_bound(dex_offset);
  if (it == symbols_.end() || dex_offset < it->second.offset) {
    // Lookup the function in the underlying dex file.
    size_t found = dex_api_->dex_->FindMethodAtOffset(dex_offset, [&](const auto& method) {
      size_t code_size, name_size;
      uint32_t offset = method.GetCodeOffset(&code_size);
      const char* name = method.GetQualifiedName(/*with_params=*/false, &name_size);
      it = symbols_.emplace(offset + code_size, Info{offset, std::string(name, name_size)}).first;
    });
    if (found == 0) {
      return false;
    }
  }

  // Return the found function.
  *method_offset = dex_offset - it->second.offset;
  *method_name = it->second.name;
  return true;
}

}  // namespace unwindstack
