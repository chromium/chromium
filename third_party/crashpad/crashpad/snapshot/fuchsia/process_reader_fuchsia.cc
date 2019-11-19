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

#include "snapshot/fuchsia/process_reader_fuchsia.h"

#include <lib/zx/thread.h>
#include <link.h>
#include <zircon/syscalls.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "util/fuchsia/koid_utilities.h"

namespace crashpad {

namespace {

// Based on the thread's SP and the process's memory map, attempts to figure out
// the stack regions for the thread. Fuchsia's C ABI specifies
// https://fuchsia.googlesource.com/zircon/+/master/docs/safestack.md so the
// callstack and locals-that-have-their-address-taken are in two different
// stacks.
void GetStackRegions(
    const zx_thread_state_general_regs_t& regs,
    const MemoryMapFuchsia& memory_map,
    std::vector<CheckedRange<zx_vaddr_t, size_t>>* stack_regions) {
  stack_regions->clear();

  uint64_t sp;
#if defined(ARCH_CPU_X86_64)
  sp = regs.rsp;
#elif defined(ARCH_CPU_ARM64)
  sp = regs.sp;
#else
#error Port
#endif

  zx_info_maps_t range_with_sp;
  if (!memory_map.FindMappingForAddress(sp, &range_with_sp)) {
    LOG(ERROR) << "stack pointer not found in mapping";
    return;
  }

  if (range_with_sp.type != ZX_INFO_MAPS_TYPE_MAPPING) {
    LOG(ERROR) << "stack range has unexpected type " << range_with_sp.type
               << ", aborting";
    return;
  }

  if (range_with_sp.u.mapping.mmu_flags & ZX_VM_PERM_EXECUTE) {
    LOG(ERROR)
        << "stack range is unexpectedly marked executable, continuing anyway";
  }

  // The stack covers [range_with_sp.base, range_with_sp.base +
  // range_with_sp.size). The stack pointer (sp) can be anywhere in that range.
  // It starts at the end of the range (range_with_sp.base + range_with_sp.size)
  // and goes downwards until range_with_sp.base. Capture the part of the stack
  // that is currently used: [sp, range_with_sp.base + range_with_sp.size).

  // Capture up to kExtraCaptureSize additional bytes of stack, but only if
  // present in the region that was already found.
  constexpr uint64_t kExtraCaptureSize = 128;
  const uint64_t start_address =
      std::max(sp >= kExtraCaptureSize ? sp - kExtraCaptureSize : sp,
               range_with_sp.base);
  const size_t region_size =
      range_with_sp.size - (start_address - range_with_sp.base);

  // Because most Fuchsia processes use safestack, it is very unlikely that a
  // stack this large would be valid. Even if it were, avoid creating
  // unreasonably large dumps by artificially limiting the captured amount.
  constexpr uint64_t kMaxStackCapture = 1048576u;
  LOG_IF(ERROR, region_size > kMaxStackCapture)
      << "clamping unexpectedly large stack capture of " << region_size;
  const size_t clamped_region_size = std::min(region_size, kMaxStackCapture);
  stack_regions->push_back(
      CheckedRange<zx_vaddr_t, size_t>(start_address, clamped_region_size));

  // TODO(scottmg): https://crashpad.chromium.org/bug/196, once the retrievable
  // registers include FS and similar for ARM, retrieve the region for the
  // unsafe part of the stack too.
}

}  // namespace

ProcessReaderFuchsia::Module::Module() = default;

ProcessReaderFuchsia::Module::~Module() = default;

ProcessReaderFuchsia::Thread::Thread() = default;

ProcessReaderFuchsia::Thread::~Thread() = default;

ProcessReaderFuchsia::ProcessReaderFuchsia() = default;

ProcessReaderFuchsia::~ProcessReaderFuchsia() = default;

bool ProcessReaderFuchsia::Initialize(const zx::process& process) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  process_ = zx::unowned_process(process);

  process_memory_.reset(new ProcessMemoryFuchsia());
  process_memory_->Initialize(*process_);

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

const std::vector<ProcessReaderFuchsia::Module>&
ProcessReaderFuchsia::Modules() {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  if (!initialized_modules_) {
    InitializeModules();
  }

  return modules_;
}

const std::vector<ProcessReaderFuchsia::Thread>&
ProcessReaderFuchsia::Threads() {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  if (!initialized_threads_) {
    InitializeThreads();
  }

  return threads_;
}

const MemoryMapFuchsia* ProcessReaderFuchsia::MemoryMap() {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  if (!initialized_memory_map_) {
    InitializeMemoryMap();
  }

  return memory_map_.get();
}

void ProcessReaderFuchsia::InitializeModules() {
  DCHECK(!initialized_modules_);
  DCHECK(modules_.empty());

  initialized_modules_ = true;

  // TODO(scottmg): <inspector/inspector.h> does some of this, but doesn't
  // expose any of the data that's necessary to fill out a Module after it
  // retrieves (some of) the data into internal structures. It may be worth
  // trying to refactor/upstream some of this into Fuchsia.

  // Starting from the ld.so's _dl_debug_addr, read the link_map structure and
  // walk the list to fill out modules_.

  uintptr_t debug_address;
  zx_status_t status = process_->get_property(
      ZX_PROP_PROCESS_DEBUG_ADDR, &debug_address, sizeof(debug_address));
  if (status != ZX_OK || debug_address == 0) {
    LOG(ERROR) << "zx_object_get_property ZX_PROP_PROCESS_DEBUG_ADDR";
    return;
  }

  constexpr auto k_r_debug_map_offset = offsetof(r_debug, r_map);
  uintptr_t map;
  if (!process_memory_->Read(
          debug_address + k_r_debug_map_offset, sizeof(map), &map)) {
    LOG(ERROR) << "read link_map";
    return;
  }

  int i = 0;
  constexpr int kMaxDso = 1000;  // Stop after an unreasonably large number.
  while (map != 0) {
    if (++i >= kMaxDso) {
      LOG(ERROR) << "possibly circular dso list, terminating";
      return;
    }

    constexpr auto k_link_map_addr_offset = offsetof(link_map, l_addr);
    zx_vaddr_t base;
    if (!process_memory_->Read(
            map + k_link_map_addr_offset, sizeof(base), &base)) {
      LOG(ERROR) << "Read base";
      // Could theoretically continue here, but realistically if any part of
      // link_map fails to read, things are looking bad, so just abort.
      break;
    }

    constexpr auto k_link_map_next_offset = offsetof(link_map, l_next);
    zx_vaddr_t next;
    if (!process_memory_->Read(
            map + k_link_map_next_offset, sizeof(next), &next)) {
      LOG(ERROR) << "Read next";
      break;
    }

    constexpr auto k_link_map_name_offset = offsetof(link_map, l_name);
    zx_vaddr_t name_address;
    if (!process_memory_->Read(map + k_link_map_name_offset,
                               sizeof(name_address),
                               &name_address)) {
      LOG(ERROR) << "Read name address";
      break;
    }

    std::string dsoname;
    if (!process_memory_->ReadCString(name_address, &dsoname)) {
      // In this case, it could be reasonable to continue on to the next module
      // as this data isn't strictly in the link_map.
      LOG(ERROR) << "ReadCString name";
    }

    // Debug symbols are indexed by module name x build-id on the crash server.
    // The module name in the indexed Breakpad files is set at build time. So
    // Crashpad needs to use the same module name at run time for symbol
    // resolution to work properly.
    //
    // TODO(fuchsia/DX-1234): once Crashpad switches to elf-search, the
    // following overwrites won't be necessary as only shared libraries will
    // have a soname at runtime, just like at build time.
    //
    // * For shared libraries, the soname is used as module name at build time,
    //   which is the dsoname here except for libzircon.so (because it is
    //   injected by the kernel, its load name is "<vDSO>" and Crashpad needs to
    //   replace it for symbol resolution to work properly).
    if (dsoname == "<vDSO>") {
      dsoname = "libzircon.so";
    }
    // * For executables and loadable modules, the dummy value "<_>" is used as
    //   module name at build time. This is because executable and loadable
    //   modules don't have a name on Fuchsia. So we need to use the same dummy
    //   value at build and run times.
    //   Most executables have an empty dsoname. Loadable modules (and some rare
    //   executables) have a non-empty dsoname starting with a specific prefix,
    //   which Crashpas can use to identify loadable modules and clear the
    //   dsoname for them.
    static constexpr const char kLoadableModuleLoadNamePrefix[] = "<VMO#";
    // Pre-C++ 20 std::basic_string::starts_with
    if (dsoname.compare(0,
                        strlen(kLoadableModuleLoadNamePrefix),
                        kLoadableModuleLoadNamePrefix) == 0) {
      dsoname = "";
    }

    Module module;
    if (dsoname.empty()) {
      // This value must be kept in sync with what is used at build time to
      // index symbols for executables and loadable modules.
      // See fuchsia/DX-1193 for more details.
      module.name = "<_>";
      module.type = ModuleSnapshot::kModuleTypeExecutable;
    } else {
      module.name = dsoname;
      // TODO(scottmg): Handle kModuleTypeDynamicLoader.
      module.type = ModuleSnapshot::kModuleTypeSharedLibrary;
    }

    std::unique_ptr<ElfImageReader> reader(new ElfImageReader());

    std::unique_ptr<ProcessMemoryRange> process_memory_range(
        new ProcessMemoryRange());
    // TODO(scottmg): Could this be limited range?
    if (process_memory_range->Initialize(process_memory_.get(), true)) {
      process_memory_ranges_.push_back(std::move(process_memory_range));

      if (reader->Initialize(*process_memory_ranges_.back(), base)) {
        module.reader = reader.get();
        module_readers_.push_back(std::move(reader));
        modules_.push_back(module);
      }
    }

    map = next;
  }
}

void ProcessReaderFuchsia::InitializeThreads() {
  DCHECK(!initialized_threads_);
  DCHECK(threads_.empty());

  initialized_threads_ = true;

  std::vector<zx_koid_t> thread_koids =
      GetChildKoids(*process_, ZX_INFO_PROCESS_THREADS);
  std::vector<zx::thread> thread_handles =
      GetHandlesForThreadKoids(*process_, thread_koids);
  DCHECK_EQ(thread_koids.size(), thread_handles.size());

  for (size_t i = 0; i < thread_handles.size(); ++i) {
    Thread thread;
    thread.id = thread_koids[i];

    if (thread_handles[i].is_valid()) {
      char name[ZX_MAX_NAME_LEN] = {0};
      zx_status_t status =
          thread_handles[i].get_property(ZX_PROP_NAME, &name, sizeof(name));
      if (status != ZX_OK) {
        ZX_LOG(WARNING, status) << "zx_object_get_property ZX_PROP_NAME";
      } else {
        thread.name.assign(name);
      }

      zx_info_thread_t thread_info;
      status = thread_handles[i].get_info(
          ZX_INFO_THREAD, &thread_info, sizeof(thread_info), nullptr, nullptr);
      if (status != ZX_OK) {
        ZX_LOG(WARNING, status) << "zx_object_get_info ZX_INFO_THREAD";
      } else {
        thread.state = thread_info.state;
      }

      zx_thread_state_general_regs_t general_regs;
      status = thread_handles[i].read_state(
          ZX_THREAD_STATE_GENERAL_REGS, &general_regs, sizeof(general_regs));
      if (status != ZX_OK) {
        ZX_LOG(WARNING, status)
            << "zx_thread_read_state(ZX_THREAD_STATE_GENERAL_REGS)";
      } else {
        thread.general_registers = general_regs;

        const MemoryMapFuchsia* memory_map = MemoryMap();
        if (memory_map) {
          // Attempt to retrive stack regions if a memory map was retrieved. In
          // particular, this may be null when operating on the current process
          // where the memory map will not be able to be retrieved.
          GetStackRegions(general_regs, *memory_map, &thread.stack_regions);
        }
      }

      zx_thread_state_vector_regs_t vector_regs;
      status = thread_handles[i].read_state(
          ZX_THREAD_STATE_VECTOR_REGS, &vector_regs, sizeof(vector_regs));
      if (status != ZX_OK) {
        ZX_LOG(WARNING, status)
            << "zx_thread_read_state(ZX_THREAD_STATE_VECTOR_REGS)";
      } else {
        thread.vector_registers = vector_regs;
      }
    }

    threads_.push_back(thread);
  }
}

void ProcessReaderFuchsia::InitializeMemoryMap() {
  DCHECK(!initialized_memory_map_);

  initialized_memory_map_ = true;

  memory_map_.reset(new MemoryMapFuchsia);
  if (!memory_map_->Initialize(*process_)) {
    memory_map_.reset();
  }
}

}  // namespace crashpad
