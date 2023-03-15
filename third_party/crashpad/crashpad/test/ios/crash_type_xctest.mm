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

#import <XCTest/XCTest.h>
#include <objc/runtime.h>

#include <vector>

#import "Service/Sources/EDOClientService.h"
#include "build/build_config.h"
#include "client/length_delimited_ring_buffer.h"
#import "test/ios/host/cptest_shared_object.h"
#include "util/mach/exception_types.h"
#include "util/mach/mach_extensions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CPTestTestCase : XCTestCase {
  XCUIApplication* app_;
  CPTestSharedObject* rootObject_;
}
@end

@implementation CPTestTestCase

+ (void)setUp {
  [CPTestTestCase swizzleHandleCrashUnderSymbol];
  [CPTestTestCase swizleMayTerminateOutOfBandWithoutCrashReport];

  // Override EDO default error handler.  Without this, the default EDO error
  // handler will throw an error and fail the test.
  EDOSetClientErrorHandler(^(NSError* error){
      // Do nothing.
  });
}

// Swizzle away the -[XCUIApplicationImpl handleCrashUnderSymbol:] callback.
// Without this, any time the host app is intentionally crashed, the test is
// immediately failed.
+ (void)swizzleHandleCrashUnderSymbol {
  SEL originalSelector = NSSelectorFromString(@"handleCrashUnderSymbol:");
  SEL swizzledSelector = @selector(handleCrashUnderSymbol:);
  Method originalMethod = class_getInstanceMethod(
      objc_getClass("XCUIApplicationImpl"), originalSelector);
  Method swizzledMethod =
      class_getInstanceMethod([self class], swizzledSelector);
  method_exchangeImplementations(originalMethod, swizzledMethod);
}

// Swizzle away the time consuming 'Checking for crash reports corresponding to'
// from -[XCUIApplicationProcess swizleMayTerminateOutOfBandWithoutCrashReport]
// that is unnecessary for these tests.
+ (void)swizleMayTerminateOutOfBandWithoutCrashReport {
  SEL originalSelector =
      NSSelectorFromString(@"mayTerminateOutOfBandWithoutCrashReport");
  SEL swizzledSelector = @selector(mayTerminateOutOfBandWithoutCrashReport);
  Method originalMethod = class_getInstanceMethod(
      objc_getClass("XCUIApplicationProcess"), originalSelector);
  Method swizzledMethod =
      class_getInstanceMethod([self class], swizzledSelector);
  method_exchangeImplementations(originalMethod, swizzledMethod);
}

// This gets called after tearDown, so there's no straightforward way to
// test that this is called. However, not swizzling this out will cause every
// crashing test to fail.
- (void)handleCrashUnderSymbol:(id)arg1 {
}

- (BOOL)mayTerminateOutOfBandWithoutCrashReport {
  return YES;
}

- (void)setUp {
  app_ = [[XCUIApplication alloc] init];
  [app_ launch];
  rootObject_ = [EDOClientService rootObjectWithPort:12345];
  [rootObject_ clearPendingReports];
  XCTAssertEqual([rootObject_ pendingReportCount], 0);
  XCTAssertTrue(app_.state == XCUIApplicationStateRunningForeground);
}

- (void)verifyCrashReportException:(uint32_t)exception {
  // Confirm the app is not running.
  XCTAssertTrue([app_ waitForState:XCUIApplicationStateNotRunning timeout:15]);
  XCTAssertTrue(app_.state == XCUIApplicationStateNotRunning);

  // Restart app to get the report signal.
  [app_ launch];
  XCTAssertTrue(app_.state == XCUIApplicationStateRunningForeground);
  rootObject_ = [EDOClientService rootObjectWithPort:12345];
  XCTAssertEqual([rootObject_ pendingReportCount], 1);
  NSNumber* report_exception;
  XCTAssertTrue([rootObject_ pendingReportException:&report_exception]);
  XCTAssertEqual(report_exception.unsignedIntValue, exception);

  NSString* rawLogContents = [rootObject_ rawLogContents];
  XCTAssertFalse([rawLogContents containsString:@"allocator used in handler."]);
}

- (void)testEDO {
  NSString* result = [rootObject_ testEDO];
  XCTAssertEqualObjects(result, @"crashpad");
}

- (void)testKillAbort {
  [rootObject_ crashKillAbort];
  [self verifyCrashReportException:EXC_SOFT_SIGNAL];
  NSNumber* report_exception;
  XCTAssertTrue([rootObject_ pendingReportExceptionInfo:&report_exception]);
  XCTAssertEqual(report_exception.intValue, SIGABRT);
}

- (void)testTrap {
  [rootObject_ crashTrap];
#if defined(ARCH_CPU_X86_64)
  [self verifyCrashReportException:EXC_BAD_INSTRUCTION];
#elif defined(ARCH_CPU_ARM64)
  [self verifyCrashReportException:EXC_BREAKPOINT];
#else
#error Port to your CPU architecture
#endif
}

- (void)testAbort {
  [rootObject_ crashAbort];
  [self verifyCrashReportException:EXC_SOFT_SIGNAL];
  NSNumber* report_exception;
  XCTAssertTrue([rootObject_ pendingReportExceptionInfo:&report_exception]);
  XCTAssertEqual(report_exception.intValue, SIGABRT);
}

- (void)testBadAccess {
  [rootObject_ crashBadAccess];
  [self verifyCrashReportException:EXC_BAD_ACCESS];
}

- (void)testException {
  [rootObject_ crashException];
  [self verifyCrashReportException:EXC_SOFT_SIGNAL];
  NSNumber* report_exception;
  XCTAssertTrue([rootObject_ pendingReportExceptionInfo:&report_exception]);
  XCTAssertEqual(report_exception.intValue, SIGABRT);
}

- (void)testNSException {
  [rootObject_ crashNSException];
  [self verifyCrashReportException:crashpad::kMachExceptionFromNSException];
  NSDictionary* dict = [rootObject_ getAnnotations];
  NSString* userInfo =
      [dict[@"objects"][0] valueForKeyPath:@"exceptionUserInfo"];
  XCTAssertTrue([userInfo containsString:@"Error Object=<CPTestSharedObject"]);
  XCTAssertTrue([[dict[@"objects"][1] valueForKeyPath:@"exceptionReason"]
      isEqualToString:@"Intentionally throwing error."]);
  XCTAssertTrue([[dict[@"objects"][2] valueForKeyPath:@"exceptionName"]
      isEqualToString:@"NSInternalInconsistencyException"]);
}

- (void)testNotAnNSException {
  [rootObject_ crashNotAnNSException];
  // When @throwing something other than an NSException the
  // UncaughtExceptionHandler is not called, so the application SIGABRTs.
  [self verifyCrashReportException:EXC_SOFT_SIGNAL];
  NSNumber* report_exception;
  XCTAssertTrue([rootObject_ pendingReportExceptionInfo:&report_exception]);
  XCTAssertEqual(report_exception.intValue, SIGABRT);
}

- (void)testUnhandledNSException {
  [rootObject_ crashUnhandledNSException];
  [self verifyCrashReportException:crashpad::kMachExceptionFromNSException];
  NSDictionary* dict = [rootObject_ getAnnotations];
  NSString* uncaught_flag =
      [dict[@"objects"][0] valueForKeyPath:@"UncaughtNSException"];
  XCTAssertTrue([uncaught_flag containsString:@"true"]);
  NSString* userInfo =
      [dict[@"objects"][1] valueForKeyPath:@"exceptionUserInfo"];
  XCTAssertTrue([userInfo containsString:@"Error Object=<CPTestSharedObject"]);
  XCTAssertTrue([[dict[@"objects"][2] valueForKeyPath:@"exceptionReason"]
      isEqualToString:@"Intentionally throwing error."]);
  XCTAssertTrue([[dict[@"objects"][3] valueForKeyPath:@"exceptionName"]
      isEqualToString:@"NSInternalInconsistencyException"]);
}

- (void)testcrashUnrecognizedSelectorAfterDelay {
  [rootObject_ crashUnrecognizedSelectorAfterDelay];
  [self verifyCrashReportException:crashpad::kMachExceptionFromNSException];
  NSDictionary* dict = [rootObject_ getAnnotations];
  XCTAssertTrue([[dict[@"objects"][0] valueForKeyPath:@"exceptionReason"]
      containsString:
          @"CPTestSharedObject does_not_exist]: unrecognized selector"]);
  XCTAssertTrue([[dict[@"objects"][1] valueForKeyPath:@"exceptionName"]
      isEqualToString:@"NSInvalidArgumentException"]);
}

- (void)testCatchUIGestureEnvironmentNSException {
  // Tap the button with the string UIGestureEnvironmentException.
  [app_.buttons[@"UIGestureEnvironmentException"] tap];
  [self verifyCrashReportException:crashpad::kMachExceptionFromNSException];
  NSDictionary* dict = [rootObject_ getAnnotations];
  XCTAssertTrue([[dict[@"objects"][0] valueForKeyPath:@"exceptionReason"]
      containsString:@"NSArray0 objectAtIndex:]: index 42 beyond bounds"]);
  XCTAssertTrue([[dict[@"objects"][1] valueForKeyPath:@"exceptionName"]
      isEqualToString:@"NSRangeException"]);
}

- (void)testCatchNSException {
  [rootObject_ catchNSException];

  // The app should not crash
  XCTAssertTrue(app_.state == XCUIApplicationStateRunningForeground);

  // No report should be generated.
  [rootObject_ processIntermediateDumps];
  XCTAssertEqual([rootObject_ pendingReportCount], 0);
}

- (void)testCrashCoreAutoLayoutSinkhole {
  [rootObject_ crashCoreAutoLayoutSinkhole];
  [self verifyCrashReportException:crashpad::kMachExceptionFromNSException];
  NSDictionary* dict = [rootObject_ getAnnotations];
  XCTAssertTrue([[dict[@"objects"][0] valueForKeyPath:@"exceptionReason"]
      containsString:@"Unable to activate constraint with anchors"]);
  XCTAssertTrue([[dict[@"objects"][1] valueForKeyPath:@"exceptionName"]
      isEqualToString:@"NSGenericException"]);
}

- (void)testRecursion {
  [rootObject_ crashRecursion];
  [self verifyCrashReportException:EXC_BAD_ACCESS];
}

- (void)testClientAnnotations {
  [rootObject_ crashKillAbort];

  // Set app launch args to trigger different client annotations.
  NSArray<NSString*>* old_args = app_.launchArguments;
  app_.launchArguments = @[ @"--alternate-client-annotations" ];
  [self verifyCrashReportException:EXC_SOFT_SIGNAL];
  NSNumber* report_exception;
  XCTAssertTrue([rootObject_ pendingReportExceptionInfo:&report_exception]);
  XCTAssertEqual(report_exception.intValue, SIGABRT);

  app_.launchArguments = old_args;

  // Confirm the initial crash took the standard annotations.
  NSDictionary* dict = [rootObject_ getProcessAnnotations];
  XCTAssertTrue([dict[@"crashpad"] isEqualToString:@"yes"]);
  XCTAssertTrue([dict[@"plat"] isEqualToString:@"iOS"]);
  XCTAssertTrue([dict[@"prod"] isEqualToString:@"xcuitest"]);
  XCTAssertTrue([dict[@"ver"] isEqualToString:@"1"]);

  // Confirm passing alternate client annotation args works.
  [rootObject_ clearPendingReports];
  [rootObject_ crashKillAbort];
  [self verifyCrashReportException:EXC_SOFT_SIGNAL];
  XCTAssertTrue([rootObject_ pendingReportExceptionInfo:&report_exception]);
  XCTAssertEqual(report_exception.intValue, SIGABRT);

  dict = [rootObject_ getProcessAnnotations];
  XCTAssertTrue([dict[@"crashpad"] isEqualToString:@"no"]);
  XCTAssertTrue([dict[@"plat"] isEqualToString:@"macOS"]);
  XCTAssertTrue([dict[@"prod"] isEqualToString:@"some_app"]);
  XCTAssertTrue([dict[@"ver"] isEqualToString:@"42"]);
}

#if TARGET_OS_SIMULATOR
- (void)testCrashWithCrashInfoMessage {
  if (@available(iOS 15.0, *)) {
    // Figure out how to test this on iOS15.
    return;
  }
  [rootObject_ crashWithCrashInfoMessage];
  [self verifyCrashReportException:EXC_BAD_ACCESS];
  NSDictionary* dict = [rootObject_ getAnnotations];
  NSString* dyldMessage = dict[@"vector"][0];
  XCTAssertTrue([dyldMessage isEqualToString:@"dyld: in dlsym()"]);
}
#endif

// TODO(justincohen): Codesign crashy_initializer.so so it can run on devices.
#if TARGET_OS_SIMULATOR
- (void)testCrashWithDyldErrorString {
  if (@available(iOS 15.0, *)) {
    // iOS 15 uses dyld4, which doesn't use CRSetCrashLogMessage2
    return;
  }
  [rootObject_ crashWithDyldErrorString];
#if defined(ARCH_CPU_X86_64)
  [self verifyCrashReportException:EXC_BAD_INSTRUCTION];
#elif defined(ARCH_CPU_ARM64)
  [self verifyCrashReportException:EXC_BREAKPOINT];
#else
#error Port to your CPU architecture
#endif
  NSArray* vector = [rootObject_ getAnnotations][@"vector"];
  // This message is set by dyld-353.2.1/src/ImageLoaderMachO.cpp
  // ImageLoaderMachO::doInitialization().
  NSString* module = @"crashpad_snapshot_test_module_crashy_initializer.so";
  XCTAssertTrue([vector[0] hasSuffix:module]);
}
#endif

- (void)testCrashWithAnnotations {
  [rootObject_ crashWithAnnotations];
  [self verifyCrashReportException:EXC_SOFT_SIGNAL];
  NSNumber* report_exception;
  XCTAssertTrue([rootObject_ pendingReportExceptionInfo:&report_exception]);
  XCTAssertEqual(report_exception.intValue, SIGABRT);

  NSDictionary* dict = [rootObject_ getAnnotations];
  NSDictionary* simpleMap = dict[@"simplemap"];
  XCTAssertTrue([simpleMap[@"#TEST# empty_value"] isEqualToString:@""]);
  XCTAssertTrue([simpleMap[@"#TEST# key"] isEqualToString:@"value"]);
  XCTAssertTrue([simpleMap[@"#TEST# longer"] isEqualToString:@"shorter"]);
  XCTAssertTrue([simpleMap[@"#TEST# pad"] isEqualToString:@"crash"]);
  XCTAssertTrue([simpleMap[@"#TEST# x"] isEqualToString:@"y"]);

  XCTAssertTrue([[dict[@"objects"][0] valueForKeyPath:@"#TEST# same-name"]
      isEqualToString:@"same-name 4"]);
  XCTAssertTrue([[dict[@"objects"][1] valueForKeyPath:@"#TEST# same-name"]
      isEqualToString:@"same-name 3"]);
  XCTAssertTrue([[dict[@"objects"][2] valueForKeyPath:@"#TEST# one"]
      isEqualToString:@"moocow"]);
  // Ensure `ring_buffer` is present but not `busy_ring_buffer`.
  XCTAssertEqual(1u, [dict[@"ringbuffers"] count]);
  NSData* ringBufferNSData =
      [dict[@"ringbuffers"][0] valueForKeyPath:@"#TEST# ring_buffer"];
  crashpad::RingBufferData ringBufferData;
  XCTAssertTrue(ringBufferData.DeserializeFromBuffer(ringBufferNSData.bytes,
                                                     ringBufferNSData.length));
  crashpad::LengthDelimitedRingBufferReader reader(ringBufferData);

  std::vector<uint8_t> ringBufferEntry;
  XCTAssertTrue(reader.Pop(ringBufferEntry));
  NSString* firstEntry = [[NSString alloc] initWithBytes:ringBufferEntry.data()
                                                  length:ringBufferEntry.size()
                                                encoding:NSUTF8StringEncoding];
  XCTAssertEqualObjects(firstEntry, @"hello");
  ringBufferEntry.clear();

  XCTAssertTrue(reader.Pop(ringBufferEntry));
  NSString* secondEntry = [[NSString alloc] initWithBytes:ringBufferEntry.data()
                                                   length:ringBufferEntry.size()
                                                 encoding:NSUTF8StringEncoding];
  XCTAssertEqualObjects(secondEntry, @"goodbye");
  ringBufferEntry.clear();

  XCTAssertFalse(reader.Pop(ringBufferEntry));
}

- (void)testDumpWithoutCrash {
  [rootObject_ generateDumpWithoutCrash:10 threads:3];

  // The app should not crash
  XCTAssertTrue(app_.state == XCUIApplicationStateRunningForeground);

  XCTAssertEqual([rootObject_ pendingReportCount], 30);
}

- (void)testSimultaneousCrash {
  [rootObject_ crashConcurrentSignalAndMach];

  // Confirm the app is not running.
  XCTAssertTrue([app_ waitForState:XCUIApplicationStateNotRunning timeout:15]);
  XCTAssertTrue(app_.state == XCUIApplicationStateNotRunning);

  [app_ launch];
  XCTAssertTrue(app_.state == XCUIApplicationStateRunningForeground);
  rootObject_ = [EDOClientService rootObjectWithPort:12345];
  XCTAssertEqual([rootObject_ pendingReportCount], 1);
}

- (void)testSimultaneousNSException {
  [rootObject_ catchConcurrentNSException];

  // The app should not crash
  XCTAssertTrue(app_.state == XCUIApplicationStateRunningForeground);

  // No report should be generated.
  [rootObject_ processIntermediateDumps];
  XCTAssertEqual([rootObject_ pendingReportCount], 0);
}

- (void)testCrashInHandlerReentrant {
  XCTAssertTrue(app_.state == XCUIApplicationStateRunningForeground);
  rootObject_ = [EDOClientService rootObjectWithPort:12345];

  [rootObject_ crashInHandlerReentrant];

  // Confirm the app is not running.
  XCTAssertTrue([app_ waitForState:XCUIApplicationStateNotRunning timeout:15]);
  XCTAssertTrue(app_.state == XCUIApplicationStateNotRunning);

  [app_ launch];
  XCTAssertTrue(app_.state == XCUIApplicationStateRunningForeground);
  rootObject_ = [EDOClientService rootObjectWithPort:12345];

  XCTAssertEqual([rootObject_ pendingReportCount], 0);

  NSString* rawLogContents = [rootObject_ rawLogContents];
  NSString* errmsg = @"Cannot DumpExceptionFromSignal without writer";
  XCTAssertTrue([rawLogContents containsString:errmsg]);
}

- (void)testFailureWhenHandlerAllocates {
  XCTAssertTrue(app_.state == XCUIApplicationStateRunningForeground);
  rootObject_ = [EDOClientService rootObjectWithPort:12345];

  [rootObject_ allocateWithForbiddenAllocators];

  // Confirm the app is not running.
  XCTAssertTrue([app_ waitForState:XCUIApplicationStateNotRunning timeout:15]);
  XCTAssertTrue(app_.state == XCUIApplicationStateNotRunning);

  [app_ launch];
  XCTAssertTrue(app_.state == XCUIApplicationStateRunningForeground);
  rootObject_ = [EDOClientService rootObjectWithPort:12345];

  XCTAssertEqual([rootObject_ pendingReportCount], 0);

  NSString* rawLogContents = [rootObject_ rawLogContents];
  XCTAssertTrue([rawLogContents containsString:@"allocator used in handler."]);
}

@end
