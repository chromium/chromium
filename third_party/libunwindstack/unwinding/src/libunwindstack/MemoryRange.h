/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <map>
#include <memory>
#include <string>

#include <unwindstack/Memory.h>

namespace unwindstack {

// MemoryRange maps one address range onto another.
// The range [src_begin, src_begin + length) in the underlying Memory is mapped onto offset,
// such that range.read(offset) is equivalent to underlying.read(src_begin).
class MemoryRange : public Memory {
 public:
  MemoryRange(const std::shared_ptr<Memory>& memory, uint64_t begin, uint64_t length,
              uint64_t offset);
  virtual ~MemoryRange() = default;

  size_t Read(uint64_t addr, void* dst, size_t size) override;

  uint64_t offset() { return offset_; }
  uint64_t length() { return length_; }

 private:
  std::shared_ptr<Memory> memory_;
  uint64_t begin_;
  uint64_t length_;
  uint64_t offset_;
};

class MemoryRanges : public Memory {
 public:
  MemoryRanges() = default;
  virtual ~MemoryRanges() = default;

  bool Insert(MemoryRange* memory);

  size_t Read(uint64_t addr, void* dst, size_t size) override;

 private:
  std::map<uint64_t, std::unique_ptr<MemoryRange>> maps_;
};

}  // namespace unwindstack
