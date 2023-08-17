// Copyright 2016 Google Inc.
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
// limitations under the License.!

#include "init.h"

#include "common.h"
#include "testharness.h"

ABSL_FLAG(int32, int32_f, 10, "int32_flags");
ABSL_FLAG(bool, bool_f, false, "bool_flags");
ABSL_FLAG(int64, int64_f, 9223372036854775807LL, "int64_flags");
ABSL_FLAG(uint64, uint64_f, 18446744073709551615ULL, "uint64_flags");
ABSL_FLAG(double, double_f, 40.0, "double_flags");
ABSL_FLAG(std::string, string_f, "str", "string_flags");

ABSL_DECLARE_FLAG(bool, help);
ABSL_DECLARE_FLAG(bool, version);

using sentencepiece::ParseCommandLineFlags;

namespace absl {
TEST(FlagsTest, DefaultValueTest) {
  EXPECT_EQ(10, absl::GetFlag(FLAGS_int32_f));
  EXPECT_EQ(false, absl::GetFlag(FLAGS_bool_f));
  EXPECT_EQ(9223372036854775807LL, absl::GetFlag(FLAGS_int64_f));
  EXPECT_EQ(18446744073709551615ULL, absl::GetFlag(FLAGS_uint64_f));
  EXPECT_EQ(40.0, absl::GetFlag(FLAGS_double_f));
  EXPECT_EQ("str", absl::GetFlag(FLAGS_string_f));
}

TEST(FlagsTest, ParseCommandLineFlagsTest) {
  const char* kFlags[] = {"program",        "--int32_f=100",  "other1",
                          "--bool_f=true",  "--int64_f=200",  "--uint64_f=300",
                          "--double_f=400", "--string_f=foo", "other2",
                          "other3"};
  int argc = arraysize(kFlags);
  char** argv = const_cast<char**>(kFlags);
  ParseCommandLineFlags(kFlags[0], &argc, &argv);

  EXPECT_EQ(100, absl::GetFlag(FLAGS_int32_f));
  EXPECT_EQ(true, absl::GetFlag(FLAGS_bool_f));
  EXPECT_EQ(200, absl::GetFlag(FLAGS_int64_f));
  EXPECT_EQ(300, absl::GetFlag(FLAGS_uint64_f));
  EXPECT_EQ(400.0, absl::GetFlag(FLAGS_double_f));
  EXPECT_EQ("foo", absl::GetFlag(FLAGS_string_f));
  EXPECT_EQ(4, argc);
  EXPECT_EQ("program", std::string(argv[0]));
  EXPECT_EQ("other1", std::string(argv[1]));
  EXPECT_EQ("other2", std::string(argv[2]));
  EXPECT_EQ("other3", std::string(argv[3]));
}

TEST(FlagsTest, ParseCommandLineFlagsTest2) {
  const char* kFlags[] = {"program",       "--int32_f", "500",
                          "-int64_f=600",  "-uint64_f", "700",
                          "--bool_f=FALSE"};
  int argc = arraysize(kFlags);
  char** argv = const_cast<char**>(kFlags);
  ParseCommandLineFlags(kFlags[0], &argc, &argv);

  EXPECT_EQ(500, absl::GetFlag(FLAGS_int32_f));
  EXPECT_EQ(600, absl::GetFlag(FLAGS_int64_f));
  EXPECT_EQ(700, absl::GetFlag(FLAGS_uint64_f));
  EXPECT_FALSE(absl::GetFlag(FLAGS_bool_f));
  EXPECT_EQ(1, argc);
}

TEST(FlagsTest, ParseCommandLineFlagsTest3) {
  const char* kFlags[] = {"program", "--bool_f", "--int32_f", "800"};

  int argc = arraysize(kFlags);
  char** argv = const_cast<char**>(kFlags);
  ParseCommandLineFlags(kFlags[0], &argc, &argv);
  EXPECT_TRUE(absl::GetFlag(FLAGS_bool_f));
  EXPECT_EQ(800, absl::GetFlag(FLAGS_int32_f));
  EXPECT_EQ(1, argc);
}

#ifndef _USE_EXTERNAL_ABSL

TEST(FlagsTest, ParseCommandLineFlagsHelpTest) {
  const char* kFlags[] = {"program", "--help"};
  int argc = arraysize(kFlags);
  char** argv = const_cast<char**>(kFlags);
  EXPECT_DEATH(ParseCommandLineFlags(kFlags[0], &argc, &argv), "");
  absl::SetFlag(&FLAGS_help, false);
}

TEST(FlagsTest, ParseCommandLineFlagsVersionTest) {
  const char* kFlags[] = {"program", "--version"};
  int argc = arraysize(kFlags);
  char** argv = const_cast<char**>(kFlags);
  EXPECT_DEATH(ParseCommandLineFlags(kFlags[0], &argc, &argv), "");
  absl::SetFlag(&FLAGS_version, false);
}

TEST(FlagsTest, ParseCommandLineFlagsUnknownTest) {
  const char* kFlags[] = {"program", "--foo"};
  int argc = arraysize(kFlags);
  char** argv = const_cast<char**>(kFlags);
  EXPECT_DEATH(ParseCommandLineFlags(kFlags[0], &argc, &argv), "");
}

TEST(FlagsTest, ParseCommandLineFlagsInvalidBoolTest) {
  const char* kFlags[] = {"program", "--bool_f=X"};
  int argc = arraysize(kFlags);
  char** argv = const_cast<char**>(kFlags);
  EXPECT_DEATH(ParseCommandLineFlags(kFlags[0], &argc, &argv), "");
}

TEST(FlagsTest, ParseCommandLineFlagsEmptyStringArgs) {
  const char* kFlags[] = {"program", "--string_f="};
  int argc = arraysize(kFlags);
  char** argv = const_cast<char**>(kFlags);
  ParseCommandLineFlags(kFlags[0], &argc, &argv);
  EXPECT_EQ(1, argc);
  EXPECT_EQ("", absl::GetFlag(FLAGS_string_f));
}

TEST(FlagsTest, ParseCommandLineFlagsEmptyBoolArgs) {
  const char* kFlags[] = {"program", "--bool_f"};
  int argc = arraysize(kFlags);
  char** argv = const_cast<char**>(kFlags);
  ParseCommandLineFlags(kFlags[0], &argc, &argv);
  EXPECT_EQ(1, argc);
  EXPECT_TRUE(absl::GetFlag(FLAGS_bool_f));
}

TEST(FlagsTest, ParseCommandLineFlagsEmptyIntArgs) {
  const char* kFlags[] = {"program", "--int32_f"};
  int argc = arraysize(kFlags);
  char** argv = const_cast<char**>(kFlags);
  EXPECT_DEATH(ParseCommandLineFlags(kFlags[0], &argc, &argv), );
}
#endif  // _USE_EXTERNAL_ABSL
}  // namespace absl
