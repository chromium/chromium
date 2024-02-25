// Copyright 2014 The Crashpad Authors
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

#include "util/process/process_memory_mac.h"

#include <mach/mach_vm.h>
#include <string.h>

#include <algorithm>

#include "base/apple/mach_logging.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "util/stdlib/strnlen.h"

namespace crashpad {

ProcessMemoryMac::MappedMemory::~MappedMemory() {}

bool ProcessMemoryMac::MappedMemory::ReadCString(size_t offset,
                                                 std::string* string) const {
  if (offset >= user_size_) {
    LOG(WARNING) << "offset out of range";
    return false;
  }

  const char* string_base = reinterpret_cast<const char*>(data_) + offset;
  size_t max_length = user_size_ - offset;
  size_t string_length = strnlen(string_base, max_length);
  if (string_length == max_length) {
    LOG(WARNING) << "unterminated string";
    return false;
  }

  string->assign(string_base, string_length);
  return true;
}

ProcessMemoryMac::MappedMemory::MappedMemory(vm_address_t vm_address,
                                             size_t vm_size,
                                             size_t user_offset,
                                             size_t user_size)
    : vm_(vm_address, vm_size),
      data_(reinterpret_cast<const void*>(vm_address + user_offset)),
      user_size_(user_size) {
  vm_address_t vm_end = vm_address + vm_size;
  vm_address_t user_address = reinterpret_cast<vm_address_t>(data_);
  vm_address_t user_end = user_address + user_size;
  DCHECK_GE(user_address, vm_address);
  DCHECK_LE(user_address, vm_end);
  DCHECK_GE(user_end, vm_address);
  DCHECK_LE(user_end, vm_end);
}

ProcessMemoryMac::ProcessMemoryMac() : task_(TASK_NULL), initialized_() {}

bool ProcessMemoryMac::Initialize(task_t task) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);
  task_ = task;
  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

std::unique_ptr<ProcessMemoryMac::MappedMemory> ProcessMemoryMac::ReadMapped(
    mach_vm_address_t address,
    size_t size) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  if (size == 0) {
    return std::unique_ptr<MappedMemory>(new MappedMemory(0, 0, 0, 0));
  }

  mach_vm_address_t region_address = mach_vm_trunc_page(address);
  mach_vm_size_t region_size =
      mach_vm_round_page(address - region_address + size);

  vm_offset_t region;
  mach_msg_type_number_t region_count;
  kern_return_t kr =
      mach_vm_read(task_, region_address, region_size, &region, &region_count);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(WARNING, kr) << base::StringPrintf(
        "mach_vm_read(0x%llx, 0x%llx)", region_address, region_size);
    return std::unique_ptr<MappedMemory>();
  }
  if (region_count != region_size) {
    LOG(ERROR) << base::StringPrintf(
        "mach_vm_read() unexpected read: 0x%x != 0x%llx bytes",
        region_count,
        region_size);
    if (region_count)
      vm_deallocate(mach_task_self(), region, region_count);
    return std::unique_ptr<MappedMemory>();
  }

  return std::unique_ptr<MappedMemory>(
      new MappedMemory(region, region_size, address - region_address, size));
}

ssize_t ProcessMemoryMac::ReadUpTo(VMAddress address,
                                   size_t size,
                                   void* buffer) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  DCHECK_LE(size, (size_t)std::numeric_limits<ssize_t>::max());

  std::unique_ptr<MappedMemory> memory = ReadMapped(address, size);
  if (!memory) {
    // If we can not read the entire mapping, try to perform a short read of the
    // first page instead. This is necessary to support ReadCString().
    size_t short_read = PAGE_SIZE - (address % PAGE_SIZE);
    if (short_read >= size)
      return -1;

    memory = ReadMapped(address, short_read);
    if (!memory)
      return -1;

    size = short_read;
  }

  memcpy(buffer, memory->data(), size);
  return static_cast<ssize_t>(size);
}

}  // namespace crashpad
