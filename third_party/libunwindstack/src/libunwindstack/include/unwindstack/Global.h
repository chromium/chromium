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

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <unwindstack/Elf.h>
#include <unwindstack/Memory.h>

namespace unwindstack {

// Forward declarations.
class Maps;
class MapInfo;

class Global {
 public:
  explicit Global(std::shared_ptr<Memory>& memory);
  Global(std::shared_ptr<Memory>& memory, std::vector<std::string>& search_libs);
  virtual ~Global() = default;

  void SetArch(ArchEnum arch);

  ArchEnum arch() { return arch_; }

 protected:
  bool Searchable(const std::string& name);
  void FindAndReadVariable(Maps* maps, const char* variable);

  virtual bool ReadVariableData(uint64_t offset) = 0;

  virtual void ProcessArch() = 0;

  ArchEnum arch_ = ARCH_UNKNOWN;

  std::shared_ptr<Memory> memory_;
  std::vector<std::string> search_libs_;
};

}  // namespace unwindstack
