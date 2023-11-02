// Copyright 2018 The Crashpad Authors
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

#include "snapshot/fuchsia/memory_map_region_snapshot_fuchsia.h"

#include <iterator>

#include "base/check_op.h"

namespace crashpad {
namespace internal {

namespace {

// Maps from bitwise OR of Zircon's flags to enumerated Windows version.
uint32_t MmuFlagsToProtectFlags(zx_vm_option_t flags) {
  // These bits are currently the lowest 3 of zx_vm_option_t. They're used to
  // index into a mapping array, so make sure that we notice if they change
  // values.
  static_assert(
      ZX_VM_PERM_READ == 1 && ZX_VM_PERM_WRITE == 2 && ZX_VM_PERM_EXECUTE == 4,
      "table below will need fixing");

  // The entries set to zero don't have good corresponding Windows minidump
  // names. They also aren't currently supported by the mapping syscall on
  // Zircon, so DCHECK that they don't happen in practice. EXECUTE-only also
  // cannot currently happen, but as that has a good mapping to the Windows
  // value, leave it in place in case it's supported by the syscall in the
  // future.
  static constexpr uint32_t mapping[] = {
      /* --- */ PAGE_NOACCESS,
      /* --r */ PAGE_READONLY,
      /* -w- */ 0,
      /* -wr */ PAGE_READWRITE,
      /* x-- */ PAGE_EXECUTE,
      /* x-r */ PAGE_EXECUTE_READ,
      /* xw- */ 0,
      /* xwr */ PAGE_EXECUTE_READWRITE,
  };

  const uint32_t index =
      flags & (ZX_VM_PERM_READ | ZX_VM_PERM_WRITE | ZX_VM_PERM_EXECUTE);
  DCHECK_LT(index, std::size(mapping));

  const uint32_t protect_flags = mapping[index];
  DCHECK_NE(protect_flags, 0u);
  return protect_flags;
}

}  // namespace

MemoryMapRegionSnapshotFuchsia::MemoryMapRegionSnapshotFuchsia(
    const zx_info_maps_t& info_map)
    : memory_info_() {
  DCHECK_EQ(info_map.type, ZX_INFO_MAPS_TYPE_MAPPING);

  memory_info_.BaseAddress = info_map.base;
  memory_info_.AllocationBase = info_map.base;
  memory_info_.RegionSize = info_map.size;
  memory_info_.State = MEM_COMMIT;
  memory_info_.Protect = memory_info_.AllocationProtect =
      MmuFlagsToProtectFlags(info_map.u.mapping.mmu_flags);
  memory_info_.Type = MEM_MAPPED;
}

MemoryMapRegionSnapshotFuchsia::~MemoryMapRegionSnapshotFuchsia() {}

const MINIDUMP_MEMORY_INFO&
MemoryMapRegionSnapshotFuchsia::AsMinidumpMemoryInfo() const {
  return memory_info_;
}

}  // namespace internal
}  // namespace crashpad
