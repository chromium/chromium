// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "reference_drivers/memory.h"

#include <mach/mach_vm.h>

#include "reference_drivers/os_handle.h"
#include "third_party/abseil-cpp/absl/base/macros.h"

namespace ipcz::reference_drivers {

void Memory::Mapping::Reset() {
  if (base_address_) {
    kern_return_t kr = mach_vm_deallocate(
        mach_task_self(), reinterpret_cast<mach_vm_address_t>(base_address_),
        size_);
    ABSL_ASSERT(kr == KERN_SUCCESS);
  }
}

Memory::Memory(size_t size) {
  mach_vm_size_t vm_size = size;
  mach_port_t named_right;
  kern_return_t kr = mach_make_memory_entry_64(
      mach_task_self(), &vm_size, 0,
      MAP_MEM_NAMED_CREATE | VM_PROT_READ | VM_PROT_WRITE, &named_right,
      MACH_PORT_NULL);
  ABSL_ASSERT(kr == KERN_SUCCESS);
  ABSL_ASSERT(vm_size >= size);
  handle_ = OSHandle(OSHandle::MachSendRight(named_right));
  size_ = size;
}

Memory::Mapping Memory::Map() {
  ABSL_ASSERT(is_valid());
  mach_vm_address_t address = 0;
  kern_return_t kr = mach_vm_map(mach_task_self(), &address, size_, 0,
                                 VM_FLAGS_ANYWHERE, handle_.mach_send_right(),
                                 0, FALSE, VM_PROT_READ | VM_PROT_WRITE,
                                 VM_PROT_READ | VM_PROT_WRITE, VM_INHERIT_NONE);
  ABSL_ASSERT(kr == KERN_SUCCESS);
  return Mapping(reinterpret_cast<void*>(address), size_);
}

}  // namespace ipcz::reference_drivers
