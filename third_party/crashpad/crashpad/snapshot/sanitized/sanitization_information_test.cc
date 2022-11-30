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

#include "snapshot/sanitized/sanitization_information.h"

#include <iterator>

#include "build/build_config.h"
#include "gtest/gtest.h"
#include "util/misc/from_pointer_cast.h"
#include "util/process/process_memory_linux.h"

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "test/linux/fake_ptrace_connection.h"
#endif

namespace crashpad {
namespace test {
namespace {

class AllowedAnnotationsTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(connection_.Initialize(getpid()));

#if defined(ARCH_CPU_64_BITS)
    ASSERT_TRUE(range_.Initialize(connection_.Memory(), true));
#else
    ASSERT_TRUE(range_.Initialize(connection_.Memory(), false));
#endif
  }

 protected:
  bool DoReadAllowedAnnotations(const char* const* address) {
    return ReadAllowedAnnotations(
        range_, FromPointerCast<VMAddress>(address), &allowed_annotations_);
  }

  FakePtraceConnection connection_;
  ProcessMemoryRange range_;
  std::vector<std::string> allowed_annotations_;
};

const char* const kEmptyAllowedAnnotations[] = {nullptr};

TEST_F(AllowedAnnotationsTest, EmptyAllowedAnnotations) {
  ASSERT_TRUE(DoReadAllowedAnnotations(kEmptyAllowedAnnotations));
  EXPECT_EQ(allowed_annotations_, std::vector<std::string>());
}

const char* const kNonEmptyAllowedAnnotations[] = {"string1",
                                                   "another_string",
                                                   "",
                                                   nullptr};

TEST_F(AllowedAnnotationsTest, NonEmptyAllowedAnnotations) {
  ASSERT_TRUE(DoReadAllowedAnnotations(kNonEmptyAllowedAnnotations));
  ASSERT_EQ(allowed_annotations_.size(),
            std::size(kNonEmptyAllowedAnnotations) - 1);
  for (size_t index = 0; index < allowed_annotations_.size(); ++index) {
    EXPECT_EQ(allowed_annotations_[index], kNonEmptyAllowedAnnotations[index]);
  }
}

}  // namespace
}  // namespace test
}  // namespace crashpad
