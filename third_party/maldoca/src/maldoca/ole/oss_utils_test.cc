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

#include "maldoca/ole/oss_utils.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "maldoca/base/logging.h"

namespace maldoca {
namespace utils {
namespace {
// use this class to access protected method
class TestBufferToUtf8 : public BufferToUtf8 {
 public:
  TestBufferToUtf8() : BufferToUtf8("dummy") {}
  bool Init(const char* encode_name) override { return true; }
  bool TestConvertLatin1BufferToUTF8String(absl::string_view input,
                                           std::string* out_str,
                                           int* bytes_consumed,
                                           int* bytes_filled,
                                           int* error_char_count) {
    return ConvertLatin1BufferToUTF8String(input, out_str, bytes_consumed,
                                           bytes_filled, error_char_count);
  }
  bool TestConvertCp1251BufferToUTF8String(absl::string_view input,
                                           std::string* out_str,
                                           int* bytes_consumed,
                                           int* bytes_filled,
                                           int* error_char_count) {
    return ConvertCp1251BufferToUTF8String(input, out_str, bytes_consumed,
                                           bytes_filled, error_char_count);
  }
  bool TestConvertCp1252BufferToUTF8String(absl::string_view input,
                                           std::string* out_str,
                                           int* bytes_consumed,
                                           int* bytes_filled,
                                           int* error_char_count) {
    return ConvertCp1252BufferToUTF8String(input, out_str, bytes_consumed,
                                           bytes_filled, error_char_count);
  }
  void TestConvertEncodingBufferToUTF8String(absl::string_view input,
                                             const char* encode_name,
                                             absl::string_view expected_output,
                                             const int expected_bytes_consumed,
                                             const int expected_bytes_filled,
                                             const int expected_error_cnt,
                                             const bool expected_ret_value) {
    int bytes_consumed = 0;
    int bytes_filled = 0;
    int error_cnt = 0;
    std::string output;
    bool ret = utils::ConvertEncodingBufferToUTF8String(
        input, encode_name, &output, &bytes_consumed, &bytes_filled,
        &error_cnt);

    EXPECT_EQ(ret, expected_ret_value);
    EXPECT_EQ(output, expected_output);
    EXPECT_EQ(bytes_consumed, expected_bytes_consumed);
    EXPECT_EQ(bytes_filled, expected_bytes_filled);
    EXPECT_EQ(error_cnt, expected_error_cnt);
    EXPECT_EQ(output.size(), expected_bytes_filled);
  }
};

TEST(BufferToUtf8, InternalConvertLatin1BufferToUTF8String) {
  std::string input;
  // Create all > 0 chars in a string
  for (int i = 1; i < 256; ++i) {
    input.push_back(i);
  }
  int bytes_consumed = 0;
  int bytes_filled = 0;
  int error_cnt = 0;
  std::string output;
  TestBufferToUtf8 tester;
  tester.SetMaxError(100);
  ASSERT_TRUE(tester.TestConvertLatin1BufferToUTF8String(
      input, &output, &bytes_consumed, &bytes_filled, &error_cnt));

  std::string expected =
      "\x1\x2\x3\x4\x5\x6\x7\x8\x9\xa\xb\xc\xd\xe\xf\x10\x11\x12\x13\x14\x15"
      "\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x27"
      "\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39"
      "\x3a\x3b\x3c\x3d\x3e\x3f\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4a\x4b"
      "\x4c\x4d\x4e\x4f\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5a\x5b\x5c\x5d"
      "\x5e\x5f\x60\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f"
      "\x70\x71\x72\x73\x74\x75\x76\x77\x78\x79\x7a\x7b\x7c\x7d\x7e\x7f\xc2\x80"
      "\xc2\x81\xc2\x82\xc2\x83\xc2\x84\xc2\x85\xc2\x86\xc2\x87\xc2\x88\xc2\x89"
      "\xc2\x8a\xc2\x8b\xc2\x8c\xc2\x8d\xc2\x8e\xc2\x8f\xc2\x90\xc2\x91\xc2\x92"
      "\xc2\x93\xc2\x94\xc2\x95\xc2\x96\xc2\x97\xc2\x98\xc2\x99\xc2\x9a\xc2\x9b"
      "\xc2\x9c\xc2\x9d\xc2\x9e\xc2\x9f\xc2\xa0\xc2\xa1\xc2\xa2\xc2\xa3\xc2\xa4"
      "\xc2\xa5\xc2\xa6\xc2\xa7\xc2\xa8\xc2\xa9\xc2\xaa\xc2\xab\xc2\xac\xc2\xad"
      "\xc2\xae\xc2\xaf\xc2\xb0\xc2\xb1\xc2\xb2\xc2\xb3\xc2\xb4\xc2\xb5\xc2\xb6"
      "\xc2\xb7\xc2\xb8\xc2\xb9\xc2\xba\xc2\xbb\xc2\xbc\xc2\xbd\xc2\xbe\xc2\xbf"
      "\xc3\x80\xc3\x81\xc3\x82\xc3\x83\xc3\x84\xc3\x85\xc3\x86\xc3\x87\xc3\x88"
      "\xc3\x89\xc3\x8a\xc3\x8b\xc3\x8c\xc3\x8d\xc3\x8e\xc3\x8f\xc3\x90\xc3\x91"
      "\xc3\x92\xc3\x93\xc3\x94\xc3\x95\xc3\x96\xc3\x97\xc3\x98\xc3\x99\xc3\x9a"
      "\xc3\x9b\xc3\x9c\xc3\x9d\xc3\x9e\xc3\x9f\xc3\xa0\xc3\xa1\xc3\xa2\xc3\xa3"
      "\xc3\xa4\xc3\xa5\xc3\xa6\xc3\xa7\xc3\xa8\xc3\xa9\xc3\xaa\xc3\xab\xc3\xac"
      "\xc3\xad\xc3\xae\xc3\xaf\xc3\xb0\xc3\xb1\xc3\xb2\xc3\xb3\xc3\xb4\xc3\xb5"
      "\xc3\xb6\xc3\xb7\xc3\xb8\xc3\xb9\xc3\xba\xc3\xbb\xc3\xbc\xc3\xbd\xc3\xbe"
      "\xc3\xbf";

  EXPECT_EQ(expected, output);
  EXPECT_EQ(bytes_consumed, input.size());
  EXPECT_EQ(bytes_filled, expected.size());
  EXPECT_EQ(0, error_cnt);
}

TEST(BufferToUtf8, InternalConvertCp1251BufferToUTF8String) {
  std::string input;
  // Create all > 0 chars in a string
  for (int i = 1; i < 256; ++i) {
    input.push_back(i);
  }
  int bytes_consumed = 0;
  int bytes_filled = 0;
  int error_cnt = 0;
  std::string output;
  TestBufferToUtf8 tester;
  tester.SetMaxError(100);
  ASSERT_TRUE(tester.TestConvertCp1251BufferToUTF8String(
      input, &output, &bytes_consumed, &bytes_filled, &error_cnt));

  std::string expected =
      "\x1\x2\x3\x4\x5\x6\x7\x8\x9\xa\xb\xc\xd\xe\xf\x10\x11\x12\x13\x14\x15"
      "\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x27"
      "\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39"
      "\x3a\x3b\x3c\x3d\x3e\x3f\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4a\x4b"
      "\x4c\x4d\x4e\x4f\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5a\x5b\x5c\x5d"
      "\x5e\x5f\x60\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f"
      "\x70\x71\x72\x73\x74\x75\x76\x77\x78\x79\x7a\x7b\x7c\x7d\x7e\x7f\xd0\x82"
      "\xd0\x83\xe2\x80\x9a\xd1\x93\xe2\x80\x9e\xe2\x80\xa6\xe2\x80\xa0\xe2\x80"
      "\xa1\xe2\x82\xac\xe2\x80\xb0\xd0\x89\xe2\x80\xb9\xd0\x8a\xd0\x8c\xd0\x8b"
      "\xd0\x8f\xd1\x92\xe2\x80\x98\xe2\x80\x99\xe2\x80\x9c\xe2\x80\x9d\xe2\x80"
      "\xa2\xe2\x80\x93\xe2\x80\x94\xe2\x84\xa2\xd1\x99\xe2\x80\xba\xd1\x9a\xd1"
      "\x9c\xd1\x9b\xd1\x9f\xc2\xa0\xd0\x8e\xd1\x9e\xd0\x88\xc2\xa4\xd2\x90\xc2"
      "\xa6\xc2\xa7\xd0\x81\xc2\xa9\xd0\x84\xc2\xab\xc2\xac\xc2\xad\xc2\xae\xd0"
      "\x87\xc2\xb0\xc2\xb1\xd0\x86\xd1\x96\xd2\x91\xc2\xb5\xc2\xb6\xc2\xb7\xd1"
      "\x91\xe2\x84\x96\xd1\x94\xc2\xbb\xd1\x98\xd0\x85\xd1\x95\xd1\x97\xd0\x90"
      "\xd0\x91\xd0\x92\xd0\x93\xd0\x94\xd0\x95\xd0\x96\xd0\x97\xd0\x98\xd0\x99"
      "\xd0\x9a\xd0\x9b\xd0\x9c\xd0\x9d\xd0\x9e\xd0\x9f\xd0\xa0\xd0\xa1\xd0\xa2"
      "\xd0\xa3\xd0\xa4\xd0\xa5\xd0\xa6\xd0\xa7\xd0\xa8\xd0\xa9\xd0\xaa\xd0\xab"
      "\xd0\xac\xd0\xad\xd0\xae\xd0\xaf\xd0\xb0\xd0\xb1\xd0\xb2\xd0\xb3\xd0\xb4"
      "\xd0\xb5\xd0\xb6\xd0\xb7\xd0\xb8\xd0\xb9\xd0\xba\xd0\xbb\xd0\xbc\xd0\xbd"
      "\xd0\xbe\xd0\xbf\xd1\x80\xd1\x81\xd1\x82\xd1\x83\xd1\x84\xd1\x85\xd1\x86"
      "\xd1\x87\xd1\x88\xd1\x89\xd1\x8a\xd1\x8b\xd1\x8c\xd1\x8d\xd1\x8e\xd1"
      "\x8f";
  EXPECT_EQ(expected, output);
  EXPECT_EQ(bytes_consumed, input.size());
  EXPECT_EQ(bytes_filled, expected.size());
  EXPECT_EQ(1, error_cnt);
}

TEST(BufferToUtf8, InternalConvertCp1252BufferToUTF8String) {
  std::string input;
  // Create all > 0 chars in a string
  for (int i = 1; i < 256; ++i) {
    input.push_back(i);
  }
  int bytes_consumed = 0;
  int bytes_filled = 0;
  int error_cnt = 0;
  std::string output;
  TestBufferToUtf8 tester;
  tester.SetMaxError(100);
  ASSERT_TRUE(tester.TestConvertCp1252BufferToUTF8String(
      input, &output, &bytes_consumed, &bytes_filled, &error_cnt));

  std::string expected =
      "\x1\x2\x3\x4\x5\x6\x7\x8\x9\xa\xb\xc\xd\xe\xf\x10\x11\x12\x13\x14\x15"
      "\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x27"
      "\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39"
      "\x3a\x3b\x3c\x3d\x3e\x3f\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4a\x4b"
      "\x4c\x4d\x4e\x4f\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5a\x5b\x5c\x5d"
      "\x5e\x5f\x60\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f"
      "\x70\x71\x72\x73\x74\x75\x76\x77\x78\x79\x7a\x7b\x7c\x7d\x7e\x7f\xe2\x82"
      "\xac\xe2\x80\x9a\xc6\x92\xe2\x80\x9e\xe2\x80\xa6\xe2\x80\xa0\xe2\x80\xa1"
      "\xcb\x86\xe2\x80\xb0\xc5\xa0\xe2\x80\xb9\xc5\x92\xc5\xbd\xe2\x80\x98\xe2"
      "\x80\x99\xe2\x80\x9c\xe2\x80\x9d\xe2\x80\xa2\xe2\x80\x93\xe2\x80\x94\xcb"
      "\x9c\xe2\x84\xa2\xc5\xa1\xe2\x80\xba\xc5\x93\xc5\xbe\xc5\xb8\xc2\xa0\xc2"
      "\xa1\xc2\xa2\xc2\xa3\xc2\xa4\xc2\xa5\xc2\xa6\xc2\xa7\xc2\xa8\xc2\xa9\xc2"
      "\xaa\xc2\xab\xc2\xac\xc2\xad\xc2\xae\xc2\xaf\xc2\xb0\xc2\xb1\xc2\xb2\xc2"
      "\xb3\xc2\xb4\xc2\xb5\xc2\xb6\xc2\xb7\xc2\xb8\xc2\xb9\xc2\xba\xc2\xbb\xc2"
      "\xbc\xc2\xbd\xc2\xbe\xc2\xbf\xc3\x80\xc3\x81\xc3\x82\xc3\x83\xc3\x84\xc3"
      "\x85\xc3\x86\xc3\x87\xc3\x88\xc3\x89\xc3\x8a\xc3\x8b\xc3\x8c\xc3\x8d\xc3"
      "\x8e\xc3\x8f\xc3\x90\xc3\x91\xc3\x92\xc3\x93\xc3\x94\xc3\x95\xc3\x96\xc3"
      "\x97\xc3\x98\xc3\x99\xc3\x9a\xc3\x9b\xc3\x9c\xc3\x9d\xc3\x9e\xc3\x9f\xc3"
      "\xa0\xc3\xa1\xc3\xa2\xc3\xa3\xc3\xa4\xc3\xa5\xc3\xa6\xc3\xa7\xc3\xa8\xc3"
      "\xa9\xc3\xaa\xc3\xab\xc3\xac\xc3\xad\xc3\xae\xc3\xaf\xc3\xb0\xc3\xb1\xc3"
      "\xb2\xc3\xb3\xc3\xb4\xc3\xb5\xc3\xb6\xc3\xb7\xc3\xb8\xc3\xb9\xc3\xba\xc3"
      "\xbb\xc3\xbc\xc3\xbd\xc3\xbe\xc3\xbf";

  EXPECT_EQ(expected, output);
  EXPECT_EQ(bytes_consumed, input.size());
  EXPECT_EQ(bytes_filled, expected.size());
  EXPECT_EQ(5, error_cnt);
}

TEST(BufferToUtf8, Init) {
  BufferToUtf8 converter("UTF-16LE");
  EXPECT_TRUE(converter.IsValid());
  BufferToUtf8 converter2("non-existing encoding");
  EXPECT_FALSE(converter2.IsValid());
}

TEST(BufferToUtf8, ConvertEncodingBufferToUTF8String_EmptyString) {
  absl::string_view input = "";
  absl::string_view expected_output = "";
  TestBufferToUtf8 tester;
  tester.TestConvertEncodingBufferToUTF8String(input, "CP1252", expected_output,
                                               input.size(),
                                               expected_output.size(), 0, true);
}

TEST(BufferToUtf8, ConvertEncodingBufferToUTF8String_Latin1) {
  absl::string_view input = "abc";
  absl::string_view expected_output = "abc";
  TestBufferToUtf8 tester;
  tester.TestConvertEncodingBufferToUTF8String(input, "LATIN1", expected_output,
                                               input.size(),
                                               expected_output.size(), 0, true);
}

TEST(BufferToUtf8, ConvertEncodingBufferToUTF8String_UTF16) {
  absl::string_view input = "\x3d\xd8\x0c\xdc\x21";
  absl::string_view expected_output = "\xf0\x9f\x90\x8c";
  TestBufferToUtf8 tester;
  tester.TestConvertEncodingBufferToUTF8String(input, "UTF-16LE",
                                               expected_output, input.size(),
                                               expected_output.size(), 0, true);
}

}  // namespace
}  // namespace utils
}  // namespace maldoca

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
