// Copyright 2018 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "snapshot/sanitized/sanitization_information.h"

#include <limits>

#include "client/annotation.h"

namespace crashpad {

namespace {

template <typename Pointer>
bool ReadAnnotationsWhitelist(const ProcessMemoryRange& memory,
                              VMAddress whitelist_address,
                              std::vector<std::string>* whitelist) {
  if (!whitelist_address) {
    return true;
  }

  std::vector<std::string> local_whitelist;
  Pointer name_address;
  while (memory.Read(whitelist_address, sizeof(name_address), &name_address)) {
    if (!name_address) {
      whitelist->swap(local_whitelist);
      return true;
    }

    std::string name;
    if (!memory.ReadCStringSizeLimited(
            name_address, Annotation::kNameMaxLength, &name)) {
      return false;
    }
    local_whitelist.push_back(name);
    whitelist_address += sizeof(Pointer);
  }

  return false;
}

}  // namespace

bool ReadAnnotationsWhitelist(const ProcessMemoryRange& memory,
                              VMAddress whitelist_address,
                              std::vector<std::string>* whitelist) {
  return memory.Is64Bit() ? ReadAnnotationsWhitelist<uint64_t>(
                                memory, whitelist_address, whitelist)
                          : ReadAnnotationsWhitelist<uint32_t>(
                                memory, whitelist_address, whitelist);
}

bool ReadMemoryRangeWhitelist(
    const ProcessMemoryRange& memory,
    VMAddress whitelist_address,
    std::vector<std::pair<VMAddress, VMAddress>>* whitelist) {
  whitelist->clear();
  if (!whitelist_address) {
    return true;
  }

  SanitizationMemoryRangeWhitelist list;
  if (!memory.Read(whitelist_address, sizeof(list), &list)) {
    LOG(ERROR) << "Failed to read memory range whitelist.";
    return false;
  }

  if (!list.size) {
    return true;
  }

  // An upper bound of entries that we never expect to see more than.
  constexpr size_t kMaxListSize = 256;
  if (list.size > kMaxListSize) {
    LOG(ERROR) << "Whitelist exceeded maximum, size=" << list.size;
    return false;
  }

  SanitizationMemoryRangeWhitelist::Range ranges[list.size];
  if (!memory.Read(list.entries, sizeof(ranges), &ranges)) {
    LOG(ERROR) << "Failed to read memory range whitelist entries.";
    return false;
  }

  const VMAddress vm_max = memory.Is64Bit()
                               ? std::numeric_limits<uint64_t>::max()
                               : std::numeric_limits<uint32_t>::max();
  std::vector<std::pair<VMAddress, VMAddress>> local_whitelist;
  for (size_t i = 0; i < list.size; i++) {
    if (ranges[i].base > vm_max || ranges[i].length > vm_max - ranges[i].base) {
      LOG(ERROR) << "Invalid memory range whitelist entry base="
                 << ranges[i].base << " length=" << ranges[i].length;
      return false;
    }
    local_whitelist.emplace_back(ranges[i].base,
                                 ranges[i].base + ranges[i].length);
  }

  whitelist->swap(local_whitelist);
  return true;
}

}  // namespace crashpad
