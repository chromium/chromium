// Copyright 2020 The Crashpad Authors. All rights reserved.
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

#include "util/misc/no_cfi_icall.h"

#include <stdio.h>

#include "build/build_config.h"
#include "gtest/gtest.h"

#if defined(OS_WIN)
#include <windows.h>

#include "util/win/get_function.h"
#else
#include <dlfcn.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace crashpad {
namespace test {
namespace {

TEST(NoCfiIcall, NullptrIsFalse) {
  NoCfiIcall<void (*)(void) noexcept> call(nullptr);
  ASSERT_FALSE(call);
}

int TestFunc() noexcept {
  return 42;
}

TEST(NoCfiIcall, SameDSOICall) {
  NoCfiIcall<decltype(TestFunc)*> call(&TestFunc);
  ASSERT_TRUE(call);
  ASSERT_EQ(call(), 42);
}

TEST(NoCfiIcall, CrossDSOICall) {
#if defined(OS_WIN)
  static const NoCfiIcall<decltype(GetCurrentProcessId)*> call(
      GET_FUNCTION_REQUIRED(L"kernel32.dll", GetCurrentProcessId));
  ASSERT_TRUE(call);
  EXPECT_EQ(call(), GetCurrentProcessId());
#else
  static const NoCfiIcall<decltype(getpid)*> call(dlsym(RTLD_NEXT, "getpid"));
  ASSERT_TRUE(call);
  EXPECT_EQ(call(), getpid());
#endif
}

TEST(NoCfiIcall, Args) {
#if !defined(OS_WIN)
  static const NoCfiIcall<decltype(snprintf)*> call(
      dlsym(RTLD_NEXT, "snprintf"));
  ASSERT_TRUE(call);

  char buf[1024];

  // Regular args.
  memset(buf, 0, sizeof(buf));
  ASSERT_GT(call(buf, sizeof(buf), "Hello!"), 0);
  EXPECT_STREQ(buf, "Hello!");

  // Variadic args.
  memset(buf, 0, sizeof(buf));
  ASSERT_GT(call(buf, sizeof(buf), "%s, %s!", "Hello", "World"), 0);
  EXPECT_STREQ(buf, "Hello, World!");
#endif
}

}  // namespace
}  // namespace test
}  // namespace crashpad
