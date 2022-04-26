// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/memory/partition_allocator/inspect_utils.h"

#include <sys/mman.h>

#include "base/allocator/partition_allocator/partition_alloc_base/migration_adapter.h"
#include "base/allocator/partition_allocator/thread_cache.h"
#include "base/check_op.h"
#include "base/debug/proc_maps_linux.h"
#include "base/logging.h"
#include "base/memory/page_size.h"
#include "base/strings/stringprintf.h"

#if BUILDFLAG(IS_MAC)
#include <libproc.h>
#include <mach/mach_traps.h>
#include <mach/mach_vm.h>
#endif

namespace partition_alloc::tools {

base::ScopedFD OpenProcMem(pid_t pid) {
  std::string path = base::StringPrintf("/proc/%d/mem", pid);
  int fd = open(path.c_str(), O_RDONLY);
  CHECK_NE(fd, -1)
      << "Do you have 0 set in /proc/sys/kernel/yama/ptrace_scope?";

  return base::ScopedFD(fd);
}

char* CreateMappingAtAddress(uintptr_t address, size_t size) {
  CHECK_EQ(0u, address % internal::SystemPageSize());
  CHECK_EQ(0u, size % internal::SystemPageSize());
  // Not using MAP_FIXED since it would *overwrite* an existing
  // mapping. Instead, just provide a hint address, which will be used if
  // possible.
  void* local_memory =
      mmap(reinterpret_cast<void*>(address), size, PROT_READ | PROT_WRITE,
           MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (local_memory == MAP_FAILED) {
    LOG(WARNING) << "Cannot map memory at required address";
    return nullptr;
  }
  if (local_memory != reinterpret_cast<void*>(address)) {
    LOG(WARNING) << "Mapping successful, but not at the desired address. "
                 << "Retry to get better luck with ASLR? 0x" << std::hex
                 << address << " " << local_memory << std::dec;
    munmap(local_memory, size);
    return nullptr;
  }

  return reinterpret_cast<char*>(local_memory);
}

RemoteProcessMemoryReader::RemoteProcessMemoryReader(pid_t pid) : pid_(pid) {
#if BUILDFLAG(IS_LINUX)
  mem_fd_ = OpenProcMem(pid_);
  is_valid_ = mem_fd_.get() != -1;
#elif BUILDFLAG(IS_MAC)
  kern_return_t ret = task_for_pid(mach_task_self(), pid_, &task_);
  is_valid_ = ret == KERN_SUCCESS;
#endif
}

RemoteProcessMemoryReader::~RemoteProcessMemoryReader() = default;

bool RemoteProcessMemoryReader::IsValid() const {
  return is_valid_;
}

bool RemoteProcessMemoryReader::ReadMemory(uintptr_t remote_address,
                                           size_t size,
                                           char* buffer) {
#if BUILDFLAG(IS_LINUX)
  if (HANDLE_EINTR(pread(mem_fd_.get(), buffer, size,
                         static_cast<off_t>(remote_address))) ==
      static_cast<ssize_t>(size)) {
    return true;
  }
  return false;

#elif BUILDFLAG(IS_MAC)
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
#endif
}

char* RemoteProcessMemoryReader::ReadAtSameAddressInLocalMemory(
    uintptr_t address,
    size_t size) {
  // Try to allocate data in the local address space.
  char* local_memory = CreateMappingAtAddress(address, size);
  if (!local_memory)
    return nullptr;

  bool ok = ReadMemory(address, size, reinterpret_cast<char*>(local_memory));

  if (!ok) {
    munmap(local_memory, size);
    return nullptr;
  }

  return reinterpret_cast<char*>(local_memory);
}

base::ScopedFD OpenPagemap(pid_t pid) {
#if BUILDFLAG(IS_LINUX)
  std::string path = base::StringPrintf("/proc/%d/pagemap", pid);
  int fd = open(path.c_str(), O_RDONLY);
  CHECK_NE(fd, -1)
      << "Do you have 0 set in /proc/sys/kernel/yama/ptrace_scope?";

  return base::ScopedFD(fd);
#elif BUILDFLAG(IS_MAC)
  return base::ScopedFD(-1);
#endif
}

#if BUILDFLAG(IS_LINUX)
uintptr_t IndexThreadCacheNeedleArray(RemoteProcessMemoryReader& reader,
                                      size_t index) {
  std::vector<base::debug::MappedMemoryRegion> regions;
  DCHECK_LT(index, kThreadCacheNeedleArraySize);

  {
    // Ensures that the mappings are not going to change.
    ScopedSigStopper stop{reader.pid()};

    // There are subtleties when trying to read this file, which we blissfully
    // ignore here. See //base/debug/proc_maps_linux.h for details. We don't use
    // it, since we don't read the maps for ourselves, and everything is already
    // extremely racy. At worst we have to retry.
    LOG(INFO) << "Opening /proc/PID/maps";
    std::string path = base::StringPrintf("/proc/%d/maps", reader.pid());
    auto file = base::File(base::FilePath(path),
                           base::File::FLAG_OPEN | base::File::FLAG_READ);
    CHECK(file.IsValid());
    std::vector<char> data(1e7);
    int bytes_read =
        file.ReadAtCurrentPos(&data[0], static_cast<int>(data.size()) - 1);
    CHECK_GT(bytes_read, 0) << "Cannot read " << path;
    data[bytes_read] = '\0';
    std::string proc_maps(&data[0]);

    LOG(INFO) << "Parsing the maps";
    CHECK(base::debug::ParseProcMaps(proc_maps, &regions));
    LOG(INFO) << "Found " << regions.size() << " regions";
  }

  for (auto& region : regions) {
    using base::debug::MappedMemoryRegion;

    // The array is in .data, meaning that it's mapped from the executable, and
    // has rw-p permissions. For Chrome, .data is quite small, hence the size
    // limit.
    uint8_t expected_permissions = MappedMemoryRegion::Permission::READ |
                                   MappedMemoryRegion::Permission::WRITE |
                                   MappedMemoryRegion::Permission::PRIVATE;
    size_t region_size = region.end - region.start;
    if (region.permissions != expected_permissions || region_size > 1e7 ||
        region.path.empty()) {
      continue;
    }

    LOG(INFO) << "Found a candidate region between " << std::hex << region.start
              << " and " << region.end << std::dec
              << " (size = " << region.end - region.start
              << ") path = " << region.path;
    // Scan the region, looking for the needles.
    uintptr_t needle_array_candidate[kThreadCacheNeedleArraySize];
    for (uintptr_t address = region.start;
         address < region.end - sizeof(needle_array_candidate);
         address += sizeof(uintptr_t)) {
      bool ok =
          reader.ReadMemory(reinterpret_cast<unsigned long>(address),
                            sizeof(needle_array_candidate),
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

  LOG(ERROR) << "Failed to find the address";
  return 0;
}

#elif BUILDFLAG(IS_MAC)

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
    if (info.protection != (VM_PROT_READ | VM_PROT_WRITE) ||
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
#endif

}  // namespace partition_alloc::tools
