// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/command_dispatcher.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "ui/base/test/cocoa_helper.h"

@interface TestCommandDispatchingWindow
    : CocoaTestHelperWindow <CommandDispatchingWindow> {
  CommandDispatcher* __strong _dispatcher;
}
@end

@implementation TestCommandDispatchingWindow

- (instancetype)initWithContentRect:(NSRect)contentRect {
  if ((self = [super initWithContentRect:contentRect])) {
    _dispatcher = [[CommandDispatcher alloc] initWithOwner:self];
  }
  return self;
}

- (CommandDispatcher*)commandDispatcher {
  return _dispatcher;
}

- (NSWindow<CommandDispatchingWindow>*)commandDispatchParent {
  return nil;
}

- (void)setCommandHandler:(id<UserInterfaceItemCommandHandler>)commandHandler {
}

- (BOOL)defaultPerformKeyEquivalent:(NSEvent*)event {
  return NO;
}

- (BOOL)defaultValidateUserInterfaceItem:
    (id<NSValidatedUserInterfaceItem>)item {
  return NO;
}

- (void)commandDispatch:(id)sender {
}

- (void)commandDispatchUsingKeyModifiers:(id)sender {
}

@end

namespace ui {

class CommandDispatcherTest : public CocoaTest {
 public:
  void SetUp() override {
    CocoaTest::SetUp();
    window_ = [[TestCommandDispatchingWindow alloc] init];
    key_event_ = [NSEvent keyEventWithType:NSEventTypeKeyDown
                                  location:NSZeroPoint
                             modifierFlags:0
                                 timestamp:0
                              windowNumber:window_.windowNumber
                                   context:nil
                                characters:@"a"
               charactersIgnoringModifiers:@"a"
                                 isARepeat:NO
                                   keyCode:0];
  }

  void TearDown() override {
    key_event_ = nil;
    [window_ close];
    window_ = nil;
    CocoaTest::TearDown();
  }

  TestCommandDispatchingWindow* __strong window_;
  NSEvent* __strong key_event_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
};

// Verifies that -redispatchKeyEvent: drops redispatched events (unhandled
// renderer events returned for system processing) when the window is no longer
// key, as they would otherwise be incorrectly redirected to the now-key window.
TEST_F(CommandDispatcherTest, RedispatchDropsEventIfWindowNotKey) {
  CommandDispatcher* dispatcher = [window_ commandDispatcher];
  window_.pretendIsKeyWindow = NO;
  EXPECT_FALSE(window_.isKeyWindow);
  EXPECT_FALSE([dispatcher redispatchKeyEvent:key_event_]);
}

// Tests that -redispatchKeyEvent: correctly redispatches events when the
// window is still key, allowing normal system handling of unhandled keys.
TEST_F(CommandDispatcherTest, RedispatchSendsEventIfWindowIsKey) {
  CommandDispatcher* dispatcher = [window_ commandDispatcher];
  window_.pretendIsKeyWindow = YES;
  EXPECT_TRUE(window_.isKeyWindow);
  EXPECT_TRUE([dispatcher redispatchKeyEvent:key_event_]);
}

// Verifies that -redispatchKeyEvent: drops events with no associated window
// if the dispatcher's owner is no longer the key window
// (https://crbug.com/517040438).
TEST_F(CommandDispatcherTest, RedispatchDropsEventIfNilWindowAndOwnerNotKey) {
  CommandDispatcher* dispatcher = [window_ commandDispatcher];
  window_.pretendIsKeyWindow = NO;
  EXPECT_FALSE(window_.isKeyWindow);

  NSEvent* nil_window_event = [NSEvent keyEventWithType:NSEventTypeKeyDown
                                               location:NSZeroPoint
                                          modifierFlags:0
                                              timestamp:0
                                           windowNumber:0
                                                context:nil
                                             characters:@"a"
                            charactersIgnoringModifiers:@"a"
                                              isARepeat:NO
                                                keyCode:0];
  EXPECT_EQ(nil_window_event.window, nil);
  EXPECT_FALSE([dispatcher redispatchKeyEvent:nil_window_event]);
}

}  // namespace ui
