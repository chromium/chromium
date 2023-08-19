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

#include <sys/time.h>

#include "base/apple/scoped_mach_vm.h"
#include "gtest/gtest.h"
#include "test/mac/mach_errors.h"

namespace crashpad {
namespace test {
namespace {

TEST(ScopedVMMapTest, BasicFunctionality) {
  // bad data or count.
  internal::ScopedVMMap<vm_address_t> vmmap_bad;
  EXPECT_FALSE(vmmap_bad.Map(nullptr, 100));
  EXPECT_FALSE(vmmap_bad.Map(reinterpret_cast<void*>(0x1000), 100));
  vm_address_t invalid_address = 1;
  EXPECT_FALSE(vmmap_bad.Map(&invalid_address, 1000000000));
  EXPECT_FALSE(vmmap_bad.Map(&invalid_address, -1));

  vm_address_t valid_address = reinterpret_cast<vm_address_t>(this);
  EXPECT_FALSE(vmmap_bad.Map(&valid_address, 1000000000));
  EXPECT_FALSE(vmmap_bad.Map(&valid_address, -1));

  // array
  static constexpr char map_me[] = "map me";
  internal::ScopedVMMap<char> vmmap_string;
  ASSERT_TRUE(vmmap_string.Map(map_me, strlen(map_me)));
  EXPECT_STREQ(vmmap_string.get(), map_me);
  EXPECT_TRUE(vmmap_string.CurrentProtection() & VM_PROT_READ);

  // struct
  timeval time_of_day;
  EXPECT_TRUE(gettimeofday(&time_of_day, nullptr) == 0);
  internal::ScopedVMMap<timeval> vmmap_time;
  ASSERT_TRUE(vmmap_time.Map(&time_of_day));
  constexpr vm_prot_t kReadWrite = VM_PROT_READ | VM_PROT_WRITE;
  EXPECT_EQ(vmmap_time.CurrentProtection() & kReadWrite, kReadWrite);
  EXPECT_EQ(vmmap_time->tv_sec, time_of_day.tv_sec);
  EXPECT_EQ(vmmap_time->tv_usec, time_of_day.tv_usec);

  // reset.
  timeval time_of_day2;
  EXPECT_TRUE(gettimeofday(&time_of_day2, nullptr) == 0);
  ASSERT_TRUE(vmmap_time.Map(&time_of_day2));
  EXPECT_EQ(vmmap_time.CurrentProtection() & kReadWrite, kReadWrite);
  EXPECT_EQ(vmmap_time->tv_sec, time_of_day2.tv_sec);
  EXPECT_EQ(vmmap_time->tv_usec, time_of_day2.tv_usec);
}

TEST(ScopedVMMapTest, MissingMiddleVM) {
  char* region;
  vm_size_t page_size = getpagesize();
  vm_size_t region_size = page_size * 3;
  kern_return_t kr = vm_allocate(mach_task_self(),
                                 reinterpret_cast<vm_address_t*>(&region),
                                 region_size,
                                 VM_FLAGS_ANYWHERE);
  ASSERT_EQ(kr, KERN_SUCCESS) << MachErrorMessage(kr, "vm_allocate");

  base::apple::ScopedMachVM vm_owner(reinterpret_cast<vm_address_t>(region),
                                     region_size);

  internal::ScopedVMMap<char> vmmap_missing_middle;
  ASSERT_TRUE(vmmap_missing_middle.Map(region, region_size));

  // Dealloc middle page.
  kr = vm_deallocate(mach_task_self(),
                     reinterpret_cast<vm_address_t>(region + page_size),
                     page_size);
  ASSERT_EQ(kr, KERN_SUCCESS) << MachErrorMessage(kr, "vm_deallocate");

  EXPECT_FALSE(vmmap_missing_middle.Map(region, region_size));
  ASSERT_TRUE(vmmap_missing_middle.Map(region, page_size));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
