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

#pragma once

#include <stdint.h>

#include <memory>

#include <unwindstack/DwarfSection.h>

namespace unwindstack {

template <typename AddressType>
class DwarfDebugFrame : public DwarfSectionImpl<AddressType> {
 public:
  DwarfDebugFrame(std::shared_ptr<Memory>& memory) : DwarfSectionImpl<AddressType>(memory) {
    this->cie32_value_ = static_cast<uint32_t>(-1);
    this->cie64_value_ = static_cast<uint64_t>(-1);
  }
  virtual ~DwarfDebugFrame() = default;

  uint64_t GetCieOffsetFromFde32(uint32_t pointer) override {
    return this->entries_offset_ + pointer;
  }

  uint64_t GetCieOffsetFromFde64(uint64_t pointer) override {
    return this->entries_offset_ + pointer;
  }

  uint64_t AdjustPcFromFde(uint64_t pc) override { return pc; }
};

}  // namespace unwindstack
