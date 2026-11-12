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

#include <vector>

#include <unwindstack/Memory.h>

namespace unwindstack {

class MemoryBuffer : public Memory {
 public:
  // If a size is too large, assume it's likely corrupted data, and set to zero.
  MemoryBuffer(size_t size) : raw_(size > kMaxBufferSize ? 0 : size), offset_(0) {}
  MemoryBuffer(size_t size, uint64_t offset)
      : raw_(size > kMaxBufferSize ? 0 : size), offset_(offset) {}
  virtual ~MemoryBuffer() = default;

  size_t Read(uint64_t addr, void* dst, size_t size) override;

  uint8_t* GetPtr(size_t offset) override;

  uint8_t* Data() { return raw_.data(); }
  uint64_t Size() { return raw_.size(); }

 private:
  std::vector<uint8_t> raw_;
  uint64_t offset_;

  // This class is only used for global data and a compressed .debug_frame in
  // the library code. The limit of 10MB is way over what any valid existing
  // globals data section is expected to be. A 50MB shared library only contains
  // a .debug_frame that is < 100KB in size. Therefore, 10MB should be able to
  // handle any valid large shared library with a valid large .debug_frame.
  static constexpr size_t kMaxBufferSize = 10 * 1024 * 1024;
};

}  // namespace unwindstack
