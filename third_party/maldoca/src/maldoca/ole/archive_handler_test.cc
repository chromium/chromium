// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "maldoca/ole/archive_handler.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "maldoca/base/file.h"
#include "maldoca/base/testing/test_utils.h"

namespace maldoca {
namespace utils {
namespace {

std::string TestFilename(absl::string_view filename) {
  return maldoca::testing::OleTestFilename(filename);
}

std::string GetTestContent(absl::string_view filename) {
  std::string content;
  auto status =
      maldoca::testing::GetTestContents(TestFilename(filename), &content);
  EXPECT_TRUE(status.ok()) << status;
  return content;
}

std::string DataToString(const std::string& filename, int64_t size, bool isdir,
                         const std::string& content) {
  return absl::StrCat(filename, ":", size, ":", isdir, ":", content);
}

TEST(ArchiveHandler, ExtractZipStringTest) {
  std::string content = GetTestContent("archive_zip.zip");
  auto status_or =
      ::maldoca::utils::GetArchiveHandler(content, "zip", "", false, false);
  CHECK(status_or.ok() && status_or.value()->Initialized())
      << "Can't initialize, error: " << status_or.status();
  auto handler = status_or.value().get();

  CHECK(handler->Initialized());
  std::string filename;
  int64_t size = 0;
  bool isdir = false;
  std::vector<std::string> output;
  while (handler->GetNextEntry(&filename, &size, &isdir)) {
    std::string extracted_content;
    if (!isdir) {
      CHECK(handler->GetEntryContent(&extracted_content));
    }
    output.push_back(DataToString(filename, size, isdir, extracted_content));
  }
  EXPECT_THAT(
      output,
      ::testing::UnorderedElementsAre(
          "archive_zip/:0:1:", "archive_zip/test.txt:5:0:blah\n",
          "archive_zip/dir1/:0:1:", "archive_zip/dir1/test1.txt:5:0:blah\n",
          "archive_zip/dir2/:0:1:"));
}

TEST(ArchiveHandler, ExtractZipStringGetNextGoodContentTest) {
  std::string content = GetTestContent("archive_zip.zip");
  auto status_or =
      ::maldoca::utils::GetArchiveHandler(content, "zip", "", false, false);
  CHECK(status_or.ok() && status_or.value()->Initialized())
      << "Can't initialize, error: " << status_or.status();
  auto handler = status_or.value().get();
  CHECK(handler->Initialized());
  std::string filename;
  int64_t size = 0;
  bool isdir = false;
  std::vector<std::string> output;
  std::string extracted_content;
  while (handler->GetNextGoodContent(&filename, &size, &extracted_content)) {
    output.push_back(DataToString(filename, size, isdir, extracted_content));
    extracted_content.clear();
  }
  EXPECT_THAT(
      output,
      ::testing::UnorderedElementsAre(
          "archive_zip/test.txt:5:0:blah\n",
          "archive_zip/dir1/test1.txt:5:0:blah\n"));
}

}  // namespace
}  // namespace utils
}  // namespace maldoca
