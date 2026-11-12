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

#include <unwindstack/DwarfSection.h>
#include <unwindstack/Memory.h>

namespace unwindstack {

template <typename AddressType>
class DwarfEhFrame : public DwarfSectionImpl<AddressType> {
 public:
  DwarfEhFrame(std::shared_ptr<Memory>& memory) : DwarfSectionImpl<AddressType>(memory) {}
  virtual ~DwarfEhFrame() = default;

  uint64_t GetCieOffsetFromFde32(uint32_t pointer) override {
    return this->memory_.cur_offset() - pointer - 4;
  }

  uint64_t GetCieOffsetFromFde64(uint64_t pointer) override {
    return this->memory_.cur_offset() - pointer - 8;
  }

  uint64_t AdjustPcFromFde(uint64_t pc) override {
    // The eh_frame uses relative pcs.
    return pc + this->memory_.cur_offset() - 4;
  }
};

}  // namespace unwindstack
