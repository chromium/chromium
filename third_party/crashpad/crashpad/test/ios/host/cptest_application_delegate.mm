// Copyright 2020 The Crashpad Authors. All rights reserved.
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

#import "test/ios/host/cptest_application_delegate.h"

#include <dispatch/dispatch.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <vector>

#import "Service/Sources/EDOHostNamingService.h"
#import "Service/Sources/EDOHostService.h"
#include "client/crashpad_client.h"
#import "test/ios/host/cptest_crash_view_controller.h"
#import "test/ios/host/cptest_shared_object.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CPTestApplicationDelegate

@synthesize window = _window;

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  // Start up crashpad.
  crashpad::CrashpadClient client;
  client.StartCrashpadInProcessHandler(base::FilePath(), "", {});

  self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  [self.window makeKeyAndVisible];
  self.window.backgroundColor = UIColor.greenColor;

  CPTestCrashViewController* controller =
      [[CPTestCrashViewController alloc] init];
  self.window.rootViewController = controller;

  // Start up EDO.
  [EDOHostService serviceWithPort:12345
                       rootObject:[[CPTestSharedObject alloc] init]
                            queue:dispatch_get_main_queue()];
  return YES;
}

@end

@implementation CPTestSharedObject

- (NSString*)testEDO {
  return @"crashpad";
}

- (void)crashBadAccess {
  strcpy(nullptr, "bla");
}

- (void)crashKillAbort {
  kill(getpid(), SIGABRT);
}

- (void)crashSegv {
  long* zero = nullptr;
  *zero = 0xc045004d;
}

- (void)crashTrap {
  __builtin_trap();
}

- (void)crashAbort {
  abort();
}

- (void)crashException {
  std::vector<int> empty_vector = {};
  empty_vector.at(42);
}

- (void)crashNSException {
  // EDO has its own sinkhole.
  dispatch_async(dispatch_get_main_queue(), ^{
    NSArray* empty_array = @[];
    [empty_array objectAtIndex:42];
  });
}

- (void)catchNSException {
  @try {
    NSArray* empty_array = @[];
    [empty_array objectAtIndex:42];
  } @catch (NSException* exception) {
  } @finally {
  }
}

- (void)crashUnreocgnizedSelectorAfterDelay {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundeclared-selector"
  [self performSelector:@selector(does_not_exist) withObject:nil afterDelay:1];
#pragma clang diagnostic pop
}

- (void)recurse {
  [self recurse];
}

- (void)crashRecursion {
  [self recurse];
}

@end
