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

#include "util/mach/mach_extensions.h"

#include "base/mac/scoped_mach_port.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "test/mac/mach_errors.h"
#include "util/mac/mac_util.h"

namespace crashpad {
namespace test {
namespace {

TEST(MachExtensions, MachThreadSelf) {
  base::mac::ScopedMachSendRight thread_self(mach_thread_self());
  EXPECT_EQ(MachThreadSelf(), thread_self);
}

TEST(MachExtensions, NewMachPort_Receive) {
  base::mac::ScopedMachReceiveRight port(NewMachPort(MACH_PORT_RIGHT_RECEIVE));
  ASSERT_NE(port, kMachPortNull);

  mach_port_type_t type;
  kern_return_t kr = mach_port_type(mach_task_self(), port.get(), &type);
  ASSERT_EQ(kr, KERN_SUCCESS) << MachErrorMessage(kr, "mach_port_get_type");

  EXPECT_EQ(type, MACH_PORT_TYPE_RECEIVE);
}

TEST(MachExtensions, NewMachPort_PortSet) {
  base::mac::ScopedMachPortSet port(NewMachPort(MACH_PORT_RIGHT_PORT_SET));
  ASSERT_NE(port, kMachPortNull);

  mach_port_type_t type;
  kern_return_t kr = mach_port_type(mach_task_self(), port.get(), &type);
  ASSERT_EQ(kr, KERN_SUCCESS) << MachErrorMessage(kr, "mach_port_get_type");

  EXPECT_EQ(type, MACH_PORT_TYPE_PORT_SET);
}

TEST(MachExtensions, NewMachPort_DeadName) {
  base::mac::ScopedMachSendRight port(NewMachPort(MACH_PORT_RIGHT_DEAD_NAME));
  ASSERT_NE(port, kMachPortNull);

  mach_port_type_t type;
  kern_return_t kr = mach_port_type(mach_task_self(), port.get(), &type);
  ASSERT_EQ(kr, KERN_SUCCESS) << MachErrorMessage(kr, "mach_port_get_type");

  EXPECT_EQ(type, MACH_PORT_TYPE_DEAD_NAME);
}

constexpr exception_mask_t kExcMaskBasic =
    EXC_MASK_BAD_ACCESS |
    EXC_MASK_BAD_INSTRUCTION |
    EXC_MASK_ARITHMETIC |
    EXC_MASK_EMULATION |
    EXC_MASK_SOFTWARE |
    EXC_MASK_BREAKPOINT |
    EXC_MASK_SYSCALL |
    EXC_MASK_MACH_SYSCALL |
    EXC_MASK_RPC_ALERT;

TEST(MachExtensions, ExcMaskAll) {
  const exception_mask_t exc_mask_all = ExcMaskAll();
  EXPECT_EQ(exc_mask_all & kExcMaskBasic, kExcMaskBasic);

  EXPECT_FALSE(exc_mask_all & EXC_MASK_CRASH);
  EXPECT_FALSE(exc_mask_all & EXC_MASK_CORPSE_NOTIFY);

#if defined(OS_IOS)
  // Assume at least iOS 7 (≅ OS X 10.9).
  EXPECT_TRUE(exc_mask_all & EXC_MASK_RESOURCE);
  EXPECT_TRUE(exc_mask_all & EXC_MASK_GUARD);
#else  // OS_IOS
  const int mac_os_x_minor_version = MacOSXMinorVersion();
  if (mac_os_x_minor_version >= 8) {
    EXPECT_TRUE(exc_mask_all & EXC_MASK_RESOURCE);
  } else {
    EXPECT_FALSE(exc_mask_all & EXC_MASK_RESOURCE);
  }

  if (mac_os_x_minor_version >= 9) {
    EXPECT_TRUE(exc_mask_all & EXC_MASK_GUARD);
  } else {
    EXPECT_FALSE(exc_mask_all & EXC_MASK_GUARD);
  }
#endif  // OS_IOS

  // Bit 0 should not be set.
  EXPECT_FALSE(ExcMaskAll() & 1);

  // Every bit set in ExcMaskAll() must also be set in ExcMaskValid().
  EXPECT_EQ(ExcMaskAll() & ExcMaskValid(), ExcMaskAll());
}

TEST(MachExtensions, ExcMaskValid) {
  const exception_mask_t exc_mask_valid = ExcMaskValid();
  EXPECT_EQ(exc_mask_valid & kExcMaskBasic, kExcMaskBasic);

  EXPECT_TRUE(exc_mask_valid & EXC_MASK_CRASH);

#if defined(OS_IOS)
  // Assume at least iOS 9 (≅ OS X 10.11).
  EXPECT_TRUE(exc_mask_valid & EXC_MASK_RESOURCE);
  EXPECT_TRUE(exc_mask_valid & EXC_MASK_GUARD);
  EXPECT_TRUE(exc_mask_valid & EXC_MASK_CORPSE_NOTIFY);
#else  // OS_IOS
  const int mac_os_x_minor_version = MacOSXMinorVersion();
  if (mac_os_x_minor_version >= 8) {
    EXPECT_TRUE(exc_mask_valid & EXC_MASK_RESOURCE);
  } else {
    EXPECT_FALSE(exc_mask_valid & EXC_MASK_RESOURCE);
  }

  if (mac_os_x_minor_version >= 9) {
    EXPECT_TRUE(exc_mask_valid & EXC_MASK_GUARD);
  } else {
    EXPECT_FALSE(exc_mask_valid & EXC_MASK_GUARD);
  }

  if (mac_os_x_minor_version >= 11) {
    EXPECT_TRUE(exc_mask_valid & EXC_MASK_CORPSE_NOTIFY);
  } else {
    EXPECT_FALSE(exc_mask_valid & EXC_MASK_CORPSE_NOTIFY);
  }
#endif  // OS_IOS

  // Bit 0 should not be set.
  EXPECT_FALSE(ExcMaskValid() & 1);

  // There must be bits set in ExcMaskValid() that are not set in ExcMaskAll().
  EXPECT_TRUE(ExcMaskValid() & ~ExcMaskAll());
}

}  // namespace
}  // namespace test
}  // namespace crashpad
