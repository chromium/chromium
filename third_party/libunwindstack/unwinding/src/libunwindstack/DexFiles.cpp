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

#include <unwindstack/DexFiles.h>

#if defined(DEXFILE_SUPPORT)
#include "DexFile.h"
#endif

#include "GlobalDebugImpl.h"

namespace unwindstack {

#if defined(DEXFILE_SUPPORT)

template <>
bool GlobalDebugInterface<DexFile>::Load(Maps* maps, std::shared_ptr<Memory>& memory, uint64_t addr,
                                         uint64_t size, /*out*/ std::shared_ptr<DexFile>& dex) {
  dex = DexFile::Create(addr, size, memory.get(), maps->Find(addr).get());
  return dex.get() != nullptr;
}

std::unique_ptr<DexFiles> CreateDexFiles(ArchEnum arch, std::shared_ptr<Memory>& memory,
                                         std::vector<std::string> search_libs) {
  return CreateGlobalDebugImpl<DexFile>(arch, memory, search_libs, "__dex_debug_descriptor");
}

#else

template <>
bool GlobalDebugInterface<DexFile>::Load(Maps*, std::shared_ptr<Memory>&, uint64_t, uint64_t,
                                         std::shared_ptr<DexFile>&) {
  return false;
}

std::unique_ptr<DexFiles> CreateDexFiles(ArchEnum, std::shared_ptr<Memory>&,
                                         std::vector<std::string>) {
  return nullptr;
}

#endif

}  // namespace unwindstack
