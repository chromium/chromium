// Copyright 2021 The Crashpad Authors. All rights reserved.
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

#include "util/ios/scoped_vm_read.h"

#include <sys/time.h>

#include "base/mac/scoped_mach_vm.h"
#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

TEST(ScopedVMReadTest, BasicFunctionality) {
  // bad data or count.
  internal::ScopedVMRead<vm_address_t> vmread_bad;
  ASSERT_FALSE(vmread_bad.Read(nullptr, 100));
  ASSERT_FALSE(vmread_bad.Read(reinterpret_cast<void*>(0x1000), 100));
  vm_address_t address = 1;
  ASSERT_FALSE(vmread_bad.Read(&address, 1000000000));

  // array
  constexpr char read_me[] = "read me";
  internal::ScopedVMRead<char> vmread_string;
  ASSERT_TRUE(vmread_string.Read(read_me, strlen(read_me)));
  EXPECT_STREQ(read_me, vmread_string.get());

  // struct
  timeval time_of_day;
  EXPECT_TRUE(gettimeofday(&time_of_day, nullptr) == 0);
  internal::ScopedVMRead<timeval> vmread_time;
  ASSERT_TRUE(vmread_time.Read(&time_of_day));
  EXPECT_EQ(time_of_day.tv_sec, vmread_time->tv_sec);
  EXPECT_EQ(time_of_day.tv_usec, vmread_time->tv_usec);

  // reset.
  timeval time_of_day2;
  EXPECT_TRUE(gettimeofday(&time_of_day2, nullptr) == 0);
  ASSERT_TRUE(vmread_time.Read(&time_of_day2));
  EXPECT_EQ(time_of_day2.tv_sec, vmread_time->tv_sec);
  EXPECT_EQ(time_of_day2.tv_usec, vmread_time->tv_usec);
}

TEST(ScopedVMReadTest, MissingMiddleVM) {
  char* region;
  vm_size_t page_size = getpagesize();
  vm_size_t region_size = page_size * 3;
  ASSERT_EQ(vm_allocate(mach_task_self(),
                        reinterpret_cast<vm_address_t*>(&region),
                        region_size,
                        VM_FLAGS_ANYWHERE),
            0);
  base::mac::ScopedMachVM vm_owner(reinterpret_cast<vm_address_t>(region),
                                   region_size);

  internal::ScopedVMRead<char> vmread_missing_middle;
  ASSERT_TRUE(vmread_missing_middle.Read(region, region_size));

  // Dealloc middle page.
  ASSERT_EQ(vm_deallocate(mach_task_self(),
                          reinterpret_cast<vm_address_t>(region + page_size),
                          page_size),
            0);

  ASSERT_FALSE(vmread_missing_middle.Read(region, region_size));
  ASSERT_TRUE(vmread_missing_middle.Read(region, page_size));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
