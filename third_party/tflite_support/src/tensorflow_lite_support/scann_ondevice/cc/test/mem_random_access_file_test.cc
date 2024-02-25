/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow_lite_support/scann_ondevice/cc/mem_random_access_file.h"

#include "leveldb/slice.h"  // from @com_google_leveldb
#include "tensorflow/lite/test_util.h"
#include "tensorflow_lite_support/cc/port/gmock.h"
#include "tensorflow_lite_support/cc/port/gtest.h"

namespace tflite {
namespace scann_ondevice {
namespace {

constexpr char kBufferData[] = "abcdef";
constexpr size_t kBufferSize = 6;

class MemRandomAccessFileTest : public tflite::testing::Test {
 public:
  MemRandomAccessFileTest() : file_(kBufferData, kBufferSize) {}

 protected:
  MemRandomAccessFile file_;
  leveldb::Slice result_;
};

TEST_F(MemRandomAccessFileTest, ReadFailsWithOutOfBoundsOffset) {
  EXPECT_TRUE(file_.Read(/*offset=*/7, /*n=*/1, &result_, /*scratch=*/nullptr)
                  .IsInvalidArgument());
}

TEST_F(MemRandomAccessFileTest, ReadSucceedsWithoutTruncation) {
  EXPECT_TRUE(
      file_.Read(/*offset=*/1, /*n=*/5, &result_, /*scratch=*/nullptr).ok());
  EXPECT_EQ("bcdef", result_.ToString());
}

TEST_F(MemRandomAccessFileTest, ReadSucceedsWithTruncation) {
  EXPECT_TRUE(
      file_.Read(/*offset=*/1, /*n=*/6, &result_, /*scratch=*/nullptr).ok());
  EXPECT_EQ("bcdef", result_.ToString());
}

TEST_F(MemRandomAccessFileTest, ReadSucceedsWithZeroLength) {
  EXPECT_TRUE(
      file_.Read(/*offset=*/1, /*n=*/0, &result_, /*scratch=*/nullptr).ok());
  EXPECT_EQ("", result_.ToString());
}

}  // namespace
}  // namespace scann_ondevice
}  // namespace tflite
