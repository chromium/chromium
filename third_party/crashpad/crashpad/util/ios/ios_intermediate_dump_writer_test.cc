// Copyright 2021 The Crashpad Authors. All rights reserved.
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

#include "util/ios/ios_intermediate_dump_writer.h"

#include <fcntl.h>

#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/errors.h"
#include "test/scoped_temp_dir.h"
#include "util/file/file_io.h"

namespace crashpad {
namespace test {
namespace {

using Key = internal::IntermediateDumpKey;
using internal::IOSIntermediateDumpWriter;

class IOSIntermediateDumpWriterTest : public testing::Test {
 protected:
  // testing::Test:

  void SetUp() override {
    path_ = temp_dir_.path().Append("dump_file");
    writer_ = std::make_unique<IOSIntermediateDumpWriter>();
  }

  void TearDown() override {
    writer_.reset();
    EXPECT_EQ(unlink(path_.value().c_str()), 0) << ErrnoMessage("unlink");
  }

  const base::FilePath& path() const { return path_; }

  std::unique_ptr<IOSIntermediateDumpWriter> writer_;

 private:
  ScopedTempDir temp_dir_;
  base::FilePath path_;
};

// Test file is locked.
TEST_F(IOSIntermediateDumpWriterTest, OpenLocked) {
  EXPECT_TRUE(writer_->Open(path()));

  ScopedFileHandle handle(LoggingOpenFileForRead(path()));
  EXPECT_TRUE(handle.is_valid());
  EXPECT_EQ(LoggingLockFile(handle.get(),
                            FileLocking::kExclusive,
                            FileLockingBlocking::kNonBlocking),
            FileLockingResult::kWouldBlock);
}

TEST_F(IOSIntermediateDumpWriterTest, Close) {
  EXPECT_TRUE(writer_->Open(path()));
  EXPECT_TRUE(writer_->Close());

  std::string contents;
  ASSERT_TRUE(LoggingReadEntireFile(path(), &contents));
  ASSERT_EQ(contents, "");
}

TEST_F(IOSIntermediateDumpWriterTest, ScopedArray) {
  EXPECT_TRUE(writer_->Open(path()));
  {
    IOSIntermediateDumpWriter::ScopedRootMap rootMap(writer_.get());
    IOSIntermediateDumpWriter::ScopedArray threadArray(writer_.get(),
                                                       Key::kThreads);
    IOSIntermediateDumpWriter::ScopedArrayMap threadMap(writer_.get());
  }
  std::string contents;
  ASSERT_TRUE(LoggingReadEntireFile(path(), &contents));
  std::string result("\6\x3p\x17\1\2\4\a", 8);
  ASSERT_EQ(contents, result);
}

TEST_F(IOSIntermediateDumpWriterTest, ScopedMap) {
  EXPECT_TRUE(writer_->Open(path()));
  {
    IOSIntermediateDumpWriter::ScopedRootMap rootMap(writer_.get());
    IOSIntermediateDumpWriter::ScopedMap map(writer_.get(),
                                             Key::kMachException);
  }

  std::string contents;
  ASSERT_TRUE(LoggingReadEntireFile(path(), &contents));
  std::string result("\6\1\xe8\3\2\a", 6);
  ASSERT_EQ(contents, result);
}

TEST_F(IOSIntermediateDumpWriterTest, Property) {
  EXPECT_TRUE(writer_->Open(path()));
  EXPECT_TRUE(writer_->AddProperty(Key::kVersion, "version", 7));

  std::string contents;
  ASSERT_TRUE(LoggingReadEntireFile(path(), &contents));
  std::string result("\5\1\0\a\0\0\0\0\0\0\0version", 18);
  ASSERT_EQ(contents, result);
}

TEST_F(IOSIntermediateDumpWriterTest, BadProperty) {
  EXPECT_TRUE(writer_->Open(path()));
  ASSERT_FALSE(writer_->AddProperty(Key::kVersion, "version", -1));

  std::string contents;
  ASSERT_TRUE(LoggingReadEntireFile(path(), &contents));

  // path() is now invalid, as type, key and value were written, but the
  // value itself is not.
  std::string results("\5\1\0\xff\xff\xff\xff\xff\xff\xff\xff", 11);
  ASSERT_EQ(contents, results);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
