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

#include <stdint.h>
#include <memory>

#include <unwindstack/Maps.h>

namespace unwindstack {

// Base class for architecture specific implementations (see "GlobalDebugImpl.h").
// It provides access to JITed ELF files, and loaded DEX files in the ART runtime.
template <typename Symfile>
class GlobalDebugInterface {
 public:
  virtual ~GlobalDebugInterface() {}

  virtual bool GetFunctionName(Maps* maps, uint64_t pc, SharedString* name, uint64_t* offset) = 0;

  virtual Symfile* Find(Maps* maps, uint64_t pc) = 0;

 protected:
  bool Load(Maps* maps, std::shared_ptr<Memory>& memory, uint64_t addr, uint64_t size,
            /*out*/ std::shared_ptr<Symfile>& dex);
};

}  // namespace unwindstack
