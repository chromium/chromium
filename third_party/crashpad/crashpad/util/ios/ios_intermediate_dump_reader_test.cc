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

#include "util/ios/ios_intermediate_dump_reader.h"

#include <fcntl.h>
#include <mach/vm_map.h>

#include "base/posix/eintr_wrapper.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/errors.h"
#include "test/scoped_temp_dir.h"
#include "util/file/filesystem.h"
#include "util/ios/ios_intermediate_dump_data.h"
#include "util/ios/ios_intermediate_dump_format.h"
#include "util/ios/ios_intermediate_dump_list.h"
#include "util/ios/ios_intermediate_dump_writer.h"

namespace crashpad {
namespace test {
namespace {

using Key = internal::IntermediateDumpKey;
using internal::IOSIntermediateDumpWriter;

class IOSIntermediateDumpReaderTest : public testing::Test {
 protected:
  // testing::Test:

  void SetUp() override {
    path_ = temp_dir_.path().Append("dump_file");
    fd_ = base::ScopedFD(HANDLE_EINTR(
        ::open(path_.value().c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644)));
    ASSERT_GE(fd_.get(), 0) << ErrnoMessage("open");

    writer_ = std::make_unique<IOSIntermediateDumpWriter>();
    ASSERT_TRUE(writer_->Open(path_));
    ASSERT_TRUE(IsRegularFile(path_));
  }

  void TearDown() override {
    fd_.reset();
    writer_.reset();
    EXPECT_FALSE(IsRegularFile(path_));
  }

  int fd() { return fd_.get(); }

  const base::FilePath& path() const { return path_; }

  std::unique_ptr<IOSIntermediateDumpWriter> writer_;

 private:
  base::ScopedFD fd_;
  ScopedTempDir temp_dir_;
  base::FilePath path_;
};

TEST_F(IOSIntermediateDumpReaderTest, ReadNoFile) {
  internal::IOSIntermediateDumpReader reader;
  EXPECT_FALSE(reader.Initialize(base::FilePath()));
  EXPECT_TRUE(LoggingRemoveFile(path()));
  EXPECT_FALSE(IsRegularFile(path()));
}

TEST_F(IOSIntermediateDumpReaderTest, ReadEmptyFile) {
  internal::IOSIntermediateDumpReader reader;
  EXPECT_FALSE(reader.Initialize(path()));
  EXPECT_FALSE(IsRegularFile(path()));

  const auto root_map = reader.RootMap();
  EXPECT_TRUE(root_map->empty());
}

TEST_F(IOSIntermediateDumpReaderTest, ReadHelloWorld) {
  std::string hello_world("hello world.");
  EXPECT_TRUE(
      LoggingWriteFile(fd(), hello_world.c_str(), hello_world.length()));
  internal::IOSIntermediateDumpReader reader;
  EXPECT_TRUE(reader.Initialize(path()));
  EXPECT_FALSE(IsRegularFile(path()));

  const auto root_map = reader.RootMap();
  EXPECT_TRUE(root_map->empty());
}

TEST_F(IOSIntermediateDumpReaderTest, WriteBadPropertyDataLength) {
  internal::IOSIntermediateDumpReader reader;
  IOSIntermediateDumpWriter::CommandType command_type =
      IOSIntermediateDumpWriter::CommandType::kRootMapStart;
  EXPECT_TRUE(LoggingWriteFile(fd(), &command_type, sizeof(command_type)));

  command_type = IOSIntermediateDumpWriter::CommandType::kProperty;
  EXPECT_TRUE(LoggingWriteFile(fd(), &command_type, sizeof(command_type)));
  Key key = Key::kVersion;
  EXPECT_TRUE(LoggingWriteFile(fd(), &key, sizeof(key)));
  uint8_t value = 1;
  size_t value_length = 999999;
  EXPECT_TRUE(LoggingWriteFile(fd(), &value_length, sizeof(size_t)));
  EXPECT_TRUE(LoggingWriteFile(fd(), &value, sizeof(value)));
  EXPECT_TRUE(reader.Initialize(path()));
  EXPECT_FALSE(IsRegularFile(path()));

  const auto root_map = reader.RootMap();
  EXPECT_TRUE(root_map->empty());
  const auto version_data = root_map->GetAsData(Key::kVersion);
  EXPECT_EQ(version_data, nullptr);
}

TEST_F(IOSIntermediateDumpReaderTest, InvalidArrayInArray) {
  internal::IOSIntermediateDumpReader reader;
  {
    IOSIntermediateDumpWriter::ScopedRootMap scopedRoot(writer_.get());
    IOSIntermediateDumpWriter::ScopedArray threadArray(writer_.get(),
                                                       Key::kThreads);
    IOSIntermediateDumpWriter::ScopedArray innerThreadArray(writer_.get(),
                                                            Key::kModules);

    // Write version last, so it's not parsed.
    int8_t version = 1;
    writer_->AddProperty(Key::kVersion, &version);
  }
  EXPECT_TRUE(writer_->Close());
  EXPECT_TRUE(reader.Initialize(path()));
  EXPECT_FALSE(IsRegularFile(path()));

  const auto root_map = reader.RootMap();
  EXPECT_FALSE(root_map->empty());
  const auto version_data = root_map->GetAsData(Key::kVersion);
  EXPECT_EQ(version_data, nullptr);
}

TEST_F(IOSIntermediateDumpReaderTest, InvalidPropertyInArray) {
  internal::IOSIntermediateDumpReader reader;

  {
    IOSIntermediateDumpWriter::ScopedRootMap scopedRoot(writer_.get());
    IOSIntermediateDumpWriter::ScopedArray threadArray(writer_.get(),
                                                       Key::kThreads);

    // Write version last, so it's not parsed.
    int8_t version = 1;
    writer_->AddProperty(Key::kVersion, &version);
  }
  EXPECT_TRUE(writer_->Close());
  EXPECT_TRUE(reader.Initialize(path()));
  EXPECT_FALSE(IsRegularFile(path()));

  const auto root_map = reader.RootMap();
  EXPECT_FALSE(root_map->empty());
  const auto version_data = root_map->GetAsData(Key::kVersion);
  EXPECT_EQ(version_data, nullptr);
}

TEST_F(IOSIntermediateDumpReaderTest, ReadValidData) {
  internal::IOSIntermediateDumpReader reader;
  uint8_t version = 1;
  {
    IOSIntermediateDumpWriter::ScopedRootMap scopedRoot(writer_.get());
    EXPECT_TRUE(writer_->AddProperty(Key::kVersion, &version));
    {
      IOSIntermediateDumpWriter::ScopedArray threadArray(
          writer_.get(), Key::kThreadContextMemoryRegions);
      IOSIntermediateDumpWriter::ScopedArrayMap threadMap(writer_.get());

      std::string random_data("random_data");
      EXPECT_TRUE(writer_->AddProperty(Key::kThreadContextMemoryRegionAddress,
                                       &version));
      EXPECT_TRUE(writer_->AddProperty(Key::kThreadContextMemoryRegionData,
                                       random_data.c_str(),
                                       random_data.length()));
    }

    {
      IOSIntermediateDumpWriter::ScopedMap map(writer_.get(),
                                               Key::kProcessInfo);
      pid_t p_pid = getpid();
      EXPECT_TRUE(writer_->AddProperty(Key::kPID, &p_pid));
    }
  }

  EXPECT_TRUE(writer_->Close());
  EXPECT_TRUE(reader.Initialize(path()));
  EXPECT_FALSE(IsRegularFile(path()));

  auto root_map = reader.RootMap();
  EXPECT_FALSE(root_map->empty());
  version = -1;
  const auto version_data = root_map->GetAsData(Key::kVersion);
  ASSERT_NE(version_data, nullptr);
  EXPECT_TRUE(version_data->GetValue<uint8_t>(&version));
  EXPECT_EQ(version, 1);

  const auto process_info = root_map->GetAsMap(Key::kProcessInfo);
  ASSERT_NE(process_info, nullptr);
  const auto pid_data = process_info->GetAsData(Key::kPID);
  ASSERT_NE(pid_data, nullptr);
  pid_t p_pid = -1;
  EXPECT_TRUE(pid_data->GetValue<pid_t>(&p_pid));
  ASSERT_EQ(p_pid, getpid());

  const auto thread_context_memory_regions =
      root_map->GetAsList(Key::kThreadContextMemoryRegions);
  EXPECT_EQ(thread_context_memory_regions->size(), 1UL);
  for (const auto& region : *thread_context_memory_regions) {
    const auto data = region->GetAsData(Key::kThreadContextMemoryRegionData);
    ASSERT_NE(data, nullptr);
    // Load as string.
    EXPECT_EQ(data->GetString(), "random_data");

    // Load as bytes.
    auto bytes = data->bytes();
    vm_size_t data_size = bytes.size();
    EXPECT_EQ(data_size, 11UL);

    const char* data_bytes = reinterpret_cast<const char*>(bytes.data());
    EXPECT_EQ(std::string(data_bytes, data_size), "random_data");
  }

  const auto system_info = root_map->GetAsMap(Key::kSystemInfo);
  EXPECT_EQ(system_info, nullptr);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
