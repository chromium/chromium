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

#include <vector>

namespace unwindstack {

struct DwarfCie {
  uint8_t version = 0;
  uint8_t fde_address_encoding = 0;
  uint8_t lsda_encoding = 0;
  uint8_t segment_size = 0;
  std::vector<char> augmentation_string;
  uint64_t personality_handler = 0;
  uint64_t cfa_instructions_offset = 0;
  uint64_t cfa_instructions_end = 0;
  uint64_t code_alignment_factor = 0;
  int64_t data_alignment_factor = 0;
  uint64_t return_address_register = 0;
  bool is_signal_frame = false;
};

struct DwarfFde {
  uint64_t cie_offset = 0;
  uint64_t cfa_instructions_offset = 0;
  uint64_t cfa_instructions_end = 0;
  uint64_t pc_start = 0;
  uint64_t pc_end = 0;
  uint64_t lsda_address = 0;
  const DwarfCie* cie = nullptr;
};

constexpr uint16_t CFA_REG = static_cast<uint16_t>(-1);

}  // namespace unwindstack
