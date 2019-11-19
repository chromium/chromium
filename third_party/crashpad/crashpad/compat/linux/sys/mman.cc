// Copyright 2019 The Crashpad Authors. All rights reserved.
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

#include <sys/mman.h>

#include <dlfcn.h>
#include <sys/syscall.h>
#include <unistd.h>

#if defined(__GLIBC__)

extern "C" {

int memfd_create(const char* name, unsigned int flags) {
  using MemfdCreateType = int (*)(const char*, int);
  static const MemfdCreateType next_memfd_create =
      reinterpret_cast<MemfdCreateType>(dlsym(RTLD_NEXT, "memfd_create"));
  return next_memfd_create ? next_memfd_create(name, flags)
                           : syscall(SYS_memfd_create, name, flags);
}

}  // extern "C"

#endif  // __GLIBC__
