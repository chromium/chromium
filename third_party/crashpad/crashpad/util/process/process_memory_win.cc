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

#include "util/process/process_memory_win.h"

#include <windows.h>

#include <algorithm>
#include <limits>

#include "base/logging.h"
#include "base/memory/page_size.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"

namespace crashpad {

ProcessMemoryWin::ProcessMemoryWin()
    : ProcessMemory(), handle_(), process_info_(), initialized_() {}

ProcessMemoryWin::~ProcessMemoryWin() {}

bool ProcessMemoryWin::Initialize(HANDLE handle) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  handle_ = handle;
  if (!process_info_.Initialize(handle)) {
    LOG(ERROR) << "Failed to initialize ProcessInfo.";
    return false;
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

ssize_t ProcessMemoryWin::ReadUpTo(VMAddress address,
                                   size_t size,
                                   void* buffer) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  DCHECK_LE(size, (size_t)std::numeric_limits<ssize_t>::max());

  SIZE_T size_out = 0;
  BOOL success = ReadProcessMemory(
      handle_, reinterpret_cast<void*>(address), buffer, size, &size_out);
  if (success)
    return base::checked_cast<ssize_t>(size_out);

  if (GetLastError() == ERROR_PARTIAL_COPY) {
    // If we can not read the entire section, perform a short read of the first
    // page instead. This is necessary to support ReadCString().
    size_t short_read =
        base::GetPageSize() - (address & (base::GetPageSize() - 1));
    success = ReadProcessMemory(handle_,
                                reinterpret_cast<void*>(address),
                                buffer,
                                short_read,
                                &size_out);
    if (success)
      return base::checked_cast<ssize_t>(size_out);
  }

  PLOG(ERROR) << "ReadMemory at 0x" << std::hex << address << std::dec << " of "
              << size << " bytes failed";
  return -1;
}

size_t ProcessMemoryWin::ReadAvailableMemory(VMAddress address,
                                             size_t size,
                                             void* buffer) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  DCHECK_LE(size, (size_t)std::numeric_limits<ssize_t>::max());

  if (size == 0)
    return 0;

  auto ranges = process_info_.GetReadableRanges(
      CheckedRange<WinVMAddress, WinVMSize>(address, size));

  // We only read up until the first unavailable byte, so we only read from the
  // first range. If we have no ranges, then no bytes were accessible anywhere
  // in the range.
  if (ranges.empty()) {
    LOG(ERROR) << base::StringPrintf(
        "range at 0x%llx, size 0x%zx completely inaccessible", address, size);
    return 0;
  }

  // If the start address was adjusted, we couldn't read even the first
  // requested byte.
  if (ranges.front().base() != address) {
    LOG(ERROR) << base::StringPrintf(
        "start of range at 0x%llx, size 0x%zx inaccessible", address, size);
    return 0;
  }

  DCHECK_LE(ranges.front().size(), size);

  ssize_t result = ReadUpTo(ranges.front().base(),
                            base::checked_cast<size_t>(ranges.front().size()),
                            buffer);
  if (result < 0)
    return 0;

  return base::checked_cast<size_t>(result);
}

}  // namespace crashpad
