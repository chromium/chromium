// Copyright 2014 The Crashpad Authors. All rights reserved.
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

#include <mach/mach.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <string>

#include "base/mac/scoped_mach_port.h"
#include "base/mac/scoped_mach_vm.h"
#include "gtest/gtest.h"
#include "test/mac/mach_errors.h"
#include "util/misc/from_pointer_cast.h"

namespace crashpad {
namespace test {
namespace {

TEST(ProcessMemoryMac, ReadMappedSelf) {
  vm_address_t address = 0;
  const vm_size_t kSize = 4 * PAGE_SIZE;
  kern_return_t kr =
      vm_allocate(mach_task_self(), &address, kSize, VM_FLAGS_ANYWHERE);
  ASSERT_EQ(kr, KERN_SUCCESS) << MachErrorMessage(kr, "vm_allocate");
  base::mac::ScopedMachVM vm_owner(address, mach_vm_round_page(kSize));

  char* region = reinterpret_cast<char*>(address);
  for (size_t index = 0; index < kSize; ++index) {
    region[index] = (index % 256) ^ ((index >> 8) % 256);
  }

  ProcessMemoryMac memory;
  ASSERT_TRUE(memory.Initialize(mach_task_self()));

  std::string result(kSize, '\0');
  std::unique_ptr<ProcessMemoryMac::MappedMemory> mapped;

  // Ensure that the entire region can be read.
  ASSERT_TRUE((mapped = memory.ReadMapped(address, kSize)));
  EXPECT_EQ(memcmp(region, mapped->data(), kSize), 0);

  // Ensure that a read of length 0 succeeds and doesn't touch the result.
  result.assign(kSize, '\0');
  std::string zeroes = result;
  ASSERT_TRUE((mapped = memory.ReadMapped(address, 0)));
  EXPECT_EQ(result, zeroes);

  // Ensure that a read starting at an unaligned address works.
  ASSERT_TRUE((mapped = memory.ReadMapped(address + 1, kSize - 1)));
  EXPECT_EQ(memcmp(region + 1, mapped->data(), kSize - 1), 0);

  // Ensure that a read ending at an unaligned address works.
  ASSERT_TRUE((mapped = memory.ReadMapped(address, kSize - 1)));
  EXPECT_EQ(memcmp(region, mapped->data(), kSize - 1), 0);

  // Ensure that a read starting and ending at unaligned addresses works.
  ASSERT_TRUE((mapped = memory.ReadMapped(address + 1, kSize - 2)));
  EXPECT_EQ(memcmp(region + 1, mapped->data(), kSize - 2), 0);

  // Ensure that a read of exactly one page works.
  ASSERT_TRUE((mapped = memory.ReadMapped(address + PAGE_SIZE, PAGE_SIZE)));
  EXPECT_EQ(memcmp(region + PAGE_SIZE, mapped->data(), PAGE_SIZE), 0);

  // Ensure that a read of a single byte works.
  ASSERT_TRUE((mapped = memory.ReadMapped(address + 2, 1)));
  EXPECT_EQ(reinterpret_cast<const char*>(mapped->data())[0], region[2]);

  // Ensure that a read of length zero works and doesn't touch the data.
  result[0] = 'M';
  ASSERT_TRUE((mapped = memory.ReadMapped(address + 3, 0)));
  EXPECT_EQ(result[0], 'M');
}

TEST(ProcessMemoryMac, ReadSelfUnmapped) {
  vm_address_t address = 0;
  const vm_size_t kSize = 2 * PAGE_SIZE;
  kern_return_t kr =
      vm_allocate(mach_task_self(), &address, kSize, VM_FLAGS_ANYWHERE);
  ASSERT_EQ(kr, KERN_SUCCESS) << MachErrorMessage(kr, "vm_allocate");
  base::mac::ScopedMachVM vm_owner(address, mach_vm_round_page(kSize));

  char* region = reinterpret_cast<char*>(address);
  for (size_t index = 0; index < kSize; ++index) {
    // Don't include any NUL bytes, because ReadCString stops when it encounters
    // a NUL.
    region[index] = (index % 255) + 1;
  }

  kr = vm_protect(
      mach_task_self(), address + PAGE_SIZE, PAGE_SIZE, FALSE, VM_PROT_NONE);
  ASSERT_EQ(kr, KERN_SUCCESS) << MachErrorMessage(kr, "vm_protect");

  ProcessMemoryMac memory;
  ASSERT_TRUE(memory.Initialize(mach_task_self()));
  std::string result(kSize, '\0');

  EXPECT_FALSE(memory.Read(address, kSize, &result[0]));
  EXPECT_FALSE(memory.Read(address + 1, kSize - 1, &result[0]));
  EXPECT_FALSE(memory.Read(address + PAGE_SIZE, 1, &result[0]));
  EXPECT_FALSE(memory.Read(address + PAGE_SIZE - 1, 2, &result[0]));
  EXPECT_TRUE(memory.Read(address, PAGE_SIZE, &result[0]));
  EXPECT_TRUE(memory.Read(address + PAGE_SIZE - 1, 1, &result[0]));

  // Do the same thing with the ReadMapped() interface.
  std::unique_ptr<ProcessMemoryMac::MappedMemory> mapped;
  EXPECT_FALSE((mapped = memory.ReadMapped(address, kSize)));
  EXPECT_FALSE((mapped = memory.ReadMapped(address + 1, kSize - 1)));
  EXPECT_FALSE((mapped = memory.ReadMapped(address + PAGE_SIZE, 1)));
  EXPECT_FALSE((mapped = memory.ReadMapped(address + PAGE_SIZE - 1, 2)));
  EXPECT_TRUE((mapped = memory.ReadMapped(address, PAGE_SIZE)));
  EXPECT_TRUE((mapped = memory.ReadMapped(address + PAGE_SIZE - 1, 1)));

  // Repeat the test with an unmapped page instead of an unreadable one. This
  // portion of the test may be flaky in the presence of other threads, if
  // another thread maps something in the region that is deallocated here.
  kr = vm_deallocate(mach_task_self(), address + PAGE_SIZE, PAGE_SIZE);
  ASSERT_EQ(kr, KERN_SUCCESS) << MachErrorMessage(kr, "vm_deallocate");
  vm_owner.reset(address, PAGE_SIZE);

  EXPECT_FALSE(memory.Read(address, kSize, &result[0]));
  EXPECT_FALSE(memory.Read(address + 1, kSize - 1, &result[0]));
  EXPECT_FALSE(memory.Read(address + PAGE_SIZE, 1, &result[0]));
  EXPECT_FALSE(memory.Read(address + PAGE_SIZE - 1, 2, &result[0]));
  EXPECT_TRUE(memory.Read(address, PAGE_SIZE, &result[0]));
  EXPECT_TRUE(memory.Read(address + PAGE_SIZE - 1, 1, &result[0]));

  // Do the same thing with the ReadMapped() interface.
  EXPECT_FALSE((mapped = memory.ReadMapped(address, kSize)));
  EXPECT_FALSE((mapped = memory.ReadMapped(address + 1, kSize - 1)));
  EXPECT_FALSE((mapped = memory.ReadMapped(address + PAGE_SIZE, 1)));
  EXPECT_FALSE((mapped = memory.ReadMapped(address + PAGE_SIZE - 1, 2)));
  EXPECT_TRUE((mapped = memory.ReadMapped(address, PAGE_SIZE)));
  EXPECT_TRUE((mapped = memory.ReadMapped(address + PAGE_SIZE - 1, 1)));
}

TEST(ProcessMemoryMac, ReadCStringSelfUnmapped) {
  vm_address_t address = 0;
  const vm_size_t kSize = 2 * PAGE_SIZE;
  kern_return_t kr =
      vm_allocate(mach_task_self(), &address, kSize, VM_FLAGS_ANYWHERE);
  ASSERT_EQ(kr, KERN_SUCCESS) << MachErrorMessage(kr, "vm_allocate");
  base::mac::ScopedMachVM vm_owner(address, mach_vm_round_page(kSize));

  char* region = reinterpret_cast<char*>(address);
  for (size_t index = 0; index < kSize; ++index) {
    // Don't include any NUL bytes, because ReadCString stops when it encounters
    // a NUL.
    region[index] = (index % 255) + 1;
  }

  kr = vm_protect(
      mach_task_self(), address + PAGE_SIZE, PAGE_SIZE, FALSE, VM_PROT_NONE);
  ASSERT_EQ(kr, KERN_SUCCESS) << MachErrorMessage(kr, "vm_protect");

  ProcessMemoryMac memory;
  ASSERT_TRUE(memory.Initialize(mach_task_self()));
  std::string result;
  EXPECT_FALSE(memory.ReadCString(address, &result));

  // Make sure that if the string is NUL-terminated within the mapped memory
  // region, it can be read properly.
  char terminator_or_not = '\0';
  std::swap(region[PAGE_SIZE - 1], terminator_or_not);
  ASSERT_TRUE(memory.ReadCString(address, &result));
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(result.size(), PAGE_SIZE - 1u);
  EXPECT_EQ(result, region);

  // Repeat the test with an unmapped page instead of an unreadable one. This
  // portion of the test may be flaky in the presence of other threads, if
  // another thread maps something in the region that is deallocated here.
  std::swap(region[PAGE_SIZE - 1], terminator_or_not);
  kr = vm_deallocate(mach_task_self(), address + PAGE_SIZE, PAGE_SIZE);
  ASSERT_EQ(kr, KERN_SUCCESS) << MachErrorMessage(kr, "vm_deallocate");
  vm_owner.reset(address, PAGE_SIZE);

  EXPECT_FALSE(memory.ReadCString(address, &result));

  // Clear the result before testing that the string can be read. This makes
  // sure that the result is actually filled in, because it already contains the
  // expected value from the tests above.
  result.clear();
  std::swap(region[PAGE_SIZE - 1], terminator_or_not);
  ASSERT_TRUE(memory.ReadCString(address, &result));
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(result.size(), PAGE_SIZE - 1u);
  EXPECT_EQ(result, region);
}

bool IsAddressMapped(vm_address_t address) {
  vm_address_t region_address = address;
  vm_size_t region_size;
  mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
  vm_region_basic_info_64 info;
  mach_port_t object;
  kern_return_t kr = vm_region_64(mach_task_self(),
                                  &region_address,
                                  &region_size,
                                  VM_REGION_BASIC_INFO_64,
                                  reinterpret_cast<vm_region_info_t>(&info),
                                  &count,
                                  &object);
  if (kr == KERN_SUCCESS) {
    // |object| will be MACH_PORT_NULL (10.9.4 xnu-2422.110.17/osfmk/vm/vm_map.c
    // vm_map_region()), but the interface acts as if it might carry a send
    // right, so treat it as documented.
    base::mac::ScopedMachSendRight object_owner(object);

    return address >= region_address && address <= region_address + region_size;
  }

  if (kr == KERN_INVALID_ADDRESS) {
    return false;
  }

  ADD_FAILURE() << MachErrorMessage(kr, "vm_region_64");
  return false;
}

TEST(ProcessMemoryMac, MappedMemoryDeallocates) {
  // This tests that once a ProcessMemoryMac::MappedMemory object is destroyed,
  // it releases the mapped memory that it owned. Technically, this test is not
  // valid because after the mapping is released, something else (on another
  // thread) might wind up mapped in the same address. In the test environment,
  // hopefully there are either no other threads or they're all quiescent, so
  // nothing else should wind up mapped in the address.

  ProcessMemoryMac memory;
  ASSERT_TRUE(memory.Initialize(mach_task_self()));
  std::unique_ptr<ProcessMemoryMac::MappedMemory> mapped;

  static constexpr char kTestBuffer[] = "hello!";
  mach_vm_address_t test_address =
      FromPointerCast<mach_vm_address_t>(&kTestBuffer);
  ASSERT_TRUE((mapped = memory.ReadMapped(test_address, sizeof(kTestBuffer))));
  EXPECT_EQ(memcmp(kTestBuffer, mapped->data(), sizeof(kTestBuffer)), 0);

  vm_address_t mapped_address = reinterpret_cast<vm_address_t>(mapped->data());
  EXPECT_TRUE(IsAddressMapped(mapped_address));

  mapped.reset();
  EXPECT_FALSE(IsAddressMapped(mapped_address));

  // This is the same but with a big buffer that’s definitely larger than a
  // single page. This makes sure that the whole mapped region winds up being
  // deallocated.
  const size_t kBigSize = 4 * PAGE_SIZE;
  std::unique_ptr<char[]> big_buffer(new char[kBigSize]);
  test_address = FromPointerCast<mach_vm_address_t>(&big_buffer[0]);
  ASSERT_TRUE((mapped = memory.ReadMapped(test_address, kBigSize)));

  mapped_address = reinterpret_cast<vm_address_t>(mapped->data());
  vm_address_t mapped_last_address = mapped_address + kBigSize - 1;
  EXPECT_TRUE(IsAddressMapped(mapped_address));
  EXPECT_TRUE(IsAddressMapped(mapped_address + PAGE_SIZE));
  EXPECT_TRUE(IsAddressMapped(mapped_last_address));

  mapped.reset();
  EXPECT_FALSE(IsAddressMapped(mapped_address));
  EXPECT_FALSE(IsAddressMapped(mapped_address + PAGE_SIZE));
  EXPECT_FALSE(IsAddressMapped(mapped_last_address));
}

TEST(ProcessMemoryMac, MappedMemoryReadCString) {
  // This tests the behavior of ProcessMemoryMac::MappedMemory::ReadCString().
  ProcessMemoryMac memory;
  ASSERT_TRUE(memory.Initialize(mach_task_self()));
  std::unique_ptr<ProcessMemoryMac::MappedMemory> mapped;

  static constexpr char kTestBuffer[] = "0\0" "2\0" "45\0" "789";
  const mach_vm_address_t kTestAddress =
      FromPointerCast<mach_vm_address_t>(&kTestBuffer);
  ASSERT_TRUE((mapped = memory.ReadMapped(kTestAddress, 10)));

  std::string string;
  ASSERT_TRUE(mapped->ReadCString(0, &string));
  EXPECT_EQ(string, "0");
  ASSERT_TRUE(mapped->ReadCString(1, &string));
  EXPECT_EQ(string, "");
  ASSERT_TRUE(mapped->ReadCString(2, &string));
  EXPECT_EQ(string, "2");
  ASSERT_TRUE(mapped->ReadCString(3, &string));
  EXPECT_EQ(string, "");
  ASSERT_TRUE(mapped->ReadCString(4, &string));
  EXPECT_EQ(string, "45");
  ASSERT_TRUE(mapped->ReadCString(5, &string));
  EXPECT_EQ(string, "5");
  ASSERT_TRUE(mapped->ReadCString(6, &string));
  EXPECT_EQ(string, "");

  // kTestBuffer’s NUL terminator was not read, so these will see an
  // unterminated string and fail.
  EXPECT_FALSE(mapped->ReadCString(7, &string));
  EXPECT_FALSE(mapped->ReadCString(8, &string));
  EXPECT_FALSE(mapped->ReadCString(9, &string));

  // This is out of the range of what was read, so it will fail.
  EXPECT_FALSE(mapped->ReadCString(10, &string));
  EXPECT_FALSE(mapped->ReadCString(11, &string));

  // Read it again, this time with a length long enough to include the NUL
  // terminator.
  ASSERT_TRUE((mapped = memory.ReadMapped(kTestAddress, 11)));

  ASSERT_TRUE(mapped->ReadCString(6, &string));
  EXPECT_EQ(string, "");

  // These should now succeed.
  ASSERT_TRUE(mapped->ReadCString(7, &string));
  EXPECT_EQ(string, "789");
  ASSERT_TRUE(mapped->ReadCString(8, &string));
  EXPECT_EQ(string, "89");
  ASSERT_TRUE(mapped->ReadCString(9, &string));
  EXPECT_EQ(string, "9");
  EXPECT_TRUE(mapped->ReadCString(10, &string));
  EXPECT_EQ(string, "");

  // These are still out of range.
  EXPECT_FALSE(mapped->ReadCString(11, &string));
  EXPECT_FALSE(mapped->ReadCString(12, &string));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
