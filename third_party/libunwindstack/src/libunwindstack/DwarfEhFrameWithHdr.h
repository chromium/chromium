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

#include <memory>
#include <unordered_map>
#include <vector>

#include <unwindstack/DwarfSection.h>

namespace unwindstack {

// Forward declarations.
class Memory;
struct SectionInfo;

template <typename AddressType>
class DwarfEhFrameWithHdr : public DwarfSectionImpl<AddressType> {
 public:
  // Add these so that the protected members of DwarfSectionImpl
  // can be accessed without needing a this->.
  using DwarfSectionImpl<AddressType>::memory_;
  using DwarfSectionImpl<AddressType>::last_error_;

  struct FdeInfo {
    AddressType pc;
    uint64_t offset;
  };

  DwarfEhFrameWithHdr(std::shared_ptr<Memory>& memory) : DwarfSectionImpl<AddressType>(memory) {}
  virtual ~DwarfEhFrameWithHdr() = default;

  uint64_t GetCieOffsetFromFde32(uint32_t pointer) override {
    return memory_.cur_offset() - pointer - 4;
  }

  uint64_t GetCieOffsetFromFde64(uint64_t pointer) override {
    return memory_.cur_offset() - pointer - 8;
  }

  uint64_t AdjustPcFromFde(uint64_t pc) override {
    // The eh_frame uses relative pcs.
    return pc + memory_.cur_offset() - 4;
  }

  bool EhFrameInit(const SectionInfo& info);
  bool Init(const SectionInfo& info) override;

  const DwarfFde* GetFdeFromPc(uint64_t pc) override;

  bool GetFdeOffsetFromPc(uint64_t pc, uint64_t* fde_offset);

  const FdeInfo* GetFdeInfoFromIndex(size_t index);

  void GetFdes(std::vector<const DwarfFde*>* fdes) override;

 protected:
  uint8_t version_ = 0;
  uint8_t table_encoding_ = 0;
  size_t table_entry_size_ = 0;

  uint64_t hdr_entries_offset_ = 0;
  uint64_t hdr_entries_data_offset_ = 0;
  uint64_t hdr_section_bias_ = 0;

  uint64_t fde_count_ = 0;
  std::unordered_map<uint64_t, FdeInfo> fde_info_;
};

}  // namespace unwindstack
