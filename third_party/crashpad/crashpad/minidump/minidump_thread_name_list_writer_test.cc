// Copyright 2022 The Crashpad Authors
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

#include "minidump/minidump_thread_name_list_writer.h"

#include <iterator>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "gtest/gtest.h"
#include "minidump/minidump_file_writer.h"
#include "minidump/minidump_system_info_writer.h"
#include "minidump/test/minidump_file_writer_test_util.h"
#include "minidump/test/minidump_string_writer_test_util.h"
#include "minidump/test/minidump_writable_test_util.h"
#include "test/gtest_death.h"
#include "util/file/string_file.h"

namespace crashpad {
namespace test {
namespace {

// This returns the MINIDUMP_THREAD_NAME_LIST stream in |thread_name_list|.
void GetThreadNameListStream(
    const std::string& file_contents,
    const MINIDUMP_THREAD_NAME_LIST** thread_name_list) {
  constexpr size_t kDirectoryOffset = sizeof(MINIDUMP_HEADER);
  const uint32_t kExpectedStreams = 1;
  const size_t kThreadNameListStreamOffset =
      kDirectoryOffset + kExpectedStreams * sizeof(MINIDUMP_DIRECTORY);
  const size_t kThreadNameListOffset =
      kThreadNameListStreamOffset + sizeof(MINIDUMP_THREAD_NAME_LIST);

  ASSERT_GE(file_contents.size(), kThreadNameListOffset);

  const MINIDUMP_DIRECTORY* directory;
  const MINIDUMP_HEADER* header =
      MinidumpHeaderAtStart(file_contents, &directory);
  ASSERT_NO_FATAL_FAILURE(VerifyMinidumpHeader(header, kExpectedStreams, 0));
  ASSERT_TRUE(directory);

  ASSERT_EQ(directory[0].StreamType, kMinidumpStreamTypeThreadNameList);
  EXPECT_EQ(directory[0].Location.Rva, kThreadNameListStreamOffset);

  *thread_name_list =
      MinidumpWritableAtLocationDescriptor<MINIDUMP_THREAD_NAME_LIST>(
          file_contents, directory[0].Location);
  ASSERT_TRUE(thread_name_list);
}

TEST(MinidumpThreadNameListWriter, EmptyThreadNameList) {
  MinidumpFileWriter minidump_file_writer;
  auto thread_name_list_writer =
      std::make_unique<MinidumpThreadNameListWriter>();

  ASSERT_TRUE(
      minidump_file_writer.AddStream(std::move(thread_name_list_writer)));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  ASSERT_EQ(string_file.string().size(),
            sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
                sizeof(MINIDUMP_THREAD_NAME_LIST));

  const MINIDUMP_THREAD_NAME_LIST* thread_name_list = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      GetThreadNameListStream(string_file.string(), &thread_name_list));

  EXPECT_EQ(thread_name_list->NumberOfThreadNames, 0u);
}

// The MINIDUMP_THREAD_NAMEs |expected| and |observed| are compared against
// each other using Google Test assertions.
void ExpectThreadName(const MINIDUMP_THREAD_NAME* expected,
                      const MINIDUMP_THREAD_NAME* observed,
                      const std::string& file_contents,
                      const std::string& expected_thread_name) {
  // Copy RvaOfThreadName into a local variable because
  // |MINIDUMP_THREAD_NAME::RvaOfThreadName| requires 8-byte alignment but the
  // struct itself is 4-byte algined.
  const auto rva_of_thread_name = [&observed] {
    RVA64 data = 0;
    memcpy(&data, &observed->RvaOfThreadName, sizeof(RVA64));
    return data;
  }();

  EXPECT_EQ(observed->ThreadId, expected->ThreadId);
  EXPECT_NE(rva_of_thread_name, 0u);
  const std::string observed_thread_name = base::UTF16ToUTF8(
      MinidumpStringAtRVAAsString(file_contents, rva_of_thread_name));
  EXPECT_EQ(observed_thread_name, expected_thread_name);
}

TEST(MinidumpThreadNameListWriter, OneThread) {
  MinidumpFileWriter minidump_file_writer;
  auto thread_list_writer = std::make_unique<MinidumpThreadNameListWriter>();

  constexpr uint32_t kThreadID = 0x11111111;
  const std::string kThreadName = "ariadne";

  auto thread_name_list_writer =
      std::make_unique<MinidumpThreadNameListWriter>();
  auto thread_name_writer = std::make_unique<MinidumpThreadNameWriter>();
  thread_name_writer->SetThreadId(kThreadID);
  thread_name_writer->SetThreadName(kThreadName);
  thread_name_list_writer->AddThreadName(std::move(thread_name_writer));

  ASSERT_TRUE(
      minidump_file_writer.AddStream(std::move(thread_name_list_writer)));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  ASSERT_GT(string_file.string().size(),
            sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
                sizeof(MINIDUMP_THREAD_NAME_LIST) +
                1 * sizeof(MINIDUMP_THREAD_NAME));

  const MINIDUMP_THREAD_NAME_LIST* thread_name_list = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      GetThreadNameListStream(string_file.string(), &thread_name_list));

  EXPECT_EQ(thread_name_list->NumberOfThreadNames, 1u);

  MINIDUMP_THREAD_NAME expected = {};
  expected.ThreadId = kThreadID;

  ASSERT_NO_FATAL_FAILURE(ExpectThreadName(&expected,
                                           &thread_name_list->ThreadNames[0],
                                           string_file.string(),
                                           kThreadName));
}

TEST(MinidumpThreadNameListWriter, OneThreadWithLeadingPadding) {
  MinidumpFileWriter minidump_file_writer;

  // Add a stream before the MINIDUMP_THREAD_NAME_LIST to ensure the thread name
  // MINIDUMP_STRING requires leading padding to align to a 4-byte boundary.
  auto system_info_writer = std::make_unique<MinidumpSystemInfoWriter>();
  system_info_writer->SetCSDVersion("");
  ASSERT_TRUE(minidump_file_writer.AddStream(std::move(system_info_writer)));

  auto thread_list_writer = std::make_unique<MinidumpThreadNameListWriter>();

  constexpr uint32_t kThreadID = 0x11111111;
  const std::string kThreadName = "ariadne";

  auto thread_name_list_writer =
      std::make_unique<MinidumpThreadNameListWriter>();
  auto thread_name_writer = std::make_unique<MinidumpThreadNameWriter>();
  thread_name_writer->SetThreadId(kThreadID);
  thread_name_writer->SetThreadName(kThreadName);
  thread_name_list_writer->AddThreadName(std::move(thread_name_writer));

  ASSERT_TRUE(
      minidump_file_writer.AddStream(std::move(thread_name_list_writer)));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  ASSERT_GT(string_file.string().size(),
            sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
                sizeof(MINIDUMP_THREAD_NAME_LIST) +
                1 * sizeof(MINIDUMP_THREAD_NAME));

  const uint32_t kExpectedStreams = 2;
  const MINIDUMP_DIRECTORY* directory;
  const MINIDUMP_HEADER* header =
      MinidumpHeaderAtStart(string_file.string(), &directory);
  ASSERT_NO_FATAL_FAILURE(VerifyMinidumpHeader(header, kExpectedStreams, 0));
  ASSERT_TRUE(directory);

  ASSERT_EQ(directory[0].StreamType, kMinidumpStreamTypeSystemInfo);
  ASSERT_EQ(directory[1].StreamType, kMinidumpStreamTypeThreadNameList);

  const MINIDUMP_THREAD_NAME_LIST* thread_name_list =
      MinidumpWritableAtLocationDescriptor<MINIDUMP_THREAD_NAME_LIST>(
          string_file.string(), directory[1].Location);
  ASSERT_TRUE(thread_name_list);

  EXPECT_EQ(thread_name_list->NumberOfThreadNames, 1u);

  MINIDUMP_THREAD_NAME expected = {};
  expected.ThreadId = kThreadID;

  ASSERT_NO_FATAL_FAILURE(ExpectThreadName(&expected,
                                           &thread_name_list->ThreadNames[0],
                                           string_file.string(),
                                           kThreadName));
}

TEST(MinidumpThreadNameListWriter, TwoThreads_DifferentNames) {
  MinidumpFileWriter minidump_file_writer;
  auto thread_list_writer = std::make_unique<MinidumpThreadNameListWriter>();

  constexpr uint32_t kFirstThreadID = 0x11111111;
  const std::string kFirstThreadName = "ariadne";

  constexpr uint32_t kSecondThreadID = 0x22222222;
  const std::string kSecondThreadName = "theseus";

  auto thread_name_list_writer =
      std::make_unique<MinidumpThreadNameListWriter>();
  auto first_thread_name_writer = std::make_unique<MinidumpThreadNameWriter>();
  first_thread_name_writer->SetThreadId(kFirstThreadID);
  first_thread_name_writer->SetThreadName(kFirstThreadName);
  thread_name_list_writer->AddThreadName(std::move(first_thread_name_writer));

  auto second_thread_name_writer = std::make_unique<MinidumpThreadNameWriter>();
  second_thread_name_writer->SetThreadId(kSecondThreadID);
  second_thread_name_writer->SetThreadName(kSecondThreadName);
  thread_name_list_writer->AddThreadName(std::move(second_thread_name_writer));

  ASSERT_TRUE(
      minidump_file_writer.AddStream(std::move(thread_name_list_writer)));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  ASSERT_GT(string_file.string().size(),
            sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
                sizeof(MINIDUMP_THREAD_NAME_LIST) +
                2 * sizeof(MINIDUMP_THREAD_NAME));

  const MINIDUMP_THREAD_NAME_LIST* thread_name_list = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      GetThreadNameListStream(string_file.string(), &thread_name_list));

  EXPECT_EQ(thread_name_list->NumberOfThreadNames, 2u);

  MINIDUMP_THREAD_NAME expected = {};
  expected.ThreadId = kFirstThreadID;

  ASSERT_NO_FATAL_FAILURE(ExpectThreadName(&expected,
                                           &thread_name_list->ThreadNames[0],
                                           string_file.string(),
                                           kFirstThreadName));

  expected.ThreadId = kSecondThreadID;

  ASSERT_NO_FATAL_FAILURE(ExpectThreadName(&expected,
                                           &thread_name_list->ThreadNames[1],
                                           string_file.string(),
                                           kSecondThreadName));
}

TEST(MinidumpThreadNameListWriter, TwoThreads_SameNames) {
  MinidumpFileWriter minidump_file_writer;
  auto thread_list_writer = std::make_unique<MinidumpThreadNameListWriter>();

  constexpr uint32_t kFirstThreadID = 0x11111111;
  const std::string kThreadName = "ariadne";

  constexpr uint32_t kSecondThreadID = 0x22222222;

  auto thread_name_list_writer =
      std::make_unique<MinidumpThreadNameListWriter>();
  auto first_thread_name_writer = std::make_unique<MinidumpThreadNameWriter>();
  first_thread_name_writer->SetThreadId(kFirstThreadID);
  first_thread_name_writer->SetThreadName(kThreadName);
  thread_name_list_writer->AddThreadName(std::move(first_thread_name_writer));

  auto second_thread_name_writer = std::make_unique<MinidumpThreadNameWriter>();
  second_thread_name_writer->SetThreadId(kSecondThreadID);
  second_thread_name_writer->SetThreadName(kThreadName);
  thread_name_list_writer->AddThreadName(std::move(second_thread_name_writer));

  ASSERT_TRUE(
      minidump_file_writer.AddStream(std::move(thread_name_list_writer)));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  ASSERT_GT(string_file.string().size(),
            sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
                sizeof(MINIDUMP_THREAD_NAME_LIST) +
                2 * sizeof(MINIDUMP_THREAD_NAME));

  const MINIDUMP_THREAD_NAME_LIST* thread_name_list = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      GetThreadNameListStream(string_file.string(), &thread_name_list));

  EXPECT_EQ(thread_name_list->NumberOfThreadNames, 2u);

  MINIDUMP_THREAD_NAME expected = {};
  expected.ThreadId = kFirstThreadID;

  ASSERT_NO_FATAL_FAILURE(ExpectThreadName(&expected,
                                           &thread_name_list->ThreadNames[0],
                                           string_file.string(),
                                           kThreadName));

  expected.ThreadId = kSecondThreadID;

  ASSERT_NO_FATAL_FAILURE(ExpectThreadName(&expected,
                                           &thread_name_list->ThreadNames[1],
                                           string_file.string(),
                                           kThreadName));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
