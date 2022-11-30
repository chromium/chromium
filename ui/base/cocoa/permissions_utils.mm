// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/permissions_utils.h"

#include <CoreGraphics/CoreGraphics.h>
#include <Foundation/Foundation.h>

#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/task/thread_pool.h"

namespace ui {

// Note that the SDK has `CGPreflightScreenCaptureAccess()` and
// `CGRequestScreenCaptureAccess()` listed as available on 10.15, but using
// them yields link errors in testing. Therefore, use them on 11.0 and
// heuristic methods on 10.15.

bool IsScreenCaptureAllowed() {
  if (@available(macOS 11.0, *)) {
    return CGPreflightScreenCaptureAccess();
  } else if (@available(macOS 10.15, *)) {
    // Screen Capture is considered allowed if the name of at least one normal
    // or dock window running on another process is visible.
    // See https://crbug.com/993692.
    base::ScopedCFTypeRef<CFArrayRef> window_list(
        CGWindowListCopyWindowInfo(kCGWindowListOptionAll, kCGNullWindowID));
    int current_pid = [[NSProcessInfo processInfo] processIdentifier];
    for (NSDictionary* window in base::mac::CFToNSCast(window_list.get())) {
      NSNumber* window_pid =
          [window objectForKey:base::mac::CFToNSCast(kCGWindowOwnerPID)];
      if (!window_pid || [window_pid integerValue] == current_pid)
        continue;

      NSString* window_name =
          [window objectForKey:base::mac::CFToNSCast(kCGWindowName)];
      if (!window_name)
        continue;

      NSNumber* layer =
          [window objectForKey:base::mac::CFToNSCast(kCGWindowLayer)];
      if (!layer)
        continue;

      NSInteger layer_integer = [layer integerValue];
      if (layer_integer == CGWindowLevelForKey(kCGNormalWindowLevelKey) ||
          layer_integer == CGWindowLevelForKey(kCGDockWindowLevelKey)) {
        return true;
      }
    }
    return false;
  } else {
    // Screen capture is always allowed in older macOS versions.
    return true;
  }
}

bool TryPromptUserForScreenCapture() {
  if (@available(macOS 11.0, *)) {
    return CGRequestScreenCaptureAccess();
  } else if (@available(macOS 10.15, *)) {
    // On 10.15+, macOS will show the permissions prompt for Screen Recording
    // if we request to create a display stream and our application is not
    // in the applications list in System permissions. Stream creation will
    // fail if the user denies permission, or if our application is already
    // in the system permssion and is unchecked.
    base::ScopedCFTypeRef<CGDisplayStreamRef> stream(CGDisplayStreamCreate(
        CGMainDisplayID(), 1, 1, 'BGRA', nullptr,
        ^(CGDisplayStreamFrameStatus status, uint64_t displayTime,
          IOSurfaceRef frameSurface, CGDisplayStreamUpdateRef updateRef){
        }));
    return stream != nullptr;
  } else {
    // Screen capture is always allowed in older macOS versions.
    return true;
  }
}

void WarmScreenCapture() {
  if (@available(macOS 10.15, *)) {
    // WarmScreenCapture() is meant to be called during early startup. Since the
    // calls to warm the cache may block, execute them off the main thread so we
    // don't hold up startup. To be effective these calls need to run before
    // Chrome is updated. Running them off the main thread technically opens us
    // to a race condition, however updating happens way later so this is not a
    // concern.
    base::ThreadPool::PostTask(
        FROM_HERE,
        // Checking screen capture access hits the TCC.db and reads Chrome's
        // code signature from disk, marking as MayBlock.
        {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce([] {
          if (IsScreenCaptureAllowed()) {
            base::ScopedCFTypeRef<CGImageRef>(CGWindowListCreateImage(
                CGRectInfinite, kCGWindowListOptionOnScreenOnly,
                kCGNullWindowID, kCGWindowImageDefault));
          }
        }));
  }
}

}  // namespace ui
