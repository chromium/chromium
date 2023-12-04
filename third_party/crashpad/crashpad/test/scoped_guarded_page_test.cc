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

#include "base/memory/page_size.h"
#include "gtest/gtest.h"
#include "test/gtest_death.h"

namespace crashpad {
namespace test {
namespace {

TEST(ScopedGuardedPage, BasicFunctionality) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");

  ScopedGuardedPage page;
  char* address = (char*)page.Pointer();
  EXPECT_NE(address, nullptr);
  address[0] = 0;
  address[base::GetPageSize() - 1] = 0;
  EXPECT_DEATH_CRASH({ address[base::GetPageSize()] = 0; }, "");
}

}  // namespace
}  // namespace test
}  // namespace crashpad
