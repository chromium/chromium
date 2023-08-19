// Copyright 2016 Google Inc.
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
// limitations under the License.!

#include "filesystem.h"
#include "absl/strings/str_cat.h"
#include "testharness.h"
#include "util.h"

namespace sentencepiece {

TEST(UtilTest, FilesystemTest) {
  const std::vector<std::string> kData = {
      "This"
      "is"
      "a"
      "test"};

  {
    auto output = filesystem::NewWritableFile(
        util::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "test_file"));
    for (size_t i = 0; i < kData.size(); ++i) {
      output->WriteLine(kData[i]);
    }
  }

  {
    auto input = filesystem::NewReadableFile(
        util::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "test_file"));
    std::string line;
    for (size_t i = 0; i < kData.size(); ++i) {
      EXPECT_TRUE(input->ReadLine(&line));
      EXPECT_EQ(kData[i], line);
    }
    EXPECT_FALSE(input->ReadLine(&line));
  }
}

TEST(UtilTest, FilesystemInvalidFileTest) {
  auto input = filesystem::NewReadableFile("__UNKNOWN__FILE__");
  EXPECT_FALSE(input->status().ok());
}

}  // namespace sentencepiece
