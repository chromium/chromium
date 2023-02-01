// Copyright 2023 The Crashpad Authors
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

#include "util/ios/scoped_vm_map.h"

#include "util/ios/raw_logging.h"

namespace crashpad {
namespace internal {

ScopedVMMapInternal::ScopedVMMapInternal()
    : data_(0),
      region_start_(0),
      region_size_(0),
      cur_protection_(VM_PROT_NONE) {}

ScopedVMMapInternal::~ScopedVMMapInternal() {
  Reset();
}

bool ScopedVMMapInternal::Map(const void* data, const size_t data_length) {
  Reset();

  vm_address_t data_address = reinterpret_cast<vm_address_t>(data);
  vm_address_t page_region_address = trunc_page(data_address);
  region_size_ = round_page(data_address - page_region_address + data_length);
  if (region_size_ < data_length) {
    CRASHPAD_RAW_LOG("ScopedVMMap data_length overflow");
    return false;
  }

  // Since region_start_ is 0, vm_remap() will choose an address to which the
  // memory will be mapped and store the mapped address in region_start_ on
  // success.
  vm_prot_t max_protection;
  kern_return_t kr = vm_remap(mach_task_self(),
                              &region_start_,
                              region_size_,
                              0,
                              TRUE,
                              mach_task_self(),
                              page_region_address,
                              FALSE,
                              &cur_protection_,
                              &max_protection,
                              VM_INHERIT_DEFAULT);
  if (kr != KERN_SUCCESS) {
    // It's expected that this will sometimes fail. Don't log here.
    return false;
  }

  data_ = region_start_ + (data_address - page_region_address);
  return true;
}

void ScopedVMMapInternal::Reset() {
  if (!region_start_) {
    return;
  }

  kern_return_t kr =
      vm_deallocate(mach_task_self(), region_start_, region_size_);

  if (kr != KERN_SUCCESS) {
    CRASHPAD_RAW_LOG_ERROR(kr, "vm_deallocate");
  }

  data_ = 0;
  region_start_ = 0;
  region_size_ = 0;
  cur_protection_ = VM_PROT_NONE;
}

}  // namespace internal
}  // namespace crashpad
