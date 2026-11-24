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

#include <elf.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include <unwindstack/Memory.h>

#include "Check.h"
#include "Symbols.h"

namespace unwindstack {

Symbols::Symbols(uint64_t offset, uint64_t size, uint64_t entry_size, uint64_t str_offset,
                 uint64_t str_size)
    : offset_(offset),
      count_(entry_size != 0 ? ((size / entry_size > kMaxSymbols) ? kMaxSymbols : size / entry_size)
                             : 0),
      entry_size_(entry_size),
      str_offset_(str_offset) {
  if (__builtin_add_overflow(str_offset_, str_size, &str_end_)) {
    // Set to the max so that the code will still try to get symbol names.
    // Any reads that might be invalid will simply return no data, so
    // this will not result in crashes.
    // The assumption is that this value might have been corrupted, but
    // enough of the elf data is valid such that the code can still
    // get symbol information.
    str_end_ = UINT64_MAX;
  }
}

template <typename SymType>
static bool IsFunc(const SymType* entry) {
  return entry->st_shndx != SHN_UNDEF && ELF32_ST_TYPE(entry->st_info) == STT_FUNC;
}

// Binary search the symbol table to find function containing the given address.
// Without remap, the symbol table is assumed to be sorted and accessed directly.
// If the symbol table is not sorted this method might fail but should not crash.
// When the indices are remapped, they are guaranteed to be sorted by address.
template <typename SymType, bool RemapIndices>
Symbols::Info* Symbols::BinarySearch(uint64_t addr, Memory* elf_memory, uint64_t* func_offset) {
  // Fast-path: Check if the symbol has been already read from memory.
  // Otherwise use the cache iterator to constrain the binary search range.
  // (the symbol must be in the gap between this and the previous iterator)
  auto it = symbols_.upper_bound(addr);
  if (it != symbols_.end()) {
    uint64_t sym_value = (it->first - it->second.size);  // Function address.
    if (sym_value <= addr) {
      *func_offset = addr - sym_value;
      return &it->second;
    }
  }
  uint32_t count = RemapIndices ? remap_->size() : count_;
  uint32_t last = (it != symbols_.end()) ? it->second.index : count;
  uint32_t first = (it != symbols_.begin()) ? std::prev(it)->second.index + 1 : 0;

  while (first < last) {
    uint32_t current = first + (last - first) / 2;
    uint32_t symbol_index = RemapIndices ? remap_.value()[current] : current;
    uint64_t offset = symbol_index * entry_size_;
    if (__builtin_add_overflow(offset, offset_, &offset)) {
      // The elf data might be malformed.
      return nullptr;
    }
    SymType sym;
    if (!elf_memory->ReadFully(offset, &sym, sizeof(sym))) {
      return nullptr;
    }
    // There shouldn't be multiple symbols with same end address, but in case there are,
    // overwrite the cache with the last entry, so that 'sym' and 'info' are consistent.
    Info& info = symbols_[sym.st_value + sym.st_size];
    info = {.size = static_cast<uint32_t>(sym.st_size), .index = current};
    if (addr < sym.st_value) {
      last = current;
    } else if (addr < sym.st_value + sym.st_size) {
      *func_offset = addr - sym.st_value;
      return &info;
    } else {
      first = current + 1;
    }
  }
  return nullptr;
}

// Create remapping table which allows us to access symbols as if they were sorted by address.
template <typename SymType>
void Symbols::BuildRemapTable(Memory* elf_memory) {
  std::vector<uint64_t> addrs;  // Addresses of all symbols (addrs[i] == symbols[i].st_value).
  addrs.reserve(count_);
  remap_.emplace();  // Construct the optional remap table.
  remap_->reserve(count_);
  for (size_t symbol_idx = 0; symbol_idx < count_;) {
    // Read symbols from memory.  We intentionally bypass the cache to save memory.
    // Do the reads in batches so that we minimize the number of memory read calls.
    uint64_t read_bytes = (count_ - symbol_idx) * entry_size_;
    uint8_t buffer[1024];
    read_bytes = std::min<size_t>(sizeof(buffer), read_bytes);
    uint64_t offset = symbol_idx * entry_size_;
    if (__builtin_add_overflow(offset, offset_, &offset)) {
      // The elf data might be malformed.
      break;
    }
    read_bytes = elf_memory->Read(offset, buffer, read_bytes);
    if (read_bytes < sizeof(SymType)) {
      // The elf data might be malformed.
      break;
    }
    for (uint64_t offset = 0; offset <= read_bytes - sizeof(SymType);
         offset += entry_size_, symbol_idx++) {
      SymType sym;
      memcpy(&sym, &buffer[offset], sizeof(SymType));  // Copy to ensure alignment.
      addrs.push_back(sym.st_value);  // Always insert so it is indexable by symbol index.
      // NB: It is important to filter our zero-sized symbols since otherwise we can get
      // duplicate end addresses in the table (e.g. if there is custom "end" symbol marker).
      if (IsFunc(&sym) && sym.st_size != 0) {
        remap_->push_back(symbol_idx);  // Indices of function symbols only.
      }
    }
  }
  // Sort by address to make the remap list binary searchable (stable due to the a<b tie break).
  auto comp = [&addrs](auto a, auto b) { return std::tie(addrs[a], a) < std::tie(addrs[b], b); };
  std::sort(remap_->begin(), remap_->end(), comp);
  // Remove duplicate entries (methods de-duplicated by the linker).
  auto pred = [&addrs](auto a, auto b) { return addrs[a] == addrs[b]; };
  remap_->erase(std::unique(remap_->begin(), remap_->end(), pred), remap_->end());
  remap_->shrink_to_fit();
}

template <typename SymType>
bool Symbols::GetName(uint64_t addr, Memory* elf_memory, SharedString* name,
                      uint64_t* func_offset) {
  Info* info;
  if (!remap_.has_value()) {
    // Assume the symbol table is sorted. If it is not, this will gracefully fail.
    info = BinarySearch<SymType, false>(addr, elf_memory, func_offset);
    if (info == nullptr) {
      // Create the remapping table and retry the search.
      BuildRemapTable<SymType>(elf_memory);
      symbols_.clear();  // Remove cached symbols since the access pattern will be different.
      info = BinarySearch<SymType, true>(addr, elf_memory, func_offset);
    }
  } else {
    // Fast search using the previously created remap table.
    info = BinarySearch<SymType, true>(addr, elf_memory, func_offset);
  }
  if (info == nullptr) {
    return false;
  }
  // Read and cache the symbol name.
  if (info->name.is_null()) {
    SymType sym;
    uint32_t symbol_index = remap_.has_value() ? remap_.value()[info->index] : info->index;
    uint64_t offset = symbol_index * entry_size_;
    if (__builtin_add_overflow(offset, offset_, &offset)) {
      // The elf data might be malformed.
      return false;
    }
    if (!elf_memory->ReadFully(offset, &sym, sizeof(sym))) {
      return false;
    }
    std::string symbol_name;
    uint64_t str;
    if (__builtin_add_overflow(str_offset_, sym.st_name, &str) || str >= str_end_) {
      return false;
    }
    if (!IsFunc(&sym) || !elf_memory->ReadString(str, &symbol_name, str_end_ - str)) {
      return false;
    }
    info->name = SharedString(std::move(symbol_name));
  }
  *name = info->name;
  return true;
}

template <typename SymType>
bool Symbols::GetGlobal(Memory* elf_memory, const std::string& name, uint64_t* memory_address) {
  // Lookup from cache.
  auto it = global_variables_.find(name);
  if (it != global_variables_.end()) {
    if (it->second.has_value()) {
      *memory_address = it->second.value();
      return true;
    }
    return false;
  }

  // Linear scan of all symbols.
  for (uint32_t i = 0; i < count_; i++) {
    uint64_t offset = i * entry_size_;
    if (__builtin_add_overflow(offset_, offset, &offset)) {
      // The elf data might be malformed.
      return false;
    }
    SymType entry;
    if (!elf_memory->ReadFully(offset, &entry, sizeof(entry))) {
      return false;
    }

    if (entry.st_shndx != SHN_UNDEF && ELF32_ST_TYPE(entry.st_info) == STT_OBJECT &&
        ELF32_ST_BIND(entry.st_info) == STB_GLOBAL) {
      uint64_t str_offset = str_offset_ + entry.st_name;
      if (__builtin_add_overflow(str_offset_, entry.st_name, &str_offset)) {
        // The elf data might be malformed.
        return false;
      }
      if (str_offset < str_end_) {
        std::string symbol;
        if (elf_memory->ReadString(str_offset, &symbol, str_end_ - str_offset) && symbol == name) {
          global_variables_.emplace(name, entry.st_value);
          *memory_address = entry.st_value;
          return true;
        }
      }
    }
  }
  global_variables_.emplace(name, std::optional<uint64_t>());  // Remember "not found" outcome.
  return false;
}

// Instantiate all of the needed template functions.
template bool Symbols::GetName<Elf32_Sym>(uint64_t, Memory*, SharedString*, uint64_t*);
template bool Symbols::GetName<Elf64_Sym>(uint64_t, Memory*, SharedString*, uint64_t*);

template bool Symbols::GetGlobal<Elf32_Sym>(Memory*, const std::string&, uint64_t*);
template bool Symbols::GetGlobal<Elf64_Sym>(Memory*, const std::string&, uint64_t*);
}  // namespace unwindstack
