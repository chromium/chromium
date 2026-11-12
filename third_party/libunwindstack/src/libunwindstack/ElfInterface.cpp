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

#include <memory>
#include <string>
#include <utility>

#include <zlib.h>
#include <zstd.h>

#include <unwindstack/DwarfError.h>
#include <unwindstack/DwarfSection.h>
#include <unwindstack/ElfInterface.h>
#include <unwindstack/Log.h>
#include <unwindstack/Memory.h>
#include <unwindstack/Regs.h>

#include "DwarfDebugFrame.h"
#include "DwarfEhFrame.h"
#include "DwarfEhFrameWithHdr.h"
#include "MemoryBuffer.h"
#include "MemoryXz.h"
#include "Symbols.h"

namespace unwindstack {

ElfInterface::~ElfInterface() {
  for (auto symbol : symbols_) {
    delete symbol;
  }
}

bool ElfInterface::IsValidPc(uint64_t pc) {
  if (!pt_loads_.empty()) {
    for (auto& entry : pt_loads_) {
      uint64_t start = entry.second.table_offset;
      uint64_t end = start + entry.second.table_size;
      if (pc >= start && pc < end) {
        return true;
      }
    }
    return false;
  }

  // No PT_LOAD data, look for a fde for this pc in the section data.
  if (debug_frame_ != nullptr && debug_frame_->GetFdeFromPc(pc) != nullptr) {
    return true;
  }

  if (eh_frame_ != nullptr && eh_frame_->GetFdeFromPc(pc) != nullptr) {
    return true;
  }

  return false;
}

bool ElfInterface::GetTextRange(uint64_t* addr, uint64_t* size) {
  if (text_size_ != 0) {
    *addr = text_addr_;
    *size = text_size_;
    return true;
  }
  return false;
}

std::shared_ptr<Memory> ElfInterface::CreateGnuDebugdataMemory() {
  if (gnu_debugdata_offset_ == 0 || gnu_debugdata_size_ == 0) {
    return nullptr;
  }

  auto decompressed = std::make_shared<MemoryXz>(memory_.get(), gnu_debugdata_offset_,
                                                 gnu_debugdata_size_, GetSoname());
  if (!decompressed || !decompressed->Init()) {
    gnu_debugdata_offset_ = 0;
    gnu_debugdata_size_ = 0;
    return nullptr;
  }
  return decompressed;
}

static bool ZlibDecompress(uint8_t* compressed_data, size_t compressed_size, MemoryBuffer* memory) {
  z_stream stream;
  stream.zalloc = Z_NULL;
  stream.zfree = Z_NULL;
  stream.opaque = Z_NULL;
  if (inflateInit(&stream) != Z_OK) {
    return false;
  }
  stream.next_in = compressed_data;
  stream.avail_in = compressed_size;
  stream.next_out = memory->Data();
  stream.avail_out = memory->Size();
  int ret = inflate(&stream, Z_FINISH);
  if (inflateEnd(&stream) != Z_OK) {
    return false;
  }
  return ret == Z_STREAM_END;
}

static bool ZstdDecompress(uint8_t* compressed_data, size_t compressed_size, MemoryBuffer* memory) {
  size_t decompress_size =
      ZSTD_decompress(memory->Data(), memory->Size(), compressed_data, compressed_size);
  return memory->Size() == decompress_size;
}

template <typename ChdrType>
std::shared_ptr<Memory> CreateMemoryFromCompressedSection(SectionInfo& info,
                                                          std::shared_ptr<Memory>& elf_memory) {
  if (info.size < sizeof(ChdrType)) {
    return nullptr;
  }

  uint8_t* compressed_data = elf_memory->GetPtr(info.offset);
  std::vector<uint8_t> compressed;
  if (compressed_data == nullptr || elf_memory->GetPtr(info.offset + info.size - 1) == nullptr) {
    compressed.resize(info.size);
    if (!elf_memory->ReadFully(info.offset, compressed.data(), info.size)) {
      return nullptr;
    }
    compressed_data = compressed.data();
  }

  ChdrType* chdr = reinterpret_cast<ChdrType*>(compressed_data);
  std::shared_ptr<MemoryBuffer> memory(new MemoryBuffer(chdr->ch_size, info.offset));

  bool ret = false;
  if (chdr->ch_type == ELFCOMPRESS_ZLIB) {
    ret = ZlibDecompress(&compressed_data[sizeof(ChdrType)], info.size - sizeof(ChdrType),
                         memory.get());
  } else if (chdr->ch_type == ELFCOMPRESS_ZSTD) {
    ret = ZstdDecompress(&compressed_data[sizeof(ChdrType)], info.size - sizeof(ChdrType),
                         memory.get());
  }
  if (!ret) {
    return nullptr;
  }
  // Set the section info to match the uncompressed section data.
  info.size = chdr->ch_size;
  info.flags &= ~SHF_COMPRESSED;
  return memory;
}

template <typename ElfTypes>
void ElfInterfaceImpl<ElfTypes>::InitHeaders() {
  if (eh_frame_hdr_info_.offset != 0) {
    DwarfEhFrameWithHdr<AddressType>* eh_frame_hdr = new DwarfEhFrameWithHdr<AddressType>(memory_);
    eh_frame_.reset(eh_frame_hdr);
    if (!eh_frame_hdr->EhFrameInit(eh_frame_info_) || !eh_frame_->Init(eh_frame_hdr_info_)) {
      eh_frame_hdr_info_ = {};
      eh_frame_.reset(nullptr);
    }
  }

  if (eh_frame_.get() == nullptr && eh_frame_info_.offset != 0) {
    // If there is an eh_frame section without an eh_frame_hdr section,
    // or using the frame hdr object failed to init.
    eh_frame_.reset(new DwarfEhFrame<AddressType>(memory_));
    if (!eh_frame_->Init(eh_frame_info_)) {
      eh_frame_info_ = {};
      eh_frame_.reset(nullptr);
    }
  }

  if (debug_frame_info_.offset != 0) {
    std::shared_ptr<Memory> debug_memory = memory_;
    if (debug_frame_info_.flags & SHF_COMPRESSED) {
      debug_memory = CreateMemoryFromCompressedSection<ChdrType>(debug_frame_info_, memory_);
    }
    debug_frame_.reset(new DwarfDebugFrame<AddressType>(debug_memory));
    if (!debug_frame_->Init(debug_frame_info_)) {
      debug_frame_.reset(nullptr);
      debug_frame_info_ = {};
    }
  }
}

template <typename ElfTypes>
bool ElfInterfaceImpl<ElfTypes>::ReadAllHeaders(int64_t* load_bias) {
  EhdrType ehdr;
  if (!memory_->ReadFully(0, &ehdr, sizeof(ehdr))) {
    last_error_.code = ERROR_MEMORY_INVALID;
    last_error_.address = 0;
    return false;
  }

  // If we have enough information that this is an elf file, then allow
  // malformed program and section headers.
  ReadProgramHeaders(ehdr, load_bias);
  ReadSectionHeaders(ehdr);
  return true;
}

template <typename EhdrType, typename PhdrType>
int64_t ElfInterface::GetLoadBias(Memory* memory) {
  EhdrType ehdr;
  if (!memory->ReadFully(0, &ehdr, sizeof(ehdr))) {
    return false;
  }

  uint64_t offset = ehdr.e_phoff;
  for (size_t i = 0; i < ehdr.e_phnum; i++, offset += ehdr.e_phentsize) {
    PhdrType phdr;
    if (!memory->ReadFully(offset, &phdr, sizeof(phdr))) {
      return 0;
    }

    // Find the first executable load when looking for the load bias.
    if (phdr.p_type == PT_LOAD && (phdr.p_flags & PF_X)) {
      return static_cast<uint64_t>(phdr.p_vaddr) - phdr.p_offset;
    }
  }
  return 0;
}

template <typename ElfTypes>
void ElfInterfaceImpl<ElfTypes>::ReadProgramHeaders(const EhdrType& ehdr, int64_t* load_bias) {
  uint64_t offset = ehdr.e_phoff;
  bool first_exec_load_header = true;
  for (size_t i = 0; i < ehdr.e_phnum; i++, offset += ehdr.e_phentsize) {
    PhdrType phdr;
    if (!memory_->ReadFully(offset, &phdr, sizeof(phdr))) {
      return;
    }

    switch (phdr.p_type) {
    case PT_LOAD:
    {
      if ((phdr.p_flags & PF_X) == 0) {
        continue;
      }

      pt_loads_[phdr.p_offset] = LoadInfo{phdr.p_offset, phdr.p_vaddr,
                                          static_cast<size_t>(phdr.p_memsz)};
      // Only set the load bias from the first executable load header.
      if (first_exec_load_header) {
        *load_bias = static_cast<uint64_t>(phdr.p_vaddr) - phdr.p_offset;
      }
      first_exec_load_header = false;
      break;
    }

    case PT_GNU_EH_FRAME:
      // This is really the pointer to the .eh_frame_hdr section.
      eh_frame_hdr_info_ = {
          .offset = phdr.p_offset,
          .size = phdr.p_memsz,
          .flags = phdr.p_flags,
          .bias = static_cast<int64_t>(static_cast<uint64_t>(phdr.p_vaddr) - phdr.p_offset)};
      break;

    case PT_DYNAMIC:
      dynamic_offset_ = phdr.p_offset;
      dynamic_vaddr_start_ = phdr.p_vaddr;
      if (__builtin_add_overflow(dynamic_vaddr_start_, phdr.p_memsz, &dynamic_vaddr_end_)) {
        dynamic_offset_ = 0;
        dynamic_vaddr_start_ = 0;
        dynamic_vaddr_end_ = 0;
      }
      break;

    default:
      HandleUnknownType(phdr.p_type, phdr.p_offset, phdr.p_filesz);
      break;
    }
  }
}

template <typename ElfTypes>
std::string ElfInterfaceImpl<ElfTypes>::ReadBuildID() {
  // Ensure there is no overflow in any of the calulations below.
  uint64_t tmp;
  if (__builtin_add_overflow(gnu_build_id_offset_, gnu_build_id_size_, &tmp)) {
    return "";
  }

  uint64_t offset = 0;
  while (offset < gnu_build_id_size_) {
    if (gnu_build_id_size_ - offset < sizeof(NhdrType)) {
      return "";
    }
    NhdrType hdr;
    if (!memory_->ReadFully(gnu_build_id_offset_ + offset, &hdr, sizeof(hdr))) {
      return "";
    }
    offset += sizeof(hdr);

    if (gnu_build_id_size_ - offset < hdr.n_namesz) {
      return "";
    }
    if (hdr.n_namesz > 0) {
      std::string name(hdr.n_namesz, '\0');
      if (!memory_->ReadFully(gnu_build_id_offset_ + offset, &(name[0]), hdr.n_namesz)) {
        return "";
      }

      // Trim trailing \0 as GNU is stored as a C string in the ELF file.
      if (name.back() == '\0')
        name.resize(name.size() - 1);

      // Align hdr.n_namesz to next power multiple of 4. See man 5 elf.
      offset += (hdr.n_namesz + 3) & ~3;

      if (name == "GNU" && hdr.n_type == NT_GNU_BUILD_ID) {
        if (gnu_build_id_size_ - offset < hdr.n_descsz || hdr.n_descsz == 0) {
          return "";
        }
        std::string build_id(hdr.n_descsz, '\0');
        if (memory_->ReadFully(gnu_build_id_offset_ + offset, &build_id[0], hdr.n_descsz)) {
          return build_id;
        }
        return "";
      }
    }
    // Align hdr.n_descsz to next power multiple of 4. See man 5 elf.
    offset += (hdr.n_descsz + 3) & ~3;
  }
  return "";
}
template <typename ElfTypes>
void ElfInterfaceImpl<ElfTypes>::ReadSectionHeaders(const EhdrType& ehdr) {
  uint64_t offset = ehdr.e_shoff;
  uint64_t sec_offset = 0;
  uint64_t sec_size = 0;

  // Get the location of the section header names.
  // If something is malformed in the header table data, we aren't going
  // to terminate, we'll simply ignore this part.
  ShdrType shdr;
  if (ehdr.e_shstrndx < ehdr.e_shnum) {
    uint64_t sh_offset = offset + ehdr.e_shstrndx * ehdr.e_shentsize;
    if (memory_->ReadFully(sh_offset, &shdr, sizeof(shdr))) {
      sec_offset = shdr.sh_offset;
      sec_size = shdr.sh_size;
    }
  }

  // Skip the first header, it's always going to be NULL.
  offset += ehdr.e_shentsize;
  for (size_t i = 1; i < ehdr.e_shnum; i++, offset += ehdr.e_shentsize) {
    if (!memory_->ReadFully(offset, &shdr, sizeof(shdr))) {
      return;
    }

    if (shdr.sh_type == SHT_SYMTAB || shdr.sh_type == SHT_DYNSYM) {
      // Need to go get the information about the section that contains
      // the string terminated names.
      ShdrType str_shdr;
      if (shdr.sh_link >= ehdr.e_shnum) {
        continue;
      }
      uint64_t str_offset = ehdr.e_shoff + shdr.sh_link * ehdr.e_shentsize;
      if (!memory_->ReadFully(str_offset, &str_shdr, sizeof(str_shdr))) {
        continue;
      }
      if (str_shdr.sh_type != SHT_STRTAB) {
        continue;
      }
      symbols_.push_back(new Symbols(shdr.sh_offset, shdr.sh_size, shdr.sh_entsize,
                                     str_shdr.sh_offset, str_shdr.sh_size));
    } else if ((shdr.sh_type == SHT_PROGBITS || shdr.sh_type == SHT_NOBITS) && sec_size != 0) {
      // Look for the .debug_frame and .gnu_debugdata.
      if (shdr.sh_name < sec_size) {
        std::string name;
        if (memory_->ReadString(sec_offset + shdr.sh_name, &name, sec_size - shdr.sh_name)) {
          if (name == ".debug_frame") {
            debug_frame_info_ = {
                .offset = shdr.sh_offset,
                .size = shdr.sh_size,
                .flags = shdr.sh_flags,
                .bias = static_cast<int64_t>(static_cast<uint64_t>(shdr.sh_addr) - shdr.sh_offset)};
          } else if (name == ".gnu_debugdata") {
            gnu_debugdata_offset_ = shdr.sh_offset;
            gnu_debugdata_size_ = shdr.sh_size;
          } else if (name == ".eh_frame") {
            eh_frame_info_ = {
                .offset = shdr.sh_offset,
                .size = shdr.sh_size,
                .flags = shdr.sh_flags,
                .bias = static_cast<int64_t>(static_cast<uint64_t>(shdr.sh_addr) - shdr.sh_offset)};
          } else if (eh_frame_hdr_info_.offset == 0 && name == ".eh_frame_hdr") {
            eh_frame_hdr_info_ = {
                .offset = shdr.sh_offset,
                .size = shdr.sh_size,
                .flags = shdr.sh_flags,
                .bias = static_cast<int64_t>(static_cast<uint64_t>(shdr.sh_addr) - shdr.sh_offset)};
          } else if (name == ".data") {
            data_offset_ = shdr.sh_offset;
            data_vaddr_start_ = shdr.sh_addr;
            if (__builtin_add_overflow(data_vaddr_start_, shdr.sh_size, &data_vaddr_end_)) {
              data_offset_ = 0;
              data_vaddr_start_ = 0;
              data_vaddr_end_ = 0;
            }
          } else if (name == ".text") {
            text_addr_ = shdr.sh_addr;
            text_size_ = shdr.sh_size;
          }
        }
      }
    } else if (shdr.sh_type == SHT_STRTAB) {
      // In order to read soname, keep track of address to offset mapping.
      strtabs_.push_back(std::make_pair<uint64_t, uint64_t>(static_cast<uint64_t>(shdr.sh_addr),
                                                            static_cast<uint64_t>(shdr.sh_offset)));
    } else if (shdr.sh_type == SHT_NOTE) {
      if (shdr.sh_name < sec_size) {
        std::string name;
        if (memory_->ReadString(sec_offset + shdr.sh_name, &name, sec_size - shdr.sh_name) &&
            name == ".note.gnu.build-id") {
          gnu_build_id_offset_ = shdr.sh_offset;
          gnu_build_id_size_ = shdr.sh_size;
        }
      }
    }
  }
}

template <typename ElfTypes>
std::string ElfInterfaceImpl<ElfTypes>::GetSoname() {
  if (soname_type_ == SONAME_INVALID) {
    return "";
  }
  if (soname_type_ == SONAME_VALID) {
    return soname_;
  }

  soname_type_ = SONAME_INVALID;

  uint64_t soname_offset = 0;
  uint64_t strtab_addr = 0;
  uint64_t strtab_size = 0;

  // Find the soname location from the dynamic headers section.
  DynType dyn;
  uint64_t offset = dynamic_offset_;
  uint64_t max_offset = offset + dynamic_vaddr_end_ - dynamic_vaddr_start_;
  for (uint64_t offset = dynamic_offset_; offset < max_offset; offset += sizeof(DynType)) {
    if (!memory_->ReadFully(offset, &dyn, sizeof(dyn))) {
      last_error_.code = ERROR_MEMORY_INVALID;
      last_error_.address = offset;
      return "";
    }

    if (dyn.d_tag == DT_STRTAB) {
      strtab_addr = dyn.d_un.d_ptr;
    } else if (dyn.d_tag == DT_STRSZ) {
      strtab_size = dyn.d_un.d_val;
    } else if (dyn.d_tag == DT_SONAME) {
      soname_offset = dyn.d_un.d_val;
    } else if (dyn.d_tag == DT_NULL) {
      break;
    }
  }

  // Need to map the strtab address to the real offset.
  for (const auto& entry : strtabs_) {
    if (entry.first == strtab_addr) {
      soname_offset = entry.second + soname_offset;
      uint64_t soname_max = entry.second + strtab_size;
      if (soname_offset >= soname_max) {
        return "";
      }
      if (!memory_->ReadString(soname_offset, &soname_, soname_max - soname_offset)) {
        return "";
      }
      soname_type_ = SONAME_VALID;
      return soname_;
    }
  }
  return "";
}

template <typename ElfTypes>
bool ElfInterfaceImpl<ElfTypes>::GetFunctionName(uint64_t addr, SharedString* name,
                                                 uint64_t* func_offset) {
  if (symbols_.empty()) {
    return false;
  }

  for (const auto symbol : symbols_) {
    if (symbol->template GetName<SymType>(addr, memory_.get(), name, func_offset)) {
      return true;
    }
  }
  return false;
}

template <typename ElfTypes>
bool ElfInterfaceImpl<ElfTypes>::GetGlobalVariable(const std::string& name,
                                                   uint64_t* memory_address) {
  if (symbols_.empty()) {
    return false;
  }

  for (const auto symbol : symbols_) {
    if (symbol->template GetGlobal<SymType>(memory_.get(), name, memory_address)) {
      return true;
    }
  }
  return false;
}

bool ElfInterface::Step(uint64_t pc, Regs* regs, Memory* process_memory, bool* finished,
                        bool* is_signal_frame) {
  last_error_.code = ERROR_NONE;
  last_error_.address = 0;

  // Try the debug_frame first since it contains the most specific unwind
  // information.
  DwarfSection* debug_frame = debug_frame_.get();
  if (debug_frame != nullptr &&
      debug_frame->Step(pc, regs, process_memory, finished, is_signal_frame)) {
    return true;
  }

  // Try the eh_frame next.
  DwarfSection* eh_frame = eh_frame_.get();
  if (eh_frame != nullptr && eh_frame->Step(pc, regs, process_memory, finished, is_signal_frame)) {
    return true;
  }

  if (gnu_debugdata_interface_ != nullptr &&
      gnu_debugdata_interface_->Step(pc, regs, process_memory, finished, is_signal_frame)) {
    return true;
  }

  // Set the error code based on the first error encountered.
  DwarfSection* section = nullptr;
  if (debug_frame_ != nullptr) {
    section = debug_frame_.get();
  } else if (eh_frame_ != nullptr) {
    section = eh_frame_.get();
  } else if (gnu_debugdata_interface_ != nullptr) {
    last_error_ = gnu_debugdata_interface_->last_error();
    return false;
  } else {
    return false;
  }

  // Convert the DWARF ERROR to an external error.
  DwarfErrorCode code = section->LastErrorCode();
  switch (code) {
    case DWARF_ERROR_NONE:
      last_error_.code = ERROR_NONE;
      break;

    case DWARF_ERROR_MEMORY_INVALID:
      last_error_.code = ERROR_MEMORY_INVALID;
      last_error_.address = section->LastErrorAddress();
      break;

    case DWARF_ERROR_ILLEGAL_VALUE:
    case DWARF_ERROR_ILLEGAL_STATE:
    case DWARF_ERROR_STACK_INDEX_NOT_VALID:
    case DWARF_ERROR_TOO_MANY_ITERATIONS:
    case DWARF_ERROR_CFA_NOT_DEFINED:
    case DWARF_ERROR_NO_FDES:
      last_error_.code = ERROR_UNWIND_INFO;
      break;

    case DWARF_ERROR_NOT_IMPLEMENTED:
    case DWARF_ERROR_UNSUPPORTED_VERSION:
      last_error_.code = ERROR_UNSUPPORTED;
      break;
  }
  return false;
}

// This is an estimation of the size of the elf file using the location
// of the section headers and size. This assumes that the section headers
// are at the end of the elf file. If the elf has a load bias, the size
// will be too large, but this is acceptable.
template <typename ElfTypes>
void ElfInterfaceImpl<ElfTypes>::GetMaxSize(Memory* memory, uint64_t* size) {
  EhdrType ehdr;
  if (!memory->ReadFully(0, &ehdr, sizeof(ehdr))) {
    *size = 0;
    return;
  }

  // If this winds up as zero, the PT_LOAD reading will get a better value.
  uint64_t elf_size = ehdr.e_shoff + ehdr.e_shentsize * ehdr.e_shnum;

  // Search through the PT_LOAD values and if any result in a larger elf
  // size, use that.
  uint64_t offset = ehdr.e_phoff;
  for (size_t i = 0; i < ehdr.e_phnum; i++, offset += ehdr.e_phentsize) {
    PhdrType phdr;
    if (!memory->ReadFully(offset, &phdr, sizeof(phdr))) {
      break;
    }
    if (phdr.p_type == PT_LOAD) {
      uint64_t end_offset;
      if (__builtin_add_overflow(phdr.p_offset, phdr.p_memsz, &end_offset)) {
        continue;
      }
      if (end_offset > elf_size) {
        elf_size = end_offset;
      }
    }
  }

  *size = elf_size;
}

template <typename EhdrType, typename ShdrType>
bool GetBuildIDInfo(Memory* memory, uint64_t* build_id_offset, uint64_t* build_id_size) {
  EhdrType ehdr;
  if (!memory->ReadFully(0, &ehdr, sizeof(ehdr))) {
    return false;
  }

  uint64_t offset = ehdr.e_shoff;
  uint64_t sec_offset;
  uint64_t sec_size;
  ShdrType shdr;
  if (ehdr.e_shstrndx >= ehdr.e_shnum) {
    return false;
  }

  uint64_t sh_offset = offset + ehdr.e_shstrndx * ehdr.e_shentsize;
  if (!memory->ReadFully(sh_offset, &shdr, sizeof(shdr))) {
    return false;
  }
  sec_offset = shdr.sh_offset;
  sec_size = shdr.sh_size;

  // Skip the first header, it's always going to be NULL.
  offset += ehdr.e_shentsize;
  for (size_t i = 1; i < ehdr.e_shnum; i++, offset += ehdr.e_shentsize) {
    if (!memory->ReadFully(offset, &shdr, sizeof(shdr))) {
      return false;
    }
    std::string name;
    if (shdr.sh_type == SHT_NOTE && shdr.sh_name < sec_size &&
        memory->ReadString(sec_offset + shdr.sh_name, &name, sec_size - shdr.sh_name) &&
        name == ".note.gnu.build-id") {
      *build_id_offset = shdr.sh_offset;
      *build_id_size = shdr.sh_size;
      return true;
    }
  }

  return false;
}

template <typename EhdrType, typename ShdrType, typename NhdrType>
std::string ElfInterface::ReadBuildIDFromMemory(Memory* memory) {
  uint64_t note_offset;
  uint64_t note_size;
  if (!GetBuildIDInfo<EhdrType, ShdrType>(memory, &note_offset, &note_size)) {
    return "";
  }

  // Ensure there is no overflow in any of the calculations below.
  uint64_t tmp;
  if (__builtin_add_overflow(note_offset, note_size, &tmp)) {
    return "";
  }

  uint64_t offset = 0;
  while (offset < note_size) {
    if (note_size - offset < sizeof(NhdrType)) {
      return "";
    }
    NhdrType hdr;
    if (!memory->ReadFully(note_offset + offset, &hdr, sizeof(hdr))) {
      return "";
    }
    offset += sizeof(hdr);

    if (note_size - offset < hdr.n_namesz) {
      return "";
    }
    if (hdr.n_namesz > 0) {
      std::string name(hdr.n_namesz, '\0');
      if (!memory->ReadFully(note_offset + offset, &(name[0]), hdr.n_namesz)) {
        return "";
      }

      // Trim trailing \0 as GNU is stored as a C string in the ELF file.
      if (name.back() == '\0') name.resize(name.size() - 1);

      // Align hdr.n_namesz to next power multiple of 4. See man 5 elf.
      offset += (hdr.n_namesz + 3) & ~3;

      if (name == "GNU" && hdr.n_type == NT_GNU_BUILD_ID) {
        if (note_size - offset < hdr.n_descsz || hdr.n_descsz == 0) {
          return "";
        }
        std::string build_id(hdr.n_descsz, '\0');
        if (memory->ReadFully(note_offset + offset, &build_id[0], hdr.n_descsz)) {
          return build_id;
        }
        return "";
      }
    }
    // Align hdr.n_descsz to next power multiple of 4. See man 5 elf.
    offset += (hdr.n_descsz + 3) & ~3;
  }
  return "";
}

// Instantiate all of the needed template functions.
template class ElfInterfaceImpl<ElfTypes32>;
template class ElfInterfaceImpl<ElfTypes64>;

template int64_t ElfInterface::GetLoadBias<Elf32_Ehdr, Elf32_Phdr>(Memory*);
template int64_t ElfInterface::GetLoadBias<Elf64_Ehdr, Elf64_Phdr>(Memory*);

template std::string ElfInterface::ReadBuildIDFromMemory<Elf32_Ehdr, Elf32_Shdr, Elf32_Nhdr>(
    Memory*);
template std::string ElfInterface::ReadBuildIDFromMemory<Elf64_Ehdr, Elf64_Shdr, Elf64_Nhdr>(
    Memory*);

}  // namespace unwindstack
