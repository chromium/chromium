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

#include "test/scoped_guarded_page.h"

#include <sys/mman.h>

#include <ostream>

#include "base/check.h"
#include "base/memory/page_size.h"

namespace crashpad {
namespace test {

ScopedGuardedPage::ScopedGuardedPage() {
  ptr_ = mmap(nullptr,
              base::GetPageSize() * 2,
              PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_ANONYMOUS,
              -1,
              0);
  PCHECK(ptr_ != MAP_FAILED) << "mmap";

  // Simply mprotect()ing the guard page PROT_NONE does not make it inaccessible
  // using ptrace() or /proc/$pid/mem so we munmap() the following page instead.
  // Unfortunately, this means that the guarded page is not thread safe from
  // other threads mapping a single page into the empty region.
  char* second_page = static_cast<char*>(ptr_) + base::GetPageSize();
  PCHECK(munmap(second_page, base::GetPageSize()) >= 0) << "munmap";
}

ScopedGuardedPage::~ScopedGuardedPage() {
  PCHECK(munmap(ptr_, base::GetPageSize()) >= 0) << "munmap";
}

}  // namespace test
}  // namespace crashpad
