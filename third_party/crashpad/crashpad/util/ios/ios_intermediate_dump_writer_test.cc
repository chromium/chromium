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

#include "util/ios/ios_intermediate_dump_writer.h"

#include <fcntl.h>
#include <mach/mach.h>

#include "base/apple/scoped_mach_vm.h"
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
    EXPECT_TRUE(writer_->Close());
    writer_.reset();
    EXPECT_EQ(unlink(path_.value().c_str()), 0) << ErrnoMessage("unlink");
  }

  const base::FilePath& path() const { return path_; }

  std::unique_ptr<IOSIntermediateDumpWriter> writer_;

 private:
  ScopedTempDir temp_dir_;
  base::FilePath path_;
};

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
  EXPECT_TRUE(writer_->Close());

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
  EXPECT_TRUE(writer_->Close());

  std::string contents;
  ASSERT_TRUE(LoggingReadEntireFile(path(), &contents));
  std::string result("\6\1\xe8\3\2\a", 6);
  ASSERT_EQ(contents, result);
}

TEST_F(IOSIntermediateDumpWriterTest, Property) {
  EXPECT_TRUE(writer_->Open(path()));
  EXPECT_TRUE(writer_->AddProperty(Key::kVersion, "version", 7));
  EXPECT_TRUE(writer_->Close());

  std::string contents;
  ASSERT_TRUE(LoggingReadEntireFile(path(), &contents));
  std::string result("\5\1\0\a\0\0\0\0\0\0\0version", 18);
  ASSERT_EQ(contents, result);
}

TEST_F(IOSIntermediateDumpWriterTest, PropertyString) {
  EXPECT_TRUE(writer_->Open(path()));
  EXPECT_TRUE(writer_->AddPropertyCString(Key::kVersion, 64, "version"));
  EXPECT_TRUE(writer_->Close());

  std::string contents;
  ASSERT_TRUE(LoggingReadEntireFile(path(), &contents));
  std::string result("\5\1\0\a\0\0\0\0\0\0\0version", 18);
  ASSERT_EQ(contents, result);
}

TEST_F(IOSIntermediateDumpWriterTest, PropertyStringShort) {
  EXPECT_TRUE(writer_->Open(path()));
  EXPECT_FALSE(
      writer_->AddPropertyCString(Key::kVersion, 7, "versionnnnnnnnnnnn"));
}

TEST_F(IOSIntermediateDumpWriterTest, PropertyStringLong) {
  EXPECT_TRUE(writer_->Open(path()));

  char* bad_string = nullptr;
  EXPECT_FALSE(writer_->AddPropertyCString(Key::kVersion, 1025, bad_string));
}

TEST_F(IOSIntermediateDumpWriterTest, MissingPropertyString) {
  char* region;
  vm_size_t page_size = getpagesize();
  vm_size_t region_size = page_size * 2;
  ASSERT_EQ(vm_allocate(mach_task_self(),
                        reinterpret_cast<vm_address_t*>(&region),
                        region_size,
                        VM_FLAGS_ANYWHERE),
            0);
  base::apple::ScopedMachVM vm_owner(reinterpret_cast<vm_address_t>(region),
                                     region_size);

  // Fill first page with 'A' and second with 'B'.
  memset(region, 'A', page_size);
  memset(region + page_size, 'B', page_size);

  // Drop a NUL 10 bytes from the end of the first page and into the second
  // page.
  region[page_size - 10] = '\0';
  region[page_size + 10] = '\0';

  // Read a string that spans two pages.
  EXPECT_TRUE(writer_->Open(path()));
  EXPECT_TRUE(
      writer_->AddPropertyCString(Key::kVersion, 64, region + page_size - 5));
  EXPECT_TRUE(writer_->Close());
  std::string contents;
  ASSERT_TRUE(LoggingReadEntireFile(path(), &contents));
  std::string result("\x5\x1\0\xF\0\0\0\0\0\0\0AAAAABBBBBBBBBB", 26);
  ASSERT_EQ(contents, result);

  // Dealloc second page.
  ASSERT_EQ(vm_deallocate(mach_task_self(),
                          reinterpret_cast<vm_address_t>(region + page_size),
                          page_size),
            0);

  // Reading the same string should fail when the next page is dealloc-ed.
  EXPECT_FALSE(
      writer_->AddPropertyCString(Key::kVersion, 64, region + page_size - 5));

  // Ensure we can read the first string without loading the second page.
  EXPECT_TRUE(writer_->Open(path()));
  EXPECT_TRUE(
      writer_->AddPropertyCString(Key::kVersion, 64, region + page_size - 20));
  EXPECT_TRUE(writer_->Close());
  ASSERT_TRUE(LoggingReadEntireFile(path(), &contents));
  result.assign("\x5\x1\0\n\0\0\0\0\0\0\0AAAAAAAAAA", 21);
  ASSERT_EQ(contents, result);
}

TEST_F(IOSIntermediateDumpWriterTest, BadProperty) {
  EXPECT_TRUE(writer_->Open(path()));
  ASSERT_FALSE(writer_->AddProperty(Key::kVersion, "version", -1));
  EXPECT_TRUE(writer_->Close());
}

}  // namespace
}  // namespace test
}  // namespace crashpad
