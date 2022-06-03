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

#ifndef CRASHPAD_TEST_PROCESS_TYPE_H_
#define CRASHPAD_TEST_PROCESS_TYPE_H_

#include "build/build_config.h"

#if defined(OS_FUCHSIA)
#include <lib/zx/process.h>
#elif defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
#include <sys/types.h>
#elif defined(OS_WIN)
#include <windows.h>
#elif defined(OS_APPLE)
#include <mach/mach.h>
#endif

namespace crashpad {
namespace test {

#if defined(OS_FUCHSIA)
using ProcessType = zx::unowned_process;
#elif defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID) || \
    DOXYGEN
//! \brief Alias for platform-specific type to represent a process.
using ProcessType = pid_t;
#elif defined(OS_WIN)
using ProcessType = HANDLE;
#elif defined(OS_APPLE)
using ProcessType = task_t;
#else
#error Port.
#endif

//! \brief Get a ProcessType representing the current process.
ProcessType GetSelfProcess();

}  // namespace test
}  // namespace crashpad

#endif  // CRASHPAD_TEST_PROCESS_TYPE_H_
