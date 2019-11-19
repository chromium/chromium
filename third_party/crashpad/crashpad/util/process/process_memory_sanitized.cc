// Copyright 2019 The Crashpad Authors. All rights reserved.
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

#include "util/process/process_memory_sanitized.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <limits>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"

namespace crashpad {

ProcessMemorySanitized::ProcessMemorySanitized()
    : ProcessMemory(), memory_(nullptr), whitelist_() {}

ProcessMemorySanitized::~ProcessMemorySanitized() {}

bool ProcessMemorySanitized::Initialize(
    const ProcessMemory* memory,
    const std::vector<std::pair<VMAddress, VMAddress>>* whitelist) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);
  memory_ = memory;
  if (whitelist)
    whitelist_ = *whitelist;
  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

ssize_t ProcessMemorySanitized::ReadUpTo(VMAddress address,
                                         size_t size,
                                         void* buffer) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  VMAddress end = address + size;
  for (auto&& entry : whitelist_) {
    if (address >= entry.first && address < entry.second &&
        end >= entry.first && end <= entry.second) {
      return memory_->ReadUpTo(address, size, buffer);
    }
  }

  DLOG(ERROR)
      << "ProcessMemorySanitized failed to read unwhitelisted region. address="
      << address << " size=" << size;
  return 0;
}

}  // namespace crashpad
