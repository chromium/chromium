// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/test/cocoa_helper.h"

#include <set>
#include <vector>

#include "base/debug/debugger.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/mock_chrome_application_mac.h"
#include "base/test/test_timeouts.h"
#include "ui/display/screen.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

- (void)dealloc {
  // Just a good place to put breakpoints when having problems with
  // unittests and CocoaTestHelperWindow.
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

namespace {

// A vector to hold a list of weak NSWindow pointers. Used as the container for
// lists of weak pointers, because putting pointers that can change their values
// to nil inside a set would break the hash.
using WeakWindowVector = std::vector<NSWindow * __weak>;

// Returns a vector of currently open windows.
WeakWindowVector ApplicationWindows() {
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

// Returns a vector of windows which are in `ApplicationWindows()` but not
// `initial_windows`.
WeakWindowVector WindowsLeft(WeakWindowVector initial_windows) {
  // Window pointers can go nil only when the run loop is going, so it's safe to
  // use sets within this function, just not outside it.
  using WeakWindowSet = std::set<NSWindow * __weak>;

  WeakWindowVector windows = ApplicationWindows();
  WeakWindowSet windows_set(windows.begin(), windows.end());

  // Subtract away the initial windows. The current window set will not have any
  // nil values, as it was just obtained, so subtracting away the nil from any
  // initial windows that have been closed is safe.
  WeakWindowSet initial_windows_set(initial_windows.begin(),
                                    initial_windows.end());

  WeakWindowSet windows_left_set =
      base::STLSetDifference<WeakWindowSet>(windows_set, initial_windows_set);
  return std::vector(windows_left_set.begin(), windows_left_set.end());
}

}  // namespace.

struct CocoaTestHelper::ObjCStorage {
  display::ScopedNativeScreen screen;

  base::mac::ScopedNSAutoreleasePool pool;

  // Windows which existed at the beginning of the test.
  WeakWindowVector initial_windows;

  CocoaTestHelperWindow* __strong test_window;
};

CocoaTestHelper::CocoaTestHelper()
    : objc_storage_(std::make_unique<ObjCStorage>()) {
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
  [objc_storage_->test_window clearPretendKeyWindowAndFirstResponder];
  [objc_storage_->test_window close];
  objc_storage_->test_window = nil;

  // Recycle the pool to clean up any stuff that was put on the
  // autorelease pool due to window or window controller closures.
  objc_storage_->pool.Recycle();

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
  WeakWindowVector windows_left = WindowsLeft(objc_storage_->initial_windows);

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
      still_left = WindowsLeft(objc_storage_->initial_windows);
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
  objc_storage_->initial_windows = ApplicationWindows();
}

CocoaTestHelperWindow* CocoaTestHelper::test_window() {
  if (!objc_storage_->test_window) {
    objc_storage_->test_window = [[CocoaTestHelperWindow alloc] init];
    if (base::debug::BeingDebugged()) {
      [objc_storage_->test_window orderFront:nil];
    } else {
      [objc_storage_->test_window orderBack:nil];
    }
  }
  return objc_storage_->test_window;
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
