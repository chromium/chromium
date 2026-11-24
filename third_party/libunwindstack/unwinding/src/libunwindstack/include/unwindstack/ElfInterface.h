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

#include <elf.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <unwindstack/DwarfSection.h>
#include <unwindstack/Error.h>
#include <unwindstack/SharedString.h>

namespace unwindstack {

// Forward declarations.
class Memory;
class Regs;
class Symbols;

struct LoadInfo {
  uint64_t offset;
  uint64_t table_offset;
  size_t table_size;
};

struct SectionInfo {
  uint64_t offset;
  uint64_t size;
  uint64_t flags;
  int64_t bias;
};

enum : uint8_t {
  SONAME_UNKNOWN = 0,
  SONAME_VALID,
  SONAME_INVALID,
};

struct ElfTypes32 {
  using AddressType = uint32_t;
  using Chdr = Elf32_Chdr;
  using Dyn = Elf32_Dyn;
  using Ehdr = Elf32_Ehdr;
  using Nhdr = Elf32_Nhdr;
  using Phdr = Elf32_Phdr;
  using Shdr = Elf32_Shdr;
  using Sym = Elf32_Sym;
};

struct ElfTypes64 {
  using AddressType = uint64_t;
  using Chdr = Elf64_Chdr;
  using Dyn = Elf64_Dyn;
  using Ehdr = Elf64_Ehdr;
  using Nhdr = Elf64_Nhdr;
  using Phdr = Elf64_Phdr;
  using Shdr = Elf64_Shdr;
  using Sym = Elf64_Sym;
};

class ElfInterface {
 public:
  ElfInterface(std::shared_ptr<Memory>& memory) : memory_(memory) {}
  virtual ~ElfInterface();

  virtual bool Init(int64_t* load_bias) = 0;

  virtual void InitHeaders() = 0;

  virtual std::string GetSoname() = 0;

  virtual bool GetFunctionName(uint64_t addr, SharedString* name, uint64_t* offset) = 0;

  virtual bool GetGlobalVariable(const std::string& name, uint64_t* memory_address) = 0;

  virtual std::string GetBuildID() = 0;

  virtual bool Step(uint64_t rel_pc, Regs* regs, Memory* process_memory, bool* finished,
                    bool* is_signal_frame);

  virtual bool IsValidPc(uint64_t pc);

  bool GetTextRange(uint64_t* addr, uint64_t* size);

  std::shared_ptr<Memory> CreateGnuDebugdataMemory();

  std::shared_ptr<Memory> memory() { return memory_; }

  const std::unordered_map<uint64_t, LoadInfo>& pt_loads() { return pt_loads_; }

  void SetGnuDebugdataInterface(ElfInterface* interface) { gnu_debugdata_interface_ = interface; }

  uint64_t dynamic_offset() { return dynamic_offset_; }
  uint64_t dynamic_vaddr_start() { return dynamic_vaddr_start_; }
  uint64_t dynamic_vaddr_end() { return dynamic_vaddr_end_; }

  uint64_t data_offset() { return data_offset_; }
  uint64_t data_vaddr_start() { return data_vaddr_start_; }
  uint64_t data_vaddr_end() { return data_vaddr_end_; }

  const SectionInfo& eh_frame_hdr_info() { return eh_frame_hdr_info_; }
  const SectionInfo& eh_frame_info() { return eh_frame_info_; }
  const SectionInfo& debug_frame_info() { return debug_frame_info_; }

  uint64_t gnu_debugdata_offset() { return gnu_debugdata_offset_; }
  uint64_t gnu_debugdata_size() { return gnu_debugdata_size_; }

  uint64_t gnu_build_id_offset() { return gnu_build_id_offset_; }
  uint64_t gnu_build_id_size() { return gnu_build_id_size_; }

  DwarfSection* eh_frame() { return eh_frame_.get(); }
  DwarfSection* debug_frame() { return debug_frame_.get(); }

  const ErrorData& last_error() { return last_error_; }
  ErrorCode LastErrorCode() { return last_error_.code; }
  uint64_t LastErrorAddress() { return last_error_.address; }

  template <typename EhdrType, typename PhdrType>
  static int64_t GetLoadBias(Memory* memory);

  template <typename EhdrType, typename ShdrType, typename NhdrType>
  static std::string ReadBuildIDFromMemory(Memory* memory);

 protected:
  virtual void HandleUnknownType(uint32_t, uint64_t, uint64_t) {}

  std::shared_ptr<Memory> memory_;
  std::unordered_map<uint64_t, LoadInfo> pt_loads_;

  // Stored elf data.
  uint64_t dynamic_offset_ = 0;
  uint64_t dynamic_vaddr_start_ = 0;
  uint64_t dynamic_vaddr_end_ = 0;

  uint64_t data_offset_ = 0;
  uint64_t data_vaddr_start_ = 0;
  uint64_t data_vaddr_end_ = 0;

  SectionInfo eh_frame_hdr_info_ = {};
  SectionInfo eh_frame_info_ = {};
  SectionInfo debug_frame_info_ = {};

  uint64_t gnu_debugdata_offset_ = 0;
  uint64_t gnu_debugdata_size_ = 0;

  uint64_t gnu_build_id_offset_ = 0;
  uint64_t gnu_build_id_size_ = 0;

  uint64_t text_addr_ = 0;
  uint64_t text_size_ = 0;

  uint8_t soname_type_ = SONAME_UNKNOWN;
  std::string soname_;

  ErrorData last_error_{ERROR_NONE, 0};

  std::unique_ptr<DwarfSection> eh_frame_;
  std::unique_ptr<DwarfSection> debug_frame_;
  // The Elf object owns the gnu_debugdata interface object.
  ElfInterface* gnu_debugdata_interface_ = nullptr;

  std::vector<Symbols*> symbols_;
  std::vector<std::pair<uint64_t, uint64_t>> strtabs_;
};

template <typename ElfTypes>
class ElfInterfaceImpl : public ElfInterface {
 public:
  using AddressType = typename ElfTypes::AddressType;
  using ChdrType = typename ElfTypes::Chdr;
  using DynType = typename ElfTypes::Dyn;
  using EhdrType = typename ElfTypes::Ehdr;
  using NhdrType = typename ElfTypes::Nhdr;
  using PhdrType = typename ElfTypes::Phdr;
  using ShdrType = typename ElfTypes::Shdr;
  using SymType = typename ElfTypes::Sym;

  ElfInterfaceImpl(std::shared_ptr<Memory>& memory) : ElfInterface(memory) {}
  virtual ~ElfInterfaceImpl() = default;

  bool Init(int64_t* load_bias) override { return ReadAllHeaders(load_bias); }

  void InitHeaders() override;

  std::string GetSoname() override;

  bool GetFunctionName(uint64_t addr, SharedString* name, uint64_t* func_offset) override;

  bool GetGlobalVariable(const std::string& name, uint64_t* memory_address) override;

  std::string GetBuildID() override { return ReadBuildID(); }

  static void GetMaxSize(Memory* memory, uint64_t* size);

 protected:
  bool ReadAllHeaders(int64_t* load_bias);

  void ReadProgramHeaders(const EhdrType& ehdr, int64_t* load_bias);

  void ReadSectionHeaders(const EhdrType& ehdr);

  std::string ReadBuildID();
};

using ElfInterface32 = ElfInterfaceImpl<ElfTypes32>;
using ElfInterface64 = ElfInterfaceImpl<ElfTypes64>;

}  // namespace unwindstack
