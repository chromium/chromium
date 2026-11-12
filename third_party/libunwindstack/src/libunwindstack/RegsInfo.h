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
#include <stdlib.h>

#include <unwindstack/Regs.h>

namespace unwindstack {

template <typename AddressType>
struct RegsInfo {
  static constexpr size_t MAX_REGISTERS = 64;

  RegsInfo(RegsImpl<AddressType>* regs) : regs(regs) {}

  RegsImpl<AddressType>* regs = nullptr;
  uint64_t saved_reg_map = 0;
  AddressType saved_regs[MAX_REGISTERS];

  inline AddressType Get(uint32_t reg) {
    if (IsSaved(reg)) {
      return saved_regs[reg];
    }
    return (*regs)[reg];
  }

  inline AddressType* Save(uint32_t reg) {
    if (reg >= MAX_REGISTERS) {
      // This should never happen since all currently supported
      // architectures have < 64 total registers.
      abort();
    }
    saved_reg_map |= 1ULL << reg;
    saved_regs[reg] = (*regs)[reg];
    return &(*regs)[reg];
  }

  inline bool IsSaved(uint32_t reg) {
    if (reg > MAX_REGISTERS) {
      // This should never happen since all currently supported
      // architectures have < 64 total registers.
      abort();
    }
    return saved_reg_map & (1ULL << reg);
  }

  inline uint16_t Total() { return regs->total_regs(); }
};

}  // namespace unwindstack
