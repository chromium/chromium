// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/permissions_utils.h"

#include <CoreGraphics/CoreGraphics.h>
#include <Foundation/Foundation.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/task/thread_pool.h"

namespace ui {
namespace {

BASE_FEATURE(kWarmScreenCaptureSonoma,
             "WarmScreenCaptureSonoma",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWarmScreenCaptureSequoia,
             "WarmScreenCaptureSequoia",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool ShouldWarmScreenCapture() {
  const int macos_version = base::mac::MacOSVersion();
  // On macOS < 14, we're using CGWindowListCreateImage to capture a screenshot.
  // Starting in macOS 14, CGWindowListCreateImage causes a "your screen is
  // being captured" chip to show in the menu bar while an app is capturing the
  // screen, and if it's a one-time image capture, it shows for ten seconds.
  if (macos_version < 14'00'00) {
    return true;
  }

  // Kill switch, Sonoma.
  if (macos_version < 15'00'00 &&
      !base::FeatureList::IsEnabled(kWarmScreenCaptureSonoma)) {
    return false;
  }

  // Feature disabled by default for Sequoia unless explicitly enabled.
  if (macos_version >= 15'00'00 &&
      !base::FeatureList::IsEnabled(kWarmScreenCaptureSequoia)) {
    return false;
  }

  // On macOS >= 14, Apple introduced SCScreenshotManager that can be used to
  // capture a screenshot without any notification shown to the user. There's a
  // bug in this API that was fixed in 14.4.
  if (macos_version >= 14'04'00) {
    return true;
  }

  // macOS 14-14.3.
  return false;
}

// Capture a screenshot and throw away the result.
void CaptureScreenshot() {
  if (@available(macOS 14.0, *)) {
    CHECK(base::FeatureList::IsEnabled(kWarmScreenCaptureSonoma));
    // Capturing a screenshot using SCK involves three asynchronous steps:
    //   1. Request shareable contents (getShareableContent...),
    //   2. Instruct SCScreenshotManager to take a screenshot (captureImage...),
    //   3. Receive a callback with the actual image.
    auto shareable_content_handler = ^(SCShareableContent* content,
                                       NSError* error) {
      if (!content.displays.count || error) {
        LOG(WARNING)
            << "Failed to get shareable content during WarmScreenCapture.";
        return;
      }

      SCDisplay* first_display = content.displays.firstObject;
      SCStreamConfiguration* config = [[SCStreamConfiguration alloc] init];
      // Set the size to something small to make it clear to anyone reading the
      // code that the screenshot contains no real information.
      config.width = 16;
      config.height = 10;
      auto screenshot_handler = ^(CGImageRef sampleBuffer, NSError* sc_error) {
        // Do nothing.
      };

      SCContentFilter* filter =
          [[SCContentFilter alloc] initWithDisplay:first_display
                                  excludingWindows:@[]];
      [SCScreenshotManager captureImageWithFilter:filter
                                    configuration:config
                                completionHandler:screenshot_handler];
    };
    [SCShareableContent
        getShareableContentExcludingDesktopWindows:true
                               onScreenWindowsOnly:true
                                 completionHandler:shareable_content_handler];
  } else {
    base::apple::ScopedCFTypeRef<CGImageRef>(
        CGWindowListCreateImage(CGRectInfinite, kCGWindowListOptionOnScreenOnly,
                                kCGNullWindowID, kCGWindowImageDefault));
  }
}

}  // namespace

bool IsScreenCaptureAllowed() {
    return CGPreflightScreenCaptureAccess();
}

bool TryPromptUserForScreenCapture() {
    return CGRequestScreenCaptureAccess();
}

void WarmScreenCapture() {
  if (!ShouldWarmScreenCapture()) {
    return;
  }

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
          CaptureScreenshot();
        }
      }));
}

}  // namespace ui
