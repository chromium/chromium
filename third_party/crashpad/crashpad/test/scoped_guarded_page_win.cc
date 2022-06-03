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

#include "test/scoped_guarded_page.h"

#include <windows.h>

#include <ostream>

#include "base/check.h"
#include "base/memory/page_size.h"

namespace crashpad {
namespace test {

ScopedGuardedPage::ScopedGuardedPage() {
  const size_t page_size = base::GetPageSize();
  ptr_ = VirtualAlloc(nullptr, page_size * 2, MEM_RESERVE, PAGE_NOACCESS);
  PCHECK(ptr_ != nullptr) << "VirtualAlloc";

  PCHECK(VirtualAlloc(ptr_, page_size, MEM_COMMIT, PAGE_READWRITE) != nullptr)
      << "VirtualAlloc";
}

ScopedGuardedPage::~ScopedGuardedPage() {
  PCHECK(VirtualFree(ptr_, 0, MEM_RELEASE)) << "VirtualFree";
}

}  // namespace test
}  // namespace crashpad
