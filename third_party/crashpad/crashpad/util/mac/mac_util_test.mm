// Copyright 2014 The Crashpad Authors
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

#include "util/mac/mac_util.h"

#import <Foundation/Foundation.h>
#include <stdlib.h>

#include <string>

#include "base/strings/stringprintf.h"
#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

// Runs /usr/bin/sw_vers with a single argument, |argument|, and places the
// command’s standard output into |output| after stripping the trailing newline.
// Fatal Google Test assertions report tool failures, which the caller should
// check for with ASSERT_NO_FATAL_FAILURE() or testing::Test::HasFatalFailure().
void SwVers(NSString* argument, std::string* output) {
  @autoreleasepool {
    NSPipe* pipe = [[NSPipe alloc] init];
    NSTask* task = [[NSTask alloc] init];
    [task setStandardOutput:pipe];
    [task setLaunchPath:@"/usr/bin/sw_vers"];
    [task setArguments:@[ argument ]];

    @try {
      [task launch];
    }
    @catch (NSException* exception) {
      FAIL() << [[exception name] UTF8String] << ": "
             << [[exception reason] UTF8String];
    }

    NSData* data = [[pipe fileHandleForReading] readDataToEndOfFile];
    [task waitUntilExit];

    ASSERT_EQ([task terminationReason], NSTaskTerminationReasonExit);
    ASSERT_EQ([task terminationStatus], EXIT_SUCCESS);

    output->assign(reinterpret_cast<const char*>([data bytes]), [data length]);

    EXPECT_EQ(output->at(output->size() - 1), '\n');
    output->resize(output->size() - 1);
  }
}

TEST(MacUtil, MacOSVersionComponents) {
  int major;
  int minor;
  int bugfix;
  std::string build;
  std::string version_string;
  ASSERT_TRUE(
      MacOSVersionComponents(&major, &minor, &bugfix, &build, &version_string));

  EXPECT_GE(major, 10);
  EXPECT_LE(major, 99);
  EXPECT_GE(minor, 0);
  EXPECT_LE(minor, 99);
  EXPECT_GE(bugfix, 0);
  EXPECT_LE(bugfix, 99);

  std::string version;
  if (bugfix) {
    version = base::StringPrintf("%d.%d.%d", major, minor, bugfix);
  } else {
    // 10.x.0 releases report their version string as simply 10.x.
    version = base::StringPrintf("%d.%d", major, minor);
  }

  std::string expected_product_version;
  ASSERT_NO_FATAL_FAILURE(
      SwVers(@"-productVersion", &expected_product_version));

  EXPECT_EQ(version, expected_product_version);

  std::string expected_build_version;
  ASSERT_NO_FATAL_FAILURE(SwVers(@"-buildVersion", &expected_build_version));

  EXPECT_EQ(build, expected_build_version);

  std::string expected_product_name;
  ASSERT_NO_FATAL_FAILURE(SwVers(@"-productName", &expected_product_name));

  // Look for a space after the product name in the complete version string.
  expected_product_name += ' ';
  EXPECT_EQ(version_string.find(expected_product_name), 0u);
}

TEST(MacUtil, MacOSVersionNumber) {
  // Make sure that MacOSVersionNumber() and MacOSVersionComponents() agree. The
  // two have their own distinct implementations, and the latter was checked
  // against sw_vers above.
  int macos_version_number = MacOSVersionNumber();
  EXPECT_GE(macos_version_number, 10'00'00);
  EXPECT_LE(macos_version_number, 99'99'99);

  int major;
  int minor;
  int bugfix;
  std::string build;
  std::string version_string;
  ASSERT_TRUE(
      MacOSVersionComponents(&major, &minor, &bugfix, &build, &version_string));

  EXPECT_EQ(macos_version_number,
            major * 1'00'00 + minor * 1'00 +
                (macos_version_number >= 10'13'04 ? bugfix : 0));
}

TEST(MacUtil, MacModelAndBoard) {
  // There’s not much that can be done to test these, so just make sure they’re
  // not empty. The model could be compared against the parsed output of
  // “system_profiler SPHardwareDataType”, but the board doesn’t show up
  // anywhere other than the I/O Registry, and that’s exactly how
  // MacModelAndBoard() gets the data, so it wouldn’t be a very useful test.
  std::string model;
  std::string board;
  MacModelAndBoard(&model, &board);

  EXPECT_FALSE(model.empty());
  EXPECT_FALSE(board.empty());
}

}  // namespace
}  // namespace test
}  // namespace crashpad
