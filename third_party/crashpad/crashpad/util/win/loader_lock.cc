// Copyright 2019 The Crashpad Authors
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

#include "util/win/loader_lock.h"

#include <windows.h>

#include "build/build_config.h"
#include "util/win/process_structs.h"

namespace crashpad {

namespace {

#ifdef ARCH_CPU_64_BITS
using NativeTraits = process_types::internal::Traits64;
#else
using NativeTraits = process_types::internal::Traits32;
#endif  // ARCH_CPU_64_BITS

using PEB = process_types::PEB<NativeTraits>;
using TEB = process_types::TEB<NativeTraits>;
using RTL_CRITICAL_SECTION = process_types::RTL_CRITICAL_SECTION<NativeTraits>;

TEB* GetTeb() {
  return reinterpret_cast<TEB*>(NtCurrentTeb());
}

PEB* GetPeb() {
  return reinterpret_cast<PEB*>(GetTeb()->ProcessEnvironmentBlock);
}

}  // namespace

bool IsThreadInLoaderLock() {
  RTL_CRITICAL_SECTION* loader_lock =
      reinterpret_cast<RTL_CRITICAL_SECTION*>(GetPeb()->LoaderLock);
  return loader_lock->OwningThread == GetTeb()->ClientId.UniqueThread;
}

}  // namespace crashpad
