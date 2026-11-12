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

#include <unwindstack/DwarfError.h>
#include <unwindstack/DwarfStructs.h>
#include <unwindstack/Elf.h>
#include <unwindstack/ElfInterface.h>
#include <unwindstack/Memory.h>

#include "Check.h"
#include "DwarfEhFrameWithHdr.h"
#include "DwarfEncoding.h"

namespace unwindstack {

static inline bool IsEncodingRelative(uint8_t encoding) {
  encoding >>= 4;
  return encoding > 0 && encoding <= DW_EH_PE_funcrel;
}

template <typename AddressType>
bool DwarfEhFrameWithHdr<AddressType>::EhFrameInit(const SectionInfo& info) {
  return DwarfSectionImpl<AddressType>::Init(info);
}

template <typename AddressType>
bool DwarfEhFrameWithHdr<AddressType>::Init(const SectionInfo& info) {
  if (info.flags & SHF_COMPRESSED) {
    return false;
  }

  memory_.clear_func_offset();
  memory_.clear_text_offset();
  memory_.set_data_offset(info.offset);
  memory_.set_cur_offset(info.offset);

  hdr_section_bias_ = info.bias;

  // Read the first four bytes all at once.
  uint8_t data[4];
  if (!memory_.ReadBytes(data, 4)) {
    last_error_.code = DWARF_ERROR_MEMORY_INVALID;
    last_error_.address = memory_.cur_offset();
    return false;
  }

  version_ = data[0];
  if (version_ != 1) {
    // Unknown version.
    last_error_.code = DWARF_ERROR_UNSUPPORTED_VERSION;
    return false;
  }

  uint8_t ptr_encoding = data[1];
  uint8_t fde_count_encoding = data[2];
  table_encoding_ = data[3];
  table_entry_size_ = memory_.template GetEncodedSize<AddressType>(table_encoding_);

  // If we can't perform a binary search on the entries, it's not worth
  // using this object. The calling code will fall back to the DwarfEhFrame
  // object in this case.
  if (table_entry_size_ == 0) {
    last_error_.code = DWARF_ERROR_ILLEGAL_VALUE;
    return false;
  }

  memory_.set_pc_offset(memory_.cur_offset());
  uint64_t ptr_offset;
  if (!memory_.template ReadEncodedValue<AddressType>(ptr_encoding, &ptr_offset)) {
    last_error_.code = DWARF_ERROR_MEMORY_INVALID;
    last_error_.address = memory_.cur_offset();
    return false;
  }

  memory_.set_pc_offset(memory_.cur_offset());
  if (!memory_.template ReadEncodedValue<AddressType>(fde_count_encoding, &fde_count_)) {
    last_error_.code = DWARF_ERROR_MEMORY_INVALID;
    last_error_.address = memory_.cur_offset();
    return false;
  }

  if (fde_count_ == 0) {
    last_error_.code = DWARF_ERROR_NO_FDES;
    return false;
  }

  hdr_entries_offset_ = memory_.cur_offset();
  hdr_entries_data_offset_ = info.offset;

  return true;
}

template <typename AddressType>
const DwarfFde* DwarfEhFrameWithHdr<AddressType>::GetFdeFromPc(uint64_t pc) {
  uint64_t fde_offset;
  if (!GetFdeOffsetFromPc(pc, &fde_offset)) {
    return nullptr;
  }
  const DwarfFde* fde = this->GetFdeFromOffset(fde_offset);
  if (fde == nullptr) {
    return nullptr;
  }

  // There is a possibility that this entry points to a zero length FDE
  // due to a bug. If this happens, try and find the non-zero length FDE
  // from eh_frame directly. See b/142483624.
  if (fde->pc_start == fde->pc_end) {
    fde = DwarfSectionImpl<AddressType>::GetFdeFromPc(pc);
    if (fde == nullptr) {
      return nullptr;
    }
  }

  // Guaranteed pc >= pc_start, need to check pc in the fde range.
  if (pc < fde->pc_end) {
    return fde;
  }
  last_error_.code = DWARF_ERROR_ILLEGAL_STATE;
  return nullptr;
}

template <typename AddressType>
const typename DwarfEhFrameWithHdr<AddressType>::FdeInfo*
DwarfEhFrameWithHdr<AddressType>::GetFdeInfoFromIndex(size_t index) {
  auto entry = fde_info_.find(index);
  if (entry != fde_info_.end()) {
    return &fde_info_[index];
  }
  FdeInfo* info = &fde_info_[index];

  memory_.set_data_offset(hdr_entries_data_offset_);
  memory_.set_cur_offset(hdr_entries_offset_ + 2 * index * table_entry_size_);
  memory_.set_pc_offset(0);
  uint64_t value;
  if (!memory_.template ReadEncodedValue<AddressType>(table_encoding_, &value) ||
      !memory_.template ReadEncodedValue<AddressType>(table_encoding_, &info->offset)) {
    last_error_.code = DWARF_ERROR_MEMORY_INVALID;
    last_error_.address = memory_.cur_offset();
    fde_info_.erase(index);
    return nullptr;
  }

  // Relative encodings require adding in the load bias.
  if (IsEncodingRelative(table_encoding_)) {
    value += hdr_section_bias_;
  }
  info->pc = value;
  return info;
}

template <typename AddressType>
bool DwarfEhFrameWithHdr<AddressType>::GetFdeOffsetFromPc(uint64_t pc, uint64_t* fde_offset) {
  if (fde_count_ == 0) {
    return false;
  }

  size_t first = 0;
  size_t last = fde_count_;
  while (first < last) {
    size_t current = (first + last) / 2;
    const FdeInfo* info = GetFdeInfoFromIndex(current);
    if (info == nullptr) {
      return false;
    }
    if (pc == info->pc) {
      *fde_offset = info->offset;
      return true;
    }
    if (pc < info->pc) {
      last = current;
    } else {
      first = current + 1;
    }
  }
  if (last != 0) {
    const FdeInfo* info = GetFdeInfoFromIndex(last - 1);
    if (info == nullptr) {
      return false;
    }
    *fde_offset = info->offset;
    return true;
  }
  return false;
}

template <typename AddressType>
void DwarfEhFrameWithHdr<AddressType>::GetFdes(std::vector<const DwarfFde*>* fdes) {
  for (size_t i = 0; i < fde_count_; i++) {
    const FdeInfo* info = GetFdeInfoFromIndex(i);
    if (info == nullptr) {
      break;
    }
    const DwarfFde* fde = this->GetFdeFromOffset(info->offset);
    if (fde == nullptr) {
      break;
    }

    // There is a possibility that this entry points to a zero length FDE
    // due to a bug. If this happens, try and find the non-zero length FDE
    // from eh_frame directly. See b/142483624.
    if (fde->pc_start == fde->pc_end) {
      const DwarfFde* fde_real = DwarfSectionImpl<AddressType>::GetFdeFromPc(fde->pc_start);
      if (fde_real != nullptr) {
        fde = fde_real;
      }
    }
    fdes->push_back(fde);
  }
}

// Explicitly instantiate DwarfEhFrameWithHdr
template class DwarfEhFrameWithHdr<uint32_t>;
template class DwarfEhFrameWithHdr<uint64_t>;

}  // namespace unwindstack
