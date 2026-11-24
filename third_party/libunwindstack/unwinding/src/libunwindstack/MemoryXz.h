/*
 * Copyright (C) 2021 The Android Open Source Project
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

#include <atomic>
#include <memory>
#include <vector>

#include <unwindstack/Memory.h>

namespace unwindstack {

class MemoryXz : public Memory {
 public:
  MemoryXz(Memory* memory, uint64_t addr, uint64_t size, const std::string& name);
  ~MemoryXz();

  bool Init();
  size_t Size() { return size_; }
  size_t Read(uint64_t addr, void* dst, size_t size) override;

  // Methods used in tests.
  size_t MemoryUsage() { return used_; }
  size_t BlockCount() { return blocks_.size(); }
  size_t BlockSize() { return 1 << block_size_log2_; }

 private:
  static constexpr size_t kMaxCompressedSize = 1 << 30;  // 1GB. Arbitrary.

  struct XzBlock {
    std::unique_ptr<uint8_t[]> decompressed_data;
    uint32_t decompressed_size;
    uint32_t compressed_offset;
    uint32_t compressed_size;
    uint16_t stream_flags;
  };
  bool ReadBlocks();
  bool Decompress(XzBlock* block);

  // Compressed input.
  Memory* compressed_memory_;
  uint64_t compressed_addr_;
  uint64_t compressed_size_;
  std::string name_;

  // Decompressed output.
  std::vector<XzBlock> blocks_;
  uint32_t used_ = 0;  // Memory usage of the currently decompressed blocks.
  uint32_t size_ = 0;  // Decompressed size of all blocks.
  uint32_t block_size_log2_ = 31;

  // Statistics (used only for optional debug log messages).
  static std::atomic_size_t total_used_;  // Currently decompressed memory (current memory use).
  static std::atomic_size_t total_size_;  // Size of mini-debug-info if it was all decompressed.
  static std::atomic_size_t total_open_;  // Number of mini-debug-info files currently in use.
};

}  // namespace unwindstack
