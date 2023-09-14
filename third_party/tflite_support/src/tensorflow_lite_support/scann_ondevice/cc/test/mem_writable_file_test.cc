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

#include "tensorflow_lite_support/scann_ondevice/cc/mem_writable_file.h"

#include "tensorflow_lite_support/cc/port/gmock.h"
#include "tensorflow_lite_support/cc/port/gtest.h"
#include "tensorflow_lite_support/cc/port/status_matchers.h"

namespace tflite {
namespace scann_ondevice {
namespace {

TEST(MemWritableFileTest, AppendsContent) {
  std::string buffer;
  SUPPORT_ASSERT_OK_AND_ASSIGN(auto mem_writable_file,
                       MemWritableFile::Create(&buffer));

  ASSERT_TRUE(mem_writable_file->Append("aaa").ok());
  EXPECT_EQ(buffer, "aaa");

  ASSERT_TRUE(mem_writable_file->Append("bbb").ok());
  EXPECT_EQ(buffer, "aaabbb");

  ASSERT_TRUE(mem_writable_file->Append("ccc").ok());
  ASSERT_TRUE(mem_writable_file->Flush().ok());
  ASSERT_TRUE(mem_writable_file->Sync().ok());
  EXPECT_EQ(buffer, "aaabbbccc");
}

}  // namespace
}  // namespace scann_ondevice
}  // namespace tflite
