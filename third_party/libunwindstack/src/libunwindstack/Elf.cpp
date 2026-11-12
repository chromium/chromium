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

#include <elf.h>
#include <inttypes.h>
#include <string.h>
#include <sys/mman.h>

#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include <android-base/stringprintf.h>

#include <unwindstack/Elf.h>
#include <unwindstack/ElfInterface.h>
#include <unwindstack/Log.h>
#include <unwindstack/MapInfo.h>
#include <unwindstack/Memory.h>
#include <unwindstack/Regs.h>
#include <unwindstack/SharedString.h>

#include <android-base/stringprintf.h>

#include "ElfInterfaceArm.h"
#include "Symbols.h"

namespace unwindstack {

bool Elf::cache_enabled_;
std::unordered_map<std::string, std::unordered_map<uint64_t, std::shared_ptr<Elf>>>* Elf::cache_;
std::mutex* Elf::cache_lock_;

bool Elf::Init() {
  load_bias_ = 0;
  if (!memory_) {
    return false;
  }

  interface_.reset(CreateInterfaceFromMemory(memory_));
  if (!interface_) {
    return false;
  }

  valid_ = interface_->Init(&load_bias_);
  if (valid_) {
    interface_->InitHeaders();
    InitGnuDebugdata();
  } else {
    interface_.reset(nullptr);
  }
  return valid_;
}

// It is expensive to initialize the .gnu_debugdata section. Provide a method
// to initialize this data separately.
void Elf::InitGnuDebugdata() {
  if (!valid_ || interface_->gnu_debugdata_offset() == 0) {
    return;
  }

  auto memory = interface_->CreateGnuDebugdataMemory();
  gnu_debugdata_interface_.reset(CreateInterfaceFromMemory(memory));
  ElfInterface* gnu = gnu_debugdata_interface_.get();
  if (gnu == nullptr) {
    return;
  }

  // Ignore the load_bias from the compressed section, the correct load bias
  // is in the uncompressed data.
  int64_t load_bias;
  if (gnu->Init(&load_bias)) {
    gnu->InitHeaders();
    interface_->SetGnuDebugdataInterface(gnu);
  } else {
    gnu_debugdata_interface_.reset(nullptr);
  }
}

void Elf::Invalidate() {
  interface_.reset(nullptr);
  valid_ = false;
}

std::string Elf::GetSoname() {
  std::lock_guard<std::mutex> guard(lock_);
  if (!valid_) {
    return "";
  }
  return interface_->GetSoname();
}

uint64_t Elf::GetRelPc(uint64_t pc, MapInfo* map_info) {
  return pc - map_info->start() + load_bias_ + map_info->elf_offset();
}

bool Elf::GetFunctionName(uint64_t addr, SharedString* name, uint64_t* func_offset) {
  std::lock_guard<std::mutex> guard(lock_);
  return valid_ && (interface_->GetFunctionName(addr, name, func_offset) ||
                    (gnu_debugdata_interface_ &&
                     gnu_debugdata_interface_->GetFunctionName(addr, name, func_offset)));
}

bool Elf::GetGlobalVariableOffset(const std::string& name, uint64_t* memory_offset) {
  if (!valid_) {
    return false;
  }

  uint64_t vaddr;
  if (!interface_->GetGlobalVariable(name, &vaddr) &&
      (gnu_debugdata_interface_ == nullptr ||
       !gnu_debugdata_interface_->GetGlobalVariable(name, &vaddr))) {
    return false;
  }

  if (arch() == ARCH_ARM64) {
    // Tagged pointer after Android R would lead top byte to have random values
    // https://source.android.com/devices/tech/debug/tagged-pointers
    vaddr &= (1ULL << 56) - 1;
  }

  // Check the .data section.
  uint64_t vaddr_start = interface_->data_vaddr_start();
  if (vaddr >= vaddr_start && vaddr < interface_->data_vaddr_end()) {
    *memory_offset = vaddr - vaddr_start + interface_->data_offset();
    return true;
  }

  // Check the .dynamic section.
  vaddr_start = interface_->dynamic_vaddr_start();
  if (vaddr >= vaddr_start && vaddr < interface_->dynamic_vaddr_end()) {
    *memory_offset = vaddr - vaddr_start + interface_->dynamic_offset();
    return true;
  }

  return false;
}

std::string Elf::GetBuildID() {
  if (!valid_) {
    return "";
  }
  return interface_->GetBuildID();
}

void Elf::GetLastError(ErrorData* data) {
  if (valid_) {
    *data = interface_->last_error();
  } else {
    data->code = ERROR_INVALID_ELF;
    data->address = 0;
  }
}

ErrorCode Elf::GetLastErrorCode() {
  if (valid_) {
    return interface_->LastErrorCode();
  }
  return ERROR_INVALID_ELF;
}

uint64_t Elf::GetLastErrorAddress() {
  if (valid_) {
    return interface_->LastErrorAddress();
  }
  return 0;
}

// The relative pc expectd by this function is relative to the start of the elf.
bool Elf::StepIfSignalHandler(uint64_t rel_pc, Regs* regs, Memory* process_memory) {
  if (!valid_) {
    return false;
  }

  // Convert the rel_pc to an elf_offset.
  if (rel_pc < static_cast<uint64_t>(load_bias_)) {
    return false;
  }
  return regs->StepIfSignalHandler(rel_pc - load_bias_, this, process_memory);
}

// The relative pc is always relative to the start of the map from which it comes.
bool Elf::Step(uint64_t rel_pc, Regs* regs, Memory* process_memory, bool* finished,
               bool* is_signal_frame) {
  if (!valid_) {
    return false;
  }

  // Lock during the step which can update information in the object.
  std::lock_guard<std::mutex> guard(lock_);
  return interface_->Step(rel_pc, regs, process_memory, finished, is_signal_frame);
}

bool Elf::IsValidElf(Memory* memory) {
  if (memory == nullptr) {
    return false;
  }

  // Verify that this is a valid elf file.
  uint8_t e_ident[SELFMAG + 1];
  if (!memory->ReadFully(0, e_ident, SELFMAG)) {
    return false;
  }

  if (memcmp(e_ident, ELFMAG, SELFMAG) != 0) {
    return false;
  }
  return true;
}

bool Elf::GetInfo(Memory* memory, uint64_t* size) {
  if (!IsValidElf(memory)) {
    return false;
  }
  *size = 0;

  uint8_t class_type;
  if (!memory->ReadFully(EI_CLASS, &class_type, 1)) {
    return false;
  }

  // Get the maximum size of the elf data from the header.
  if (class_type == ELFCLASS32) {
    ElfInterface32::GetMaxSize(memory, size);
  } else if (class_type == ELFCLASS64) {
    ElfInterface64::GetMaxSize(memory, size);
  } else {
    return false;
  }
  return true;
}

bool Elf::IsValidPc(uint64_t pc) {
  if (!valid_ || (load_bias_ > 0 && pc < static_cast<uint64_t>(load_bias_))) {
    return false;
  }

  if (interface_->IsValidPc(pc)) {
    return true;
  }

  if (gnu_debugdata_interface_ != nullptr && gnu_debugdata_interface_->IsValidPc(pc)) {
    return true;
  }

  return false;
}

bool Elf::GetTextRange(uint64_t* addr, uint64_t* size) {
  if (!valid_) {
    return false;
  }

  if (interface_->GetTextRange(addr, size)) {
    *addr += load_bias_;
    return true;
  }

  if (gnu_debugdata_interface_ != nullptr && gnu_debugdata_interface_->GetTextRange(addr, size)) {
    *addr += load_bias_;
    return true;
  }

  return false;
}

ElfInterface* Elf::CreateInterfaceFromMemory(std::shared_ptr<Memory>& memory) {
  if (!IsValidElf(memory.get())) {
    return nullptr;
  }

  std::unique_ptr<ElfInterface> interface;
  if (!memory->ReadFully(EI_CLASS, &class_type_, 1)) {
    return nullptr;
  }
  if (class_type_ == ELFCLASS32) {
    Elf32_Half e_machine;
    if (!memory->ReadFully(EI_NIDENT + sizeof(Elf32_Half), &e_machine, sizeof(e_machine))) {
      return nullptr;
    }

    machine_type_ = e_machine;
    if (e_machine == EM_ARM) {
      arch_ = ARCH_ARM;
      interface.reset(new ElfInterfaceArm(memory));
    } else if (e_machine == EM_386) {
      arch_ = ARCH_X86;
      interface.reset(new ElfInterface32(memory));
    } else {
      // Unsupported.
      return nullptr;
    }
  } else if (class_type_ == ELFCLASS64) {
    Elf64_Half e_machine;
    if (!memory->ReadFully(EI_NIDENT + sizeof(Elf64_Half), &e_machine, sizeof(e_machine))) {
      return nullptr;
    }

    machine_type_ = e_machine;
    if (e_machine == EM_AARCH64) {
      arch_ = ARCH_ARM64;
    } else if (e_machine == EM_X86_64) {
      arch_ = ARCH_X86_64;
    } else if (e_machine == EM_RISCV) {
      arch_ = ARCH_RISCV64;
    } else {
      // Unsupported.
      return nullptr;
    }
    interface.reset(new ElfInterface64(memory));
  }

  return interface.release();
}

int64_t Elf::GetLoadBias(Memory* memory) {
  if (!IsValidElf(memory)) {
    return 0;
  }

  uint8_t class_type;
  if (!memory->Read(EI_CLASS, &class_type, 1)) {
    return 0;
  }

  if (class_type == ELFCLASS32) {
    return ElfInterface::GetLoadBias<Elf32_Ehdr, Elf32_Phdr>(memory);
  } else if (class_type == ELFCLASS64) {
    return ElfInterface::GetLoadBias<Elf64_Ehdr, Elf64_Phdr>(memory);
  }
  return 0;
}

void Elf::SetCachingEnabled(bool enable) {
  if (!cache_enabled_ && enable) {
    cache_enabled_ = true;
    cache_ =
        new std::unordered_map<std::string, std::unordered_map<uint64_t, std::shared_ptr<Elf>>>;
    cache_lock_ = new std::mutex;
  } else if (cache_enabled_ && !enable) {
    cache_enabled_ = false;
    delete cache_;
    delete cache_lock_;
  }
}

void Elf::CacheLock() {
  cache_lock_->lock();
}

void Elf::CacheUnlock() {
  cache_lock_->unlock();
}

void Elf::CacheAdd(MapInfo* info) {
  if (!info->elf()->valid()) {
    return;
  }
  (*cache_)[std::string(info->name())].emplace(info->elf_start_offset(), info->elf());
}

bool Elf::CacheGet(MapInfo* info) {
  auto name_entry = cache_->find(std::string(info->name()));
  if (name_entry == cache_->end()) {
    return false;
  }
  // First look to see if there is a zero offset entry, this indicates
  // the whole elf is the file.
  auto& offset_cache = name_entry->second;
  uint64_t elf_start_offset = 0;
  auto entry = offset_cache.find(elf_start_offset);
  if (entry == offset_cache.end()) {
    // Try and find using the current offset.
    elf_start_offset = info->offset();
    entry = offset_cache.find(elf_start_offset);
    if (entry == offset_cache.end()) {
      // If this is an execute map, then see if the previous read-only
      // map is the start of the elf.
      if (!(info->flags() & PROT_EXEC)) {
        return false;
      }
      auto prev_map = info->GetPrevRealMap();
      if (prev_map == nullptr || info->offset() <= prev_map->offset() ||
          (prev_map->flags() != PROT_READ)) {
        return false;
      }
      elf_start_offset = prev_map->offset();
      entry = offset_cache.find(elf_start_offset);
      if (entry == offset_cache.end()) {
        return false;
      }
    }
  }

  info->set_elf(entry->second);
  info->set_elf_start_offset(elf_start_offset);
  info->set_elf_offset(info->offset() - elf_start_offset);
  return true;
}

std::string Elf::GetBuildID(Memory* memory) {
  if (!IsValidElf(memory)) {
    return "";
  }

  uint8_t class_type;
  if (!memory->Read(EI_CLASS, &class_type, 1)) {
    return "";
  }

  if (class_type == ELFCLASS32) {
    return ElfInterface::ReadBuildIDFromMemory<Elf32_Ehdr, Elf32_Shdr, Elf32_Nhdr>(memory);
  } else if (class_type == ELFCLASS64) {
    return ElfInterface::ReadBuildIDFromMemory<Elf64_Ehdr, Elf64_Shdr, Elf64_Nhdr>(memory);
  }
  return "";
}

std::string Elf::GetPrintableBuildID(std::string& build_id) {
  if (build_id.empty()) {
    return "";
  }
  std::string printable_build_id;
  for (const char& c : build_id) {
    // Use %hhx to avoid sign extension on abis that have signed chars.
    printable_build_id += android::base::StringPrintf("%02hhx", c);
  }
  return printable_build_id;
}

std::string Elf::GetPrintableBuildID() {
  std::string build_id = GetBuildID();
  return Elf::GetPrintableBuildID(build_id);
}

}  // namespace unwindstack
