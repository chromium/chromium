// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/command_dispatcher.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "ui/base/cocoa/default_command_dispatcher_delegate.h"
#import "ui/base/test/cocoa_helper.h"

@interface TestCommandDispatchingWindow
    : CocoaTestHelperWindow <CommandDispatchingWindow> {
  CommandDispatcher* __strong _dispatcher;
}
@property(nonatomic, weak)
    NSWindow<CommandDispatchingWindow>* commandDispatchParent;
// Stands in for the firstResponder: records and answers
// -defaultPerformKeyEquivalent:.
@property(nonatomic) BOOL firstResponderHandlesKeyEquivalent;
@property(nonatomic) BOOL firstResponderSawKeyEquivalent;
@end

@implementation TestCommandDispatchingWindow
@synthesize commandDispatchParent = _commandDispatchParent;
@synthesize firstResponderHandlesKeyEquivalent =
    _firstResponderHandlesKeyEquivalent;
@synthesize firstResponderSawKeyEquivalent = _firstResponderSawKeyEquivalent;

- (instancetype)initWithContentRect:(NSRect)contentRect {
  if ((self = [super initWithContentRect:contentRect])) {
    _dispatcher = [[CommandDispatcher alloc] initWithOwner:self];
  }
  return self;
}

- (CommandDispatcher*)commandDispatcher {
  return _dispatcher;
}

- (void)setCommandHandler:(id<UserInterfaceItemCommandHandler>)commandHandler {
}

- (BOOL)defaultPerformKeyEquivalent:(NSEvent*)event {
  _firstResponderSawKeyEquivalent = YES;
  return _firstResponderHandlesKeyEquivalent;
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

@interface TestCommandDispatcherDelegate : NSObject <CommandDispatcherDelegate>
// Result to return from -prePerformKeyEquivalent:window:. kHandled stands in
// for a reserved command, which ChromeCommandDispatcherDelegate consumes
// before the firstResponder.
@property(nonatomic) ui::PerformKeyEquivalentResult preResult;
// Window the pre-firstResponder stage most recently ran for, or nil.
@property(nonatomic, weak) NSWindow* windowSeenBeforeFirstResponder;
@end

@implementation TestCommandDispatcherDelegate
@synthesize preResult = _preResult;
@synthesize windowSeenBeforeFirstResponder = _windowSeenBeforeFirstResponder;

- (ui::PerformKeyEquivalentResult)prePerformKeyEquivalent:(NSEvent*)event
                                                   window:(NSWindow*)window {
  self.windowSeenBeforeFirstResponder = window;
  return self.preResult;
}

- (ui::PerformKeyEquivalentResult)postPerformKeyEquivalent:(NSEvent*)event
                                                    window:(NSWindow*)window
                                              isRedispatch:(BOOL)isRedispatch {
  return ui::PerformKeyEquivalentResult::kUnhandled;
}

@end

// Stands in for a RenderWidgetHostViewCocoa holding a Keyboard Lock: it has
// exclusive access to the event, so no delegate may preempt it.
@interface KeyLockedResponderView : NSView <CommandDispatcherTarget>
@end

@implementation KeyLockedResponderView

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (BOOL)isKeyLocked:(NSEvent*)event {
  return YES;
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

// A window with only DefaultCommandDispatcherDelegate forwards the
// pre-firstResponder stage to the command dispatch parent, so reserved
// commands still bypass this window's firstResponder
// (https://crbug.com/556432989).
TEST_F(CommandDispatcherTest, PreFirstResponderStageRunsOnDispatchParent) {
  TestCommandDispatchingWindow* parent =
      [[TestCommandDispatchingWindow alloc] init];
  TestCommandDispatcherDelegate* delegate =
      [[TestCommandDispatcherDelegate alloc] init];
  delegate.preResult = ui::PerformKeyEquivalentResult::kHandled;
  [parent commandDispatcher].delegate = delegate;

  DefaultCommandDispatcherDelegate* default_delegate =
      [[DefaultCommandDispatcherDelegate alloc] init];
  [window_ commandDispatcher].delegate = default_delegate;

  window_.commandDispatchParent = parent;
  // The firstResponder would consume the event, e.g. a renderer composing IME
  // text.
  window_.firstResponderHandlesKeyEquivalent = YES;

  EXPECT_TRUE([[window_ commandDispatcher] performKeyEquivalent:key_event_]);
  EXPECT_NSEQ(parent, delegate.windowSeenBeforeFirstResponder);
  EXPECT_FALSE(window_.firstResponderSawKeyEquivalent);

  [parent close];
}

// The parent's pre-firstResponder stage must not preempt the firstResponder
// for commands it declines to consume, e.g. non-reserved ones.
TEST_F(CommandDispatcherTest, DispatchParentDecliningLeavesFirstResponder) {
  TestCommandDispatchingWindow* parent =
      [[TestCommandDispatchingWindow alloc] init];
  TestCommandDispatcherDelegate* delegate =
      [[TestCommandDispatcherDelegate alloc] init];
  delegate.preResult = ui::PerformKeyEquivalentResult::kUnhandled;
  [parent commandDispatcher].delegate = delegate;

  DefaultCommandDispatcherDelegate* default_delegate =
      [[DefaultCommandDispatcherDelegate alloc] init];
  [window_ commandDispatcher].delegate = default_delegate;

  window_.commandDispatchParent = parent;
  window_.firstResponderHandlesKeyEquivalent = YES;

  EXPECT_TRUE([[window_ commandDispatcher] performKeyEquivalent:key_event_]);
  EXPECT_TRUE(window_.firstResponderSawKeyEquivalent);

  [parent close];
}

// A delegate inspects the firstResponder of the window that owns its
// dispatcher, so delegating this window's pre-firstResponder stage to the
// parent leaves a key lock held here unseen. Verifies the lock is honored
// anyway, keeping the firstResponder's exclusive access to the event.
TEST_F(CommandDispatcherTest, DispatchParentStageHonorsKeyLock) {
  TestCommandDispatchingWindow* parent =
      [[TestCommandDispatchingWindow alloc] init];
  TestCommandDispatcherDelegate* delegate =
      [[TestCommandDispatcherDelegate alloc] init];
  delegate.preResult = ui::PerformKeyEquivalentResult::kHandled;
  [parent commandDispatcher].delegate = delegate;

  DefaultCommandDispatcherDelegate* default_delegate =
      [[DefaultCommandDispatcherDelegate alloc] init];
  [window_ commandDispatcher].delegate = default_delegate;

  KeyLockedResponderView* locked_responder =
      [[KeyLockedResponderView alloc] init];
  [window_.contentView addSubview:locked_responder];
  ASSERT_TRUE([window_ makeFirstResponder:locked_responder]);

  window_.commandDispatchParent = parent;
  window_.firstResponderHandlesKeyEquivalent = YES;

  EXPECT_TRUE([[window_ commandDispatcher] performKeyEquivalent:key_event_]);
  EXPECT_FALSE(delegate.windowSeenBeforeFirstResponder);
  EXPECT_TRUE(window_.firstResponderSawKeyEquivalent);

  [parent close];
}

}  // namespace ui
