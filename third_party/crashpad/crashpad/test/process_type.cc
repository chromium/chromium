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

#include "test/process_type.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_FUCHSIA)
#include <lib/zx/process.h>
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include <unistd.h>
#endif

namespace crashpad {
namespace test {

ProcessType GetSelfProcess() {
#if BUILDFLAG(IS_FUCHSIA)
  return zx::process::self();
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  return getpid();
#elif BUILDFLAG(IS_WIN)
  return GetCurrentProcess();
#elif BUILDFLAG(IS_APPLE)
  return mach_task_self();
#endif
}

}  // namespace test
}  // namespace crashpad
