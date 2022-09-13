// Copyright 2021 The Crashpad Authors
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

#include "client/ios_handler/in_process_handler.h"

#include "gtest/gtest.h"
#include "test/scoped_temp_dir.h"
#include "test/test_paths.h"
#include "util/file/directory_reader.h"
#include "util/file/filesystem.h"

namespace crashpad {
namespace test {
namespace {

bool CreateFile(const base::FilePath& file) {
  ScopedFileHandle fd(LoggingOpenFileForWrite(
      file, FileWriteMode::kCreateOrFail, FilePermissions::kOwnerOnly));
  EXPECT_TRUE(fd.is_valid());
  return fd.is_valid();
}

class InProcessHandlerTest : public testing::Test {
 protected:
  // testing::Test:

  void SetUp() override {
    ASSERT_TRUE(in_process_handler_.Initialize(temp_dir_.path(), "", {}));
    pending_dir_ = temp_dir_.path().Append("pending-serialized-ios-dump");
    bundle_identifier_and_seperator_ = system_data_.BundleIdentifier() + "@";
  }

  const auto& path() const { return pending_dir_; }
  auto& handler() { return in_process_handler_; }

  void CreateFiles(int files, int other_files) {
    base::FilePath::StringType file_prepend =
        FILE_PATH_LITERAL(bundle_identifier_and_seperator_);
    base::FilePath::StringType file_name = FILE_PATH_LITERAL("file");
    for (int i = 0; i < files; i++) {
      std::string i_str = std::to_string(i);
      base::FilePath file(file_prepend + file_name + i_str);
      CreateFile(path().Append(file));
    }

    for (int i = 0; i < other_files; i++) {
      std::string i_str = std::to_string(i);
      base::FilePath file(file_name + i_str);
      CreateFile(path().Append(file));
    }
  }

  void VerifyRemainingFileCount(int expected_files_count,
                                int expected_other_files_count) {
    DirectoryReader reader;
    ASSERT_TRUE(reader.Open(path()));
    DirectoryReader::Result result;
    base::FilePath filename;
    int files_count = 0;
    int other_files_count = 0;
    while ((result = reader.NextFile(&filename)) ==
           DirectoryReader::Result::kSuccess) {
      bool bundle_match =
          filename.value().compare(0,
                                   bundle_identifier_and_seperator_.size(),
                                   bundle_identifier_and_seperator_) == 0;
      if (bundle_match) {
        files_count++;
      } else {
        other_files_count++;
      }
    }
    EXPECT_EQ(expected_files_count, files_count);
    EXPECT_EQ(expected_other_files_count, other_files_count);
  }

  void ClearFiles() {
    DirectoryReader reader;
    ASSERT_TRUE(reader.Open(path()));
    DirectoryReader::Result result;
    base::FilePath filename;
    while ((result = reader.NextFile(&filename)) ==
           DirectoryReader::Result::kSuccess) {
      LoggingRemoveFile(path().Append(filename));
    }
  }

 private:
  ScopedTempDir temp_dir_;
  base::FilePath pending_dir_;
  std::string bundle_identifier_and_seperator_;
  internal::IOSSystemDataCollector system_data_;
  internal::InProcessHandler in_process_handler_;
};

TEST_F(InProcessHandlerTest, TestPendingFileLimit) {
  // Clear this first to blow away the pending file held by InProcessHandler.
  ClearFiles();

  // Only process other app files.
  CreateFiles(0, 20);
  handler().ProcessIntermediateDumps({});
  VerifyRemainingFileCount(0, 0);
  ClearFiles();

  // Only process our app files.
  CreateFiles(20, 20);
  handler().ProcessIntermediateDumps({});
  VerifyRemainingFileCount(0, 20);
  ClearFiles();

  // Process all of our files and 10   remaining.
  CreateFiles(10, 30);
  handler().ProcessIntermediateDumps({});
  VerifyRemainingFileCount(0, 20);
  ClearFiles();

  // Process 20 our files, leaving 10 remaining, and all other files remaining.
  CreateFiles(30, 10);
  handler().ProcessIntermediateDumps({});
  VerifyRemainingFileCount(10, 10);
  ClearFiles();

  CreateFiles(0, 0);
  handler().ProcessIntermediateDumps({});
  VerifyRemainingFileCount(0, 0);
  ClearFiles();

  CreateFiles(10, 0);
  handler().ProcessIntermediateDumps({});
  VerifyRemainingFileCount(0, 0);
  ClearFiles();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
