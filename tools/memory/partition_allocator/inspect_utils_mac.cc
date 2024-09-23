// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/memory/partition_allocator/inspect_utils.h"

#include <libproc.h>
#include <mach/mach_traps.h>
#include <mach/mach_vm.h>
#include <sys/mman.h>

#include "base/check_op.h"
#include "base/debug/proc_maps_linux.h"
#include "base/logging.h"
#include "base/memory/page_size.h"
#include "partition_alloc/thread_cache.h"

namespace partition_alloc::tools {

RemoteProcessMemoryReader::RemoteProcessMemoryReader(pid_t pid) : pid_(pid) {
  kern_return_t ret = task_for_pid(mach_task_self(), pid_, &task_);
  is_valid_ = ret == KERN_SUCCESS;
}

bool RemoteProcessMemoryReader::ReadMemory(uintptr_t remote_address,
                                           size_t size,
                                           char* buffer) {
  mach_vm_size_t read_bytes = size;
  kern_return_t ret = mach_vm_read_overwrite(
      task_, remote_address, size, reinterpret_cast<mach_vm_address_t>(buffer),
      &read_bytes);
  if (ret != KERN_SUCCESS) {
    // Try to read page by page.
    //
    // It seems that mach_vm_read() doesn't work when the target mapping is not
    // readable. Since superpages always have at least a couple of guard pages,
    // we need to read page by page.
    size_t page_count = size / base::GetPageSize();
    CHECK_EQ(0u, size % base::GetPageSize());

    size_t read_pages = 0;
    size_t page_size = base::GetPageSize();
    for (size_t i = 0; i < page_count; i++) {
      size_t offset = i * page_size;
      auto target_address =
          reinterpret_cast<mach_vm_address_t>(buffer + offset);
      auto source_address = remote_address + offset;

      ret = mach_vm_read_overwrite(task_, source_address, page_size,
                                   target_address, &read_bytes);
      if (ret == KERN_SUCCESS)
        read_pages++;
    }

    LOG(WARNING) << "Couldn't read all pages. Page count = " << page_count
                 << " Read count = " << read_pages;
    return read_pages != 0;
  }

  return ret == KERN_SUCCESS;
}

base::ScopedFD OpenPagemap(pid_t pid) {
  // Not supported.
  return base::ScopedFD(-1);
}

uintptr_t IndexThreadCacheNeedleArray(RemoteProcessMemoryReader& reader,
                                      size_t index) {
  task_t task;
  kern_return_t ret = task_for_pid(mach_task_self(), reader.pid(), &task);
  CHECK_EQ(ret, KERN_SUCCESS)
      << "Is the binary signed? codesign --force --deep -s - "
      << "out/Default/pa_tcache_inspect to sign it";

  mach_vm_address_t address = 0;
  mach_vm_size_t size = 0;

  while (true) {
    address += size;

    vm_region_extended_info_data_t info;
    mach_port_t object_name;
    mach_msg_type_number_t count;

    count = VM_REGION_EXTENDED_INFO_COUNT;
    ret = mach_vm_region(task, &address, &size, VM_REGION_EXTENDED_INFO,
                         reinterpret_cast<vm_region_info_t>(&info), &count,
                         &object_name);
    if (ret != KERN_SUCCESS) {
      LOG(ERROR) << "Cannot read region";
      return 0;
    }

    // The needle is in the .data region, which is mapped Copy On Write from the
    // binary, and is Readable and Writable.
    if ((info.protection != (VM_PROT_READ | VM_PROT_WRITE)) ||
        (info.share_mode != SM_COW))
      continue;

    char buf[PATH_MAX];
    int len = proc_regionfilename(reader.pid(), address, buf, sizeof(buf));
    buf[len] = '\0';

    // Should be in the framework, not the launcher binary.
    if (len == 0 || !strstr(buf, "Chromium Framework"))
      continue;

    // We have a candidate, let's look into it.
    LOG(INFO) << "Found a candidate region between " << std::hex << address
              << " and " << address + size << std::dec << " (size = " << size
              << ") path = " << buf;

    // Scan the region, looking for the needles.
    uintptr_t needle_array_candidate[kThreadCacheNeedleArraySize];
    for (uintptr_t addr = address;
         addr < address + size - sizeof(needle_array_candidate);
         addr += sizeof(uintptr_t)) {
      bool ok = reader.ReadMemory(
          reinterpret_cast<unsigned long>(addr), sizeof(needle_array_candidate),
          reinterpret_cast<char*>(needle_array_candidate));
      if (!ok) {
        LOG(WARNING) << "Failed to read";
        continue;
      }

      if (needle_array_candidate[0] == kNeedle1 &&
          needle_array_candidate[kThreadCacheNeedleArraySize - 1] == kNeedle2) {
        LOG(INFO) << "Got it! Address = 0x" << std::hex
                  << needle_array_candidate[index];
        return needle_array_candidate[index];
      }
    }
  }
}

}  // namespace partition_alloc::tools
