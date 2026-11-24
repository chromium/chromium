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

#include <memory>
#include <string>
#include <vector>

#include <unwindstack/Memory.h>

#include "MemoryRange.h"

namespace unwindstack {

class MemoryOffline : public Memory {
 public:
  MemoryOffline() = default;
  virtual ~MemoryOffline() = default;

  bool Init(const std::string& file, uint64_t offset);

  bool Init(const std::string& file, uint64_t offset, uint64_t start, uint64_t size);

  size_t Read(uint64_t addr, void* dst, size_t size) override;

 private:
  std::unique_ptr<MemoryRange> memory_;
};

class MemoryOfflineParts : public Memory {
 public:
  MemoryOfflineParts() = default;
  virtual ~MemoryOfflineParts();

  void Add(MemoryOffline* memory) { memories_.push_back(memory); }

  size_t Read(uint64_t addr, void* dst, size_t size) override;

 private:
  std::vector<MemoryOffline*> memories_;
};

}  // namespace unwindstack
