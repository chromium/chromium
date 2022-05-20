// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "reference_drivers/memory.h"

#include <lib/zx/vmar.h>
#include <zircon/syscalls.h>

#include <cstddef>
#include <cstdint>
#include <utility>

#include "reference_drivers/os_handle.h"
#include "third_party/abseil-cpp/absl/base/macros.h"

namespace ipcz::reference_drivers {

void Memory::Mapping::Reset() {
  if (base_address_) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(base_address_);
    zx_status_t status = zx::vmar::root_self()->unmap(addr, size_);
    ABSL_ASSERT(status == ZX_OK);
  }
}

Memory::Memory(size_t size) {
  const uint32_t page_size = zx_system_get_page_size();
  const size_t rounded_size = (size + page_size - 1) & (page_size - 1);
  zx::vmo vmo;
  zx_status_t status = zx::vmo::create(rounded_size, 0, &vmo);
  ABSL_ASSERT(status == ZX_OK);
  const int kNoExec = ZX_DEFAULT_VMO_RIGHTS & ~ZX_RIGHT_EXECUTE;
  status = vmo.replace(kNoExec, &vmo);
  ABSL_ASSERT(status == ZX_OK);
  handle_ = OSHandle(std::move(vmo));
  size_ = size;
}

Memory::Mapping Memory::Map() {
  ABSL_ASSERT(is_valid());
  uintptr_t addr;
  zx_vm_option_t options =
      ZX_VM_REQUIRE_NON_RESIZABLE | ZX_VM_PERM_READ | ZX_VM_PERM_WRITE;
  zx_status_t status = zx::vmar::root_self()->map(
      options, /*vmar_offset=*/0, *zx::unowned_vmo(handle_.handle().get()), 0,
      size_, &addr);
  if (status != ZX_OK) {
    return {};
  }
  return Mapping(reinterpret_cast<void*>(addr), size_);
}

}  // namespace ipcz::reference_drivers
