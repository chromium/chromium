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

#include <Availability.h>
#include <pthread.h>

#include "base/mac/mach_logging.h"
#include "build/build_config.h"
#include "util/mac/mac_util.h"

namespace crashpad {

thread_t MachThreadSelf() {
  // The pthreads library keeps its own copy of the thread port. Using it does
  // not increment its reference count.
  return pthread_mach_thread_np(pthread_self());
}

mach_port_t NewMachPort(mach_port_right_t right) {
  mach_port_t port = MACH_PORT_NULL;
  kern_return_t kr = mach_port_allocate(mach_task_self(), right, &port);
  MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr) << "mach_port_allocate";
  return port;
}

exception_mask_t ExcMaskAll() {
  // This is necessary because of the way that the kernel validates
  // exception_mask_t arguments to
  // {host,task,thread}_{get,set,swap}_exception_ports(). It is strict,
  // rejecting attempts to operate on any bits that it does not recognize. See
  // 10.9.4 xnu-2422.110.17/osfmk/mach/ipc_host.c and
  // xnu-2422.110.17/osfmk/mach/ipc_tt.c.

#if defined(OS_IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_7_0
// iOS 7 ≅ OS X 10.9.
#error This code was not ported to iOS versions older than 7
#endif

#if defined(OS_MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_9
  const int macos_version_number = MacOSVersionNumber();
#endif

  // See 10.6.8 xnu-1504.15.3/osfmk/mach/exception_types.h. 10.7 uses the same
  // definition as 10.6. See 10.7.5 xnu-1699.32.7/osfmk/mach/exception_types.h
  constexpr exception_mask_t kExcMaskAll_10_6 =
      EXC_MASK_BAD_ACCESS |
      EXC_MASK_BAD_INSTRUCTION |
      EXC_MASK_ARITHMETIC |
      EXC_MASK_EMULATION |
      EXC_MASK_SOFTWARE |
      EXC_MASK_BREAKPOINT |
      EXC_MASK_SYSCALL |
      EXC_MASK_MACH_SYSCALL |
      EXC_MASK_RPC_ALERT |
      EXC_MASK_MACHINE;
#if defined(OS_MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_8
  if (macos_version_number < 10'08'00) {
    return kExcMaskAll_10_6;
  }
#endif

  // 10.8 added EXC_MASK_RESOURCE. See 10.8.5
  // xnu-2050.48.11/osfmk/mach/exception_types.h.
  constexpr exception_mask_t kExcMaskAll_10_8 =
      kExcMaskAll_10_6 | EXC_MASK_RESOURCE;
#if defined(OS_MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_9
  if (macos_version_number < 10'09'00) {
    return kExcMaskAll_10_8;
  }
#endif

  // 10.9 added EXC_MASK_GUARD. See 10.9.4
  // xnu-2422.110.17/osfmk/mach/exception_types.h.
  constexpr exception_mask_t kExcMaskAll_10_9 =
      kExcMaskAll_10_8 | EXC_MASK_GUARD;
  return kExcMaskAll_10_9;
}

exception_mask_t ExcMaskValid() {
  const exception_mask_t kExcMaskValid_10_6 = ExcMaskAll() | EXC_MASK_CRASH;
#if defined(OS_IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_9_0
// iOS 9 ≅ OS X 10.11.
#error This code was not ported to iOS versions older than 9
#endif

#if defined(OS_MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_11
  if (MacOSVersionNumber() < 10'11'00) {
    return kExcMaskValid_10_6;
  }
#endif

  // 10.11 added EXC_MASK_CORPSE_NOTIFY. See 10.11 <mach/exception_types.h>.
  const exception_mask_t kExcMaskValid_10_11 =
      kExcMaskValid_10_6 | EXC_MASK_CORPSE_NOTIFY;
  return kExcMaskValid_10_11;
}

}  // namespace crashpad
