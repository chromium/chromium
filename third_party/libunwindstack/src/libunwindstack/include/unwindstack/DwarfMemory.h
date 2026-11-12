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

namespace unwindstack {

// Forward declarations.
class Memory;

class DwarfMemory {
 public:
  DwarfMemory(std::shared_ptr<Memory>& memory) : memory_(memory) {}
  virtual ~DwarfMemory() = default;

  bool ReadBytes(void* dst, size_t num_bytes);

  template <typename SignedType>
  bool ReadSigned(uint64_t* value);

  bool ReadULEB128(uint64_t* value);

  bool ReadSLEB128(int64_t* value);

  template <typename AddressType>
  size_t GetEncodedSize(uint8_t encoding);

  bool AdjustEncodedValue(uint8_t encoding, uint64_t* value);

  template <typename AddressType>
  bool ReadEncodedValue(uint8_t encoding, uint64_t* value);

  uint64_t cur_offset() { return cur_offset_; }
  void set_cur_offset(uint64_t cur_offset) { cur_offset_ = cur_offset; }

  void set_pc_offset(int64_t offset) { pc_offset_ = offset; }
  void clear_pc_offset() { pc_offset_ = INT64_MAX; }

  void set_data_offset(uint64_t offset) { data_offset_ = offset; }
  void clear_data_offset() { data_offset_ = static_cast<uint64_t>(-1); }

  void set_func_offset(uint64_t offset) { func_offset_ = offset; }
  void clear_func_offset() { func_offset_ = static_cast<uint64_t>(-1); }

  void set_text_offset(uint64_t offset) { text_offset_ = offset; }
  void clear_text_offset() { text_offset_ = static_cast<uint64_t>(-1); }

 private:
  std::shared_ptr<Memory> memory_;
  uint64_t cur_offset_ = 0;

  int64_t pc_offset_ = INT64_MAX;
  uint64_t data_offset_ = static_cast<uint64_t>(-1);
  uint64_t func_offset_ = static_cast<uint64_t>(-1);
  uint64_t text_offset_ = static_cast<uint64_t>(-1);
};

}  // namespace unwindstack
