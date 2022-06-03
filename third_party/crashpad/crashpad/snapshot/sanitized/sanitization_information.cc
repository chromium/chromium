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

#include "base/logging.h"
#include "client/annotation.h"

namespace crashpad {

namespace {

template <typename Pointer>
bool ReadAllowedAnnotations(const ProcessMemoryRange& memory,
                            VMAddress list_address,
                            std::vector<std::string>* allowed_annotations) {
  if (!list_address) {
    return true;
  }

  std::vector<std::string> local_allowed_annotations;
  Pointer name_address;
  while (memory.Read(list_address, sizeof(name_address), &name_address)) {
    if (!name_address) {
      allowed_annotations->swap(local_allowed_annotations);
      return true;
    }

    std::string name;
    if (!memory.ReadCStringSizeLimited(
            name_address, Annotation::kNameMaxLength, &name)) {
      return false;
    }
    local_allowed_annotations.push_back(name);
    list_address += sizeof(Pointer);
  }

  return false;
}

}  // namespace

bool ReadAllowedAnnotations(const ProcessMemoryRange& memory,
                            VMAddress list_address,
                            std::vector<std::string>* allowed_annotations) {
  return memory.Is64Bit() ? ReadAllowedAnnotations<uint64_t>(
                                memory, list_address, allowed_annotations)
                          : ReadAllowedAnnotations<uint32_t>(
                                memory, list_address, allowed_annotations);
}

bool ReadAllowedMemoryRanges(
    const ProcessMemoryRange& memory,
    VMAddress list_address,
    std::vector<std::pair<VMAddress, VMAddress>>* allowed_memory_ranges) {
  allowed_memory_ranges->clear();
  if (!list_address) {
    return true;
  }

  SanitizationAllowedMemoryRanges list;
  if (!memory.Read(list_address, sizeof(list), &list)) {
    LOG(ERROR) << "Failed to read allowed memory ranges";
    return false;
  }

  if (!list.size) {
    return true;
  }

  // An upper bound of entries that we never expect to see more than.
  constexpr size_t kMaxListSize = 256;
  if (list.size > kMaxListSize) {
    LOG(ERROR) << "list exceeded maximum, size=" << list.size;
    return false;
  }

  std::vector<SanitizationAllowedMemoryRanges::Range> ranges(list.size);
  if (!memory.Read(list.entries, sizeof(ranges[0]) * list.size,
                   ranges.data())) {
    LOG(ERROR) << "Failed to read allowed memory ranges";
    return false;
  }

  const VMAddress vm_max = memory.Is64Bit()
                               ? std::numeric_limits<uint64_t>::max()
                               : std::numeric_limits<uint32_t>::max();
  std::vector<std::pair<VMAddress, VMAddress>> local_allowed_memory_ranges;
  for (size_t i = 0; i < list.size; ++i) {
    if (ranges[i].base > vm_max || ranges[i].length > vm_max - ranges[i].base) {
      LOG(ERROR) << "Invalid range: base=" << ranges[i].base
                 << " length=" << ranges[i].length;
      return false;
    }
    local_allowed_memory_ranges.emplace_back(ranges[i].base,
                                             ranges[i].base + ranges[i].length);
  }

  allowed_memory_ranges->swap(local_allowed_memory_ranges);
  return true;
}

}  // namespace crashpad
