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

#include "maldoca/ole/strings_extract.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::testing::UnorderedElementsAre;

TEST(StringsExtractTest, AsciiExtract) {
  constexpr char blob[] = "\x80\x00\xABTestAsciiString\x99\xAA";
  std::set<std::string> strings;
  maldoca::GetStrings(absl::string_view(blob, sizeof(blob) - 1), 4,
                                &strings);
  EXPECT_THAT(strings, UnorderedElementsAre("TestAsciiString"));
}

TEST(StringsExtractTest, AsciiExtractEnd) {
  constexpr char blob[] = "\x80\x00\xABTestAsciiString";
  std::set<std::string> strings;
  maldoca::GetStrings(absl::string_view(blob, sizeof(blob) - 1), 4,
                                &strings);
  EXPECT_THAT(strings, UnorderedElementsAre("TestAsciiString"));
}

TEST(StringsExtractTest, AsciiExtractBegin) {
  constexpr char blob[] = "TestAsciiString\x99\xAA\x80\x00\xAB";
  std::set<std::string> strings;
  maldoca::GetStrings(absl::string_view(blob, sizeof(blob) - 1), 4,
                                &strings);
  EXPECT_THAT(strings, UnorderedElementsAre("TestAsciiString"));
}

TEST(StringsExtractTest, UnicodeExtract) {
  constexpr char blob[] =
      "\x80\x00\xABT\0e\0s\0t\0U\0n\0i\0c\0o\0d\0e\0S\0t\0r\0i\0n\0g\0\x99\xAA";
  std::set<std::string> strings;
  maldoca::GetStrings(absl::string_view(blob, sizeof(blob) - 1), 4,
                                &strings);
  EXPECT_THAT(strings, UnorderedElementsAre("TestUnicodeString"));
}

TEST(StringsExtractTest, UnicodeExtractEnd) {
  constexpr char blob[] =
      "\x80\x00\xABT\0e\0s\0t\0U\0n\0i\0c\0o\0d\0e\0S\0t\0r\0i\0n\0g\0";
  std::set<std::string> strings;
  maldoca::GetStrings(absl::string_view(blob, sizeof(blob) - 1), 4,
                                &strings);
  EXPECT_THAT(strings, UnorderedElementsAre("TestUnicodeString"));
}

TEST(StringsExtractTest, UnicodeExtractBegin) {
  constexpr char blob[] =
      "T\0e\0s\0t\0U\0n\0i\0c\0o\0d\0e\0S\0t\0r\0i\0n\0g\0\x99\xAA\x80\x00\xAB";
  std::set<std::string> strings;
  maldoca::GetStrings(absl::string_view(blob, sizeof(blob) - 1), 4,
                                &strings);
  EXPECT_THAT(strings, UnorderedElementsAre("TestUnicodeString"));
}

TEST(StringsExtractTest, MixedExtract) {
  constexpr char blob[] =
      "\x80\x00\xABTestAsciiString\x99\xAAT\0e\0s\0t\0U\0n\0i\0c\0o\0d\0e\0S\0t"
      "\0r\0i\0n\0g\0\x99\xAA\x80\x00\xAB";
  std::set<std::string> strings;
  maldoca::GetStrings(absl::string_view(blob, sizeof(blob) - 1), 4,
                                &strings);
  EXPECT_THAT(strings,
              UnorderedElementsAre("TestAsciiString", "TestUnicodeString"));
}

}  // namespace
