// Copyright 2019 The Crashpad Authors
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

#include "test/ios/google_test_setup.h"

#import <UIKit/UIKit.h>

#include "base/check.h"
#include "gtest/gtest.h"
#include "test/ios/cptest_google_test_runner_delegate.h"

@interface UIApplication (Testing)
- (void)_terminateWithStatus:(int)status;
@end

namespace {

// The iOS watchdog timer will kill an app that doesn't spin the main event loop
// often enough. This uses a Google Test TestEventListener to spin the current
// loop after each test finishes. However, if any individual test takes too
// long, it is still possible that the app will get killed.
class IOSRunLoopListener : public testing::EmptyTestEventListener {
 public:
  virtual void OnTestEnd(const testing::TestInfo& test_info) {
    @autoreleasepool {
      // At the end of the test, spin the default loop for a moment.
      NSDate* stop_date = [NSDate dateWithTimeIntervalSinceNow:0.001];
      [[NSRunLoop currentRunLoop] runUntilDate:stop_date];
    }
  }
};

void RegisterTestEndListener() {
  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new IOSRunLoopListener);
}

}  // namespace

@interface CPTestUnitTestApplicationDelegate
    : NSObject <CPTestGoogleTestRunnerDelegate>
@property(nonatomic, readwrite, strong) UIWindow* window;
- (void)runTests;
@end

@implementation CPTestUnitTestApplicationDelegate

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
  self.window.backgroundColor = UIColor.whiteColor;
  [self.window makeKeyAndVisible];

  UIViewController* controller = [[UIViewController alloc] init];
  [self.window setRootViewController:controller];

  // Add a label with the app name.
  UILabel* label = [[UILabel alloc] initWithFrame:controller.view.bounds];
  label.text = [[NSProcessInfo processInfo] processName];
  label.textAlignment = NSTextAlignmentCenter;
  label.textColor = UIColor.blackColor;
  [controller.view addSubview:label];

  // Queue up the test run.
  if (![self supportsRunningGoogleTestsWithXCTest]) {
    // When running in XCTest mode, XCTest will invoke |runGoogleTest| directly.
    // Otherwise, schedule a call to |runTests|.
    [self performSelector:@selector(runTests) withObject:nil afterDelay:0.1];
  }

  return YES;
}

- (BOOL)supportsRunningGoogleTestsWithXCTest {
  return getenv("XCTestConfigurationFilePath") != nullptr;
}

- (int)runGoogleTests {
  RegisterTestEndListener();
  int exitStatus = RUN_ALL_TESTS();
  return exitStatus;
}

- (void)runTests {
  DCHECK(![self supportsRunningGoogleTestsWithXCTest]);

  int exitStatus = [self runGoogleTests];

  // If a test app is too fast, it will exit before Instruments has has a
  // a chance to initialize and no test results will be seen.
  // TODO(crbug.com/137010): Figure out how much time is actually needed, and
  // sleep only to make sure that much time has elapsed since launch.
  [NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:2.0]];
  self.window = nil;

  // Use the hidden selector to try and cleanly take down the app (otherwise
  // things can think the app crashed even on a zero exit status).
  UIApplication* application = [UIApplication sharedApplication];
  [application _terminateWithStatus:exitStatus];

  exit(exitStatus);
}

@end

namespace crashpad {
namespace test {

void IOSLaunchApplicationAndRunTests(int argc, char* argv[]) {
  @autoreleasepool {
    int exit_status = UIApplicationMain(
        argc, argv, nil, @"CPTestUnitTestApplicationDelegate");
    exit(exit_status);
  }
}

}  // namespace test
}  // namespace crashpad
