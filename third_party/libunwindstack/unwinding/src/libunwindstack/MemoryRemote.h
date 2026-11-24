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
#include <sys/types.h>

#include <atomic>

#include <unwindstack/Memory.h>

namespace unwindstack {

class MemoryRemote : public Memory {
 public:
  MemoryRemote(pid_t pid) : pid_(pid), read_redirect_func_(0) {}
  virtual ~MemoryRemote() = default;

  size_t Read(uint64_t addr, void* dst, size_t size) override;
  long ReadTag(uint64_t addr) override;

  pid_t pid() { return pid_; }

 private:
  pid_t pid_;
  std::atomic_uintptr_t read_redirect_func_;
};

}  // namespace unwindstack
