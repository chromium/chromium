// Copyright 2020 The Crashpad Authors
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

#import <Foundation/Foundation.h>
#include <objc/message.h>
#include <objc/runtime.h>

#include "gtest/gtest.h"
#include "testing/platform_test.h"

namespace crashpad {
namespace test {
namespace {

using IOSExceptionProcessor = PlatformTest;

TEST_F(IOSExceptionProcessor, SelectorExists) {
  IMP init_imp =
      class_getMethodImplementation(NSClassFromString(@"UIGestureEnvironment"),
                                    NSSelectorFromString(@"init"));

  IMP destruct_imp =
      class_getMethodImplementation(NSClassFromString(@"UIGestureEnvironment"),
                                    NSSelectorFromString(@".cxx_destruct"));

  // From 10.15.0 objc4-779.1/runtime/objc-class.mm
  // class_getMethodImplementation returns nil or _objc_msgForward on failure.
  ASSERT_TRUE(init_imp);
  EXPECT_NE(init_imp, _objc_msgForward);
  ASSERT_TRUE(destruct_imp);
  EXPECT_NE(destruct_imp, _objc_msgForward);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
