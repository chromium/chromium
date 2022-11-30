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

#include "util/process/process_memory_sanitized.h"

#include "build/build_config.h"
#include "gtest/gtest.h"
#include "test/process_type.h"
#include "util/misc/from_pointer_cast.h"
#include "util/process/process_memory_native.h"

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "test/linux/fake_ptrace_connection.h"
#endif

namespace crashpad {
namespace test {
namespace {

TEST(ProcessMemorySanitized, DenyDisallowedMemory) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  FakePtraceConnection connection;
  ASSERT_TRUE(connection.Initialize(GetSelfProcess()));
  ProcessMemoryLinux memory(&connection);
#else
  ProcessMemoryNative memory;
  ASSERT_TRUE(memory.Initialize(GetSelfProcess()));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

  char c = 42;
  char out;

  ProcessMemorySanitized san_null;
  san_null.Initialize(&memory, nullptr);
  EXPECT_FALSE(san_null.Read(FromPointerCast<VMAddress>(&c), 1, &out));

  std::vector<std::pair<VMAddress, VMAddress>> allowed_memory;
  ProcessMemorySanitized san_empty;
  san_empty.Initialize(&memory, &allowed_memory);
  EXPECT_FALSE(san_empty.Read(FromPointerCast<VMAddress>(&c), 1, &out));
}

TEST(ProcessMemorySanitized, AllowedMemory) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  FakePtraceConnection connection;
  ASSERT_TRUE(connection.Initialize(GetSelfProcess()));
  ProcessMemoryLinux memory(&connection);
#else
  ProcessMemoryNative memory;
  ASSERT_TRUE(memory.Initialize(GetSelfProcess()));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

  char str[4] = "ABC";
  char out[4];

  std::vector<std::pair<VMAddress, VMAddress>> allowed_memory;
  allowed_memory.push_back(std::make_pair(FromPointerCast<VMAddress>(str + 1),
                                          FromPointerCast<VMAddress>(str + 2)));

  ProcessMemorySanitized sanitized;
  sanitized.Initialize(&memory, &allowed_memory);

  EXPECT_FALSE(sanitized.Read(FromPointerCast<VMAddress>(str), 1, &out));
  EXPECT_TRUE(sanitized.Read(FromPointerCast<VMAddress>(str + 1), 1, &out));
  EXPECT_FALSE(sanitized.Read(FromPointerCast<VMAddress>(str + 2), 1, &out));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
