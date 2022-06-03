// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/test/cocoa_helper.h"

#include "base/debug/debugger.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/test/mock_chrome_application_mac.h"
#include "base/test/test_timeouts.h"

namespace {

// Some AppKit function leak intentionally, e.g. for caching purposes.
// Force those leaks here, so there can be a unique calling path, allowing
// to flag intentional leaks without having to suppress all calls to
// potentially leaky functions.
void NOINLINE ForceSystemLeaks() {
  // If a test suite hasn't already initialized NSApp, register the mock one
  // now.
  if (!NSApp)
    mock_cr_app::RegisterMockCrApp();

  // First NSCursor push always leaks.
  [[NSCursor openHandCursor] push];
  [NSCursor pop];
}

}  // namespace.

@implementation CocoaTestHelperWindow

@synthesize pretendIsKeyWindow = _pretendIsKeyWindow;
@synthesize pretendIsOccluded = _pretendIsOccluded;
@synthesize pretendIsOnActiveSpace = _pretendIsOnActiveSpace;
@synthesize pretendFullKeyboardAccessIsEnabled =
    _pretendFullKeyboardAccessIsEnabled;
@synthesize useDefaultConstraints = _useDefaultConstraints;

- (instancetype)initWithContentRect:(NSRect)contentRect {
  self = [super initWithContentRect:contentRect
                          styleMask:NSBorderlessWindowMask
                            backing:NSBackingStoreBuffered
                              defer:NO];
  if (self) {
    _useDefaultConstraints = YES;
    _pretendIsOnActiveSpace = YES;
  }
  return self;
}

- (instancetype)init {
  return [self initWithContentRect:NSMakeRect(0, 0, 800, 600)];
}

- (void)dealloc {
  // Just a good place to put breakpoints when having problems with
  // unittests and CocoaTestHelperWindow.
  [super dealloc];
}

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

- (void)setPretendIsOccluded:(BOOL)flag {
  _pretendIsOccluded = flag;
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidChangeOcclusionStateNotification
                    object:self];
}

- (void)setPretendIsOnActiveSpace:(BOOL)pretendIsOnActiveSpace {
  _pretendIsOnActiveSpace = pretendIsOnActiveSpace;
  [[NSWorkspace sharedWorkspace].notificationCenter
      postNotificationName:NSWorkspaceActiveSpaceDidChangeNotification
                    object:[NSWorkspace sharedWorkspace]];
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

- (NSWindowOcclusionState)occlusionState {
  return _pretendIsOccluded ? 0 : NSWindowOcclusionStateVisible;
}

- (NSArray<NSView*>*)validKeyViews {
  NSMutableArray<NSView*>* validKeyViews = [NSMutableArray array];
  NSView* contentView = self.contentView;
  if (contentView.canBecomeKeyView)
    [validKeyViews addObject:contentView];
  for (NSView* keyView = contentView.nextValidKeyView;
       keyView != nil && ![validKeyViews containsObject:keyView];
       keyView = keyView.nextValidKeyView)
    [validKeyViews addObject:keyView];
  return validKeyViews;
}

- (NSRect)constrainFrameRect:(NSRect)frameRect toScreen:(NSScreen*)screen {
  if (!_useDefaultConstraints)
    return frameRect;

  return [super constrainFrameRect:frameRect toScreen:screen];
}

@end

namespace ui {

CocoaTestHelper::CocoaTestHelper() {
  ForceSystemLeaks();
  // Set the duration of AppKit-evaluated animations (such as frame changes)
  // to zero for testing purposes. That way they take effect immediately.
  [[NSAnimationContext currentContext] setDuration:0.0];

  // The above does not affect window-resize time, such as for an
  // attached sheet dropping in.  Set that duration for the current
  // process (this is not persisted).  Empirically, the value of 0.0
  // is ignored.
  NSDictionary* dict = @{@"NSWindowResizeTime" : @"0.01"};
  [[NSUserDefaults standardUserDefaults] registerDefaults:dict];

  MarkCurrentWindowsAsInitial();
}

CocoaTestHelper::~CocoaTestHelper() {
  // Call close on our test_window to clean it up if one was opened.
  [test_window_ clearPretendKeyWindowAndFirstResponder];
  [test_window_ close];
  test_window_ = nil;

  // Recycle the pool to clean up any stuff that was put on the
  // autorelease pool due to window or windowcontroller closures.
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
  std::set<NSWindow*> windows_left(WindowsLeft());

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
    std::set<NSWindow*> still_left = windows_left;

    NSDate* start_date = [NSDate date];
    bool one_more_time = true;
    int spins = 0;
    while (still_left.size() == windows_left.size() &&
           (spins < kCloseSpins || one_more_time)) {
      // Check the timeout before pumping events, so that we'll spin
      // the loop once after the timeout.
      one_more_time =
          ([start_date timeIntervalSinceNow] > -kCloseTimeoutSeconds);

      // Autorelease anything thrown up by the event loop.
      @autoreleasepool {
        ++spins;
        NSEvent* next_event = [NSApp nextEventMatchingMask:NSAnyEventMask
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
      for (std::set<NSWindow*>::iterator iter = windows_left.begin();
           iter != windows_left.end(); ++iter) {
        LOG(WARNING) << "Didn't close window "
                     << base::SysNSStringToUTF8([*iter description]);
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

std::set<NSWindow*> CocoaTestHelper::ApplicationWindows() {
  // This must NOT retain the windows it is returning.
  std::set<NSWindow*> windows;

  // Must create a pool here because [NSApp windows] has created an array
  // with retains on all the windows in it.
  @autoreleasepool {
    NSArray* appWindows = [NSApp windows];
    for (NSWindow* window in appWindows) {
      windows.insert(window);
    }
    return windows;
  }
}

std::set<NSWindow*> CocoaTestHelper::WindowsLeft() {
  const std::set<NSWindow*> windows(ApplicationWindows());
  std::set<NSWindow*> windows_left =
      base::STLSetDifference<std::set<NSWindow*>>(windows, initial_windows_);
  return windows_left;
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
