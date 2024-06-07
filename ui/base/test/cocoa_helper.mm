// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/test/cocoa_helper.h"

#include <objc/message.h>
#include <objc/runtime.h>

#include <set>
#include <vector>

#include "base/debug/debugger.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/mock_chrome_application_mac.h"
#include "base/test/test_timeouts.h"

@implementation CocoaTestHelperWindow

@synthesize pretendIsKeyWindow = _pretendIsKeyWindow;
@synthesize pretendIsOnActiveSpace = _pretendIsOnActiveSpace;
@synthesize pretendFullKeyboardAccessIsEnabled =
    _pretendFullKeyboardAccessIsEnabled;
@synthesize useDefaultConstraints = _useDefaultConstraints;

- (instancetype)initWithContentRect:(NSRect)contentRect {
  self = [super initWithContentRect:contentRect
                          styleMask:NSWindowStyleMaskBorderless
                            backing:NSBackingStoreBuffered
                              defer:NO];
  if (self) {
    _useDefaultConstraints = YES;
    _pretendIsOnActiveSpace = YES;
    self.releasedWhenClosed = NO;
  }
  return self;
}

- (instancetype)init {
  return [self initWithContentRect:NSMakeRect(0, 0, 800, 600)];
}

// Enables users to write debugging code to help diagnose failures to close
// CocoaTestHelperWindow. Debugging code is not usually committed in Chromium,
// but because it's difficult to correctly override retain/release in ARC, this
// is left in.
#if 0

+ (void)initialize {
  if (self == [CocoaTestHelperWindow self]) {
    Class test_class = [CocoaTestHelperWindow class];

    Method method = class_getInstanceMethod(test_class, @selector(debugRetain));
    ASSERT_TRUE(method);
    ASSERT_TRUE(class_addMethod(test_class, sel_registerName("retain"),
                                method_getImplementation(method),
                                method_getTypeEncoding(method)));

    method = class_getInstanceMethod(test_class, @selector(debugRelease));
    ASSERT_TRUE(method);
    ASSERT_TRUE(class_addMethod(test_class, sel_registerName("release"),
                                method_getImplementation(method),
                                method_getTypeEncoding(method)));
  }
}

- (void)dealloc {
  // Insert debugging code here.
}

- (instancetype)debugRetain {
  // Insert debugging code here.

  struct objc_super mySuper = {.receiver = self,
                               .super_class = [self superclass]};
  using retainSendSuper = id (*)(struct objc_super*, SEL);
  retainSendSuper sendSuper =
      reinterpret_cast<retainSendSuper>(objc_msgSendSuper);
  return sendSuper(&mySuper, _cmd);
}

- (oneway void)debugRelease {
  // Insert debugging code here.

  struct objc_super mySuper = {.receiver = self,
                               .super_class = [self superclass]};
  using releaseSendSuper = void (*)(struct objc_super*, SEL);
  releaseSendSuper sendSuper =
      reinterpret_cast<releaseSendSuper>(objc_msgSendSuper);
  return sendSuper(&mySuper, _cmd);
}

#endif

- (BOOL)isKeyWindow {
  return _pretendIsKeyWindow;
}

- (BOOL)isOnActiveSpace {
  return _pretendIsOnActiveSpace;
}

- (void)makePretendKeyWindowAndSetFirstResponder:(NSResponder*)responder {
  EXPECT_TRUE([self makeFirstResponder:responder]);
  self.pretendIsKeyWindow = YES;
}

- (void)clearPretendKeyWindowAndFirstResponder {
  self.pretendIsKeyWindow = NO;
  EXPECT_TRUE([self makeFirstResponder:NSApp]);
}

- (void)setPretendIsOnActiveSpace:(BOOL)pretendIsOnActiveSpace {
  _pretendIsOnActiveSpace = pretendIsOnActiveSpace;
  [NSWorkspace.sharedWorkspace.notificationCenter
      postNotificationName:NSWorkspaceActiveSpaceDidChangeNotification
                    object:NSWorkspace.sharedWorkspace];
}

- (void)setPretendFullKeyboardAccessIsEnabled:(BOOL)enabled {
  EXPECT_TRUE([NSWindow
      instancesRespondToSelector:@selector(_allowsAnyValidResponder)]);
  _pretendFullKeyboardAccessIsEnabled = enabled;
  [self recalculateKeyViewLoop];
}

// Override of an undocumented AppKit method which controls call to check if
// full keyboard access is enabled. Its presence is verified in
// -setPretendFullKeyboardAccessIsEnabled:.
- (BOOL)_allowsAnyValidResponder {
  return _pretendFullKeyboardAccessIsEnabled;
}

- (NSArray<NSView*>*)validKeyViews {
  NSMutableArray<NSView*>* validKeyViews = [NSMutableArray array];
  NSView* contentView = self.contentView;
  if (contentView.canBecomeKeyView) {
    [validKeyViews addObject:contentView];
  }
  for (NSView* keyView = contentView.nextValidKeyView;
       keyView != nil && ![validKeyViews containsObject:keyView];
       keyView = keyView.nextValidKeyView) {
    [validKeyViews addObject:keyView];
  }
  return validKeyViews;
}

- (NSRect)constrainFrameRect:(NSRect)frameRect toScreen:(NSScreen*)screen {
  if (!_useDefaultConstraints) {
    return frameRect;
  }

  return [super constrainFrameRect:frameRect toScreen:screen];
}

@end

namespace ui {

CocoaTestHelper::CocoaTestHelper() {
  // If a test suite hasn't already initialized NSApp, register the mock one
  // now.
  if (!NSApp) {
    mock_cr_app::RegisterMockCrApp();
  }

  // Set the duration of AppKit-evaluated animations (such as frame changes)
  // to zero for testing purposes. That way they take effect immediately.
  NSAnimationContext.currentContext.duration = 0.0;

  // The above does not affect window-resize time, such as for an
  // attached sheet dropping in.  Set that duration for the current
  // process (this is not persisted).  Empirically, the value of 0.0
  // is ignored.
  NSDictionary* dict = @{@"NSWindowResizeTime" : @"0.01"};
  [NSUserDefaults.standardUserDefaults registerDefaults:dict];

  MarkCurrentWindowsAsInitial();
}

CocoaTestHelper::~CocoaTestHelper() {
  // Call close on the test_window to clean it up if one was opened.
  [test_window_ clearPretendKeyWindowAndFirstResponder];
  [test_window_ close];
  test_window_ = nil;

  // Recycle the pool to clean up any stuff that was put on the
  // autorelease pool due to window or window controller closures.
  pool_.Recycle();

  // Some controls (NSTextFields, NSComboboxes etc) use
  // performSelector:withDelay: to clean up drag handlers and other
  // things (Radar 5851458 "Closing a window with a NSTextView in it
  // should get rid of it immediately").  The event loop must be spun
  // to get everything cleaned up correctly.  It normally only takes
  // one to two spins through the event loop to see a change.

  // NOTE(shess): Under valgrind, -nextEventMatchingMask:* in one test
  // needed to run twice, once taking .2 seconds, the next time .6
  // seconds.  The loop exit condition attempts to be scalable.

  // Get the set of windows which weren't present when the test
  // started.
  WeakWindowVector windows_left = WindowsLeft();

  while (!windows_left.empty()) {
    // Cover delayed actions by spinning the loop at least once after
    // this timeout.
    const NSTimeInterval kCloseTimeoutSeconds =
        TestTimeouts::action_timeout().InSecondsF();

    // Cover chains of delayed actions by spinning the loop at least
    // this many times.
    const int kCloseSpins = 3;

    // Track the set of remaining windows so that everything can be
    // reset if progress is made.
    WeakWindowVector still_left = windows_left;

    NSDate* start_date = [NSDate date];
    bool one_more_time = true;
    int spins = 0;
    while (still_left.size() == windows_left.size() &&
           (spins < kCloseSpins || one_more_time)) {
      // Check the timeout before pumping events, so that we'll spin
      // the loop once after the timeout.
      one_more_time = start_date.timeIntervalSinceNow > -kCloseTimeoutSeconds;

      // Autorelease anything thrown up by the event loop.
      @autoreleasepool {
        ++spins;
        NSEvent* next_event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                 untilDate:nil
                                                    inMode:NSDefaultRunLoopMode
                                                   dequeue:YES];
        [NSApp sendEvent:next_event];
        [NSApp updateWindows];
      }

      // Refresh the outstanding windows.
      still_left = WindowsLeft();
    }

    // If no progress is being made, log a failure and continue.
    if (still_left.size() == windows_left.size()) {
      // NOTE(shess): Failing this expectation means that the test
      // opened windows which have not been fully released.  Either
      // there is a leak, or perhaps one of |kCloseTimeoutSeconds| or
      // |kCloseSpins| needs adjustment.
      EXPECT_EQ(0U, windows_left.size());
      for (NSWindow* __weak window : windows_left) {
        LOG(WARNING) << "Didn't close window "
                     << base::SysNSStringToUTF8(window.description);
      }
      break;
    }

    windows_left = still_left;
  }
}

void CocoaTestHelper::MarkCurrentWindowsAsInitial() {
  // Collect the list of windows that were open when the test started so
  // that we don't wait for them to close in TearDown. Has to be done
  // after BootstrapCocoa is called.
  initial_windows_ = ApplicationWindows();
}

CocoaTestHelperWindow* CocoaTestHelper::test_window() {
  if (!test_window_) {
    test_window_ = [[CocoaTestHelperWindow alloc] init];
    if (base::debug::BeingDebugged()) {
      [test_window_ orderFront:nil];
    } else {
      [test_window_ orderBack:nil];
    }
  }
  return test_window_;
}

// Returns a vector of currently open windows.
CocoaTestHelper::WeakWindowVector CocoaTestHelper::ApplicationWindows() {
  WeakWindowVector windows;

  // Must create a pool here because [NSApp windows] has created an array which
  // retains all the windows in it.
  @autoreleasepool {
    for (NSWindow* window in NSApp.windows) {
      windows.push_back(window);
    }
    return windows;
  }
}

CocoaTestHelper::WeakWindowVector CocoaTestHelper::WindowsLeft() {
  // Window pointers can go nil only when the run loop is going, so it's safe to
  // use sets within this function, just not outside it.
  using WeakWindowSet = std::set<NSWindow * __weak>;

  WeakWindowVector windows = ApplicationWindows();
  WeakWindowSet windows_set(windows.begin(), windows.end());

  // Ignore TextInputUIMacHelper.framework created TUINSWindow. We have no
  // control or documentation about these windows, ignoring them seems like the
  // best approach.
  std::erase_if(windows_set, [](NSWindow* __weak set_window) {
    return [set_window isKindOfClass:NSClassFromString(@"TUINSWindow")];
  });

  // Subtract away the initial windows. The current window set will not have any
  // nil values, as it was just obtained, so subtracting away the nil from any
  // initial windows that have been closed is safe.
  WeakWindowSet initial_windows_set(initial_windows_.begin(),
                                    initial_windows_.end());

  WeakWindowSet windows_left_set =
      base::STLSetDifference<WeakWindowSet>(windows_set, initial_windows_set);
  return std::vector(windows_left_set.begin(), windows_left_set.end());
}

CocoaTest::CocoaTest() : helper_(std::make_unique<CocoaTestHelper>()) {}
CocoaTest::~CocoaTest() {
  CHECK(!helper_);
}

void CocoaTest::TearDown() {
  helper_.reset();
  PlatformTest::TearDown();
}

void CocoaTest::MarkCurrentWindowsAsInitial() {
  helper_->MarkCurrentWindowsAsInitial();
}

}  // namespace ui
