// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/snapshot/snapshot.h"

#import <Cocoa/Cocoa.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot_mac.h"

// The API that allows an app TCC-less access to its own windows is new in macOS
// 14.4. While this has been tested extensively on 14.4 betas, because this is a
// new API added in an OS dot release, have a "break in case of emergency" off-
// switch.
BASE_FEATURE(kUseScreenCaptureKitForSnapshots,
             "UseScreenCaptureKitForSnapshots",
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace ui {

namespace {

SnapshotAPI g_snapshot_api = SnapshotAPI::kUnspecified;

void GrabViewSnapshotScreenCaptureKitImpl(gfx::NativeView native_view,
                                          const gfx::Rect& source_rect,
                                          GrabSnapshotImageCallback callback)
    API_AVAILABLE(macos(14.4)) {
  NSView* view = native_view.GetNativeNSView();
  NSInteger window_number = view.window.windowNumber;
  __block GrabSnapshotImageCallback local_callback = std::move(callback);

  // Get the view frame relative to the window, and flip it to have an
  // upper-left origin. (ScreenCaptureKit works with upper-left origins, as does
  // Views.)
  NSRect view_frame = [view convertRect:view.bounds toView:nil];
  view_frame.origin.y = view.window.frame.size.height - view_frame.origin.y -
                        view_frame.size.height;

  // Offset the `source_rect` to be relative to the view bounds's upper-left
  // origin.
  NSRect clip_rect = source_rect.ToCGRect();
  clip_rect = NSOffsetRect(clip_rect, view_frame.origin.x, view_frame.origin.y);

  [SCShareableContent getCurrentProcessShareableContentWithCompletionHandler:^(
                          SCShareableContent* shareable_content,
                          NSError* error) {
    dispatch_async(dispatch_get_main_queue(), ^{
      if (error) {
        DLOG(ERROR) << base::SysNSStringToUTF8(error.description);
        std::move(local_callback).Run(gfx::Image());
        return;
      }

      // Find the SCWindow corresponding to the view being snapshotted.
      NSArray<SCWindow*>* sc_windows = shareable_content.windows;
      NSUInteger sc_window_index =
          [sc_windows indexOfObjectPassingTest:^BOOL(
                          SCWindow* obj, NSUInteger idx, BOOL* stop) {
            return obj.windowID == window_number;
          }];
      if (sc_window_index == NSNotFound) {
        DLOG(ERROR) << "failed to find window";
        std::move(local_callback).Run(gfx::Image());
        return;
      }
      SCWindow* sc_window = sc_windows[sc_window_index];

      // Build the filter and config for the capture.
      SCContentFilter* filter =
          [[SCContentFilter alloc] initWithDesktopIndependentWindow:sc_window];
      SCStreamConfiguration* config = [[SCStreamConfiguration alloc] init];
      NSSize image_size = clip_rect.size;
      config.width = image_size.width * filter.pointPixelScale;
      config.height = image_size.height * filter.pointPixelScale;
      config.sourceRect = clip_rect;  // In DIPs.
      config.showsCursor = NO;
      config.ignoreShadowsSingleWindow = YES;
      config.captureResolution = SCCaptureResolutionBest;
      config.ignoreGlobalClipSingleWindow = YES;
      config.includeChildWindows = NO;

      [SCScreenshotManager
          captureImageWithFilter:filter
                   configuration:config
               completionHandler:^(CGImageRef sample_buffer, NSError* error2) {
                 // The block below will retain an Objective-C object but not a
                 // CF type, so convert the CGImage to an NSImage before
                 // enqueuing the block.
                 NSImage* image;
                 if (sample_buffer) {
                   // Do not correctly size here. Downstream callers are
                   // assuming that the image returned is scaled by the device
                   // pixel ratio.
                   image = [[NSImage alloc] initWithCGImage:sample_buffer
                                                       size:NSZeroSize];
                 }
                 dispatch_async(dispatch_get_main_queue(), ^{
                   if (error2) {
                     DLOG(ERROR) << base::SysNSStringToUTF8(error2.description);
                     std::move(local_callback).Run(gfx::Image());
                     return;
                   }
                   std::move(local_callback).Run(gfx::Image(image));
                 });
               }];
    });
  }];
}

gfx::Image GrabViewSnapshotCGWindowListImpl(gfx::NativeView native_view,
                                            const gfx::Rect& snapshot_bounds) {
  NSView* view = native_view.GetNativeNSView();
  NSWindow* window = view.window;
  NSScreen* screen = NSScreen.screens.firstObject;
  gfx::Rect screen_bounds = gfx::Rect(NSRectToCGRect(screen.frame));

  // Get the view bounds relative to the screen.
  NSRect frame = [view convertRect:view.bounds toView:nil];
  frame = [window convertRectToScreen:frame];

  gfx::Rect view_bounds = gfx::Rect(NSRectToCGRect(frame));

  // Flip window coordinates based on the primary screen.
  view_bounds.set_y(screen_bounds.height() - view_bounds.y() -
                    view_bounds.height());

  // Convert snapshot bounds relative to window into bounds relative to
  // screen.
  gfx::Rect screen_snapshot_bounds = snapshot_bounds;
  screen_snapshot_bounds.Offset(view_bounds.OffsetFromOrigin());

  DCHECK_LE(screen_snapshot_bounds.right(), view_bounds.right());
  DCHECK_LE(screen_snapshot_bounds.bottom(), view_bounds.bottom());

  base::apple::ScopedCFTypeRef<CGImageRef> window_snapshot(
      CGWindowListCreateImage(
          screen_snapshot_bounds.ToCGRect(), kCGWindowListOptionIncludingWindow,
          window.windowNumber, kCGWindowImageBoundsIgnoreFraming));
  if (!window_snapshot || CGImageGetWidth(window_snapshot.get()) <= 0) {
    return gfx::Image();
  }

  return gfx::Image([[NSImage alloc] initWithCGImage:window_snapshot.get()
                                                size:NSZeroSize]);
}

bool ShouldForceOldAPIUse() {
  // The SCK API +[SCShareableContent
  // getCurrentProcessShareableContentWithCompletionHandler:] was introduced in
  // macOS 14.4, but it did not work correctly when there were multiple
  // instances of an app with the same bundle ID.
  //
  // This is fixed in macOS 15.
  //
  // https://crbug.com/333443445, FB13717818
  if (base::mac::MacOSVersion() >= 15'00'00) {
    return false;
  }

  return [NSRunningApplication
             runningApplicationsWithBundleIdentifier:NSBundle.mainBundle
                                                         .bundleIdentifier]
             .count > 1;
}

}  // namespace

void ForceAPIUsageForTesting(SnapshotAPI api) {
  CHECK(base::mac::MacOSVersion() >= 14'04'00 || api != SnapshotAPI::kNewAPI);
  g_snapshot_api = api;
}

void GrabWindowSnapshot(gfx::NativeWindow native_window,
                        const gfx::Rect& source_rect,
                        GrabSnapshotImageCallback callback) {
  // Make sure to grab the "window frame" view so we get current tab +
  // tabstrip.
  NSView* view = native_window.GetNativeNSWindow().contentView.superview;

  GrabViewSnapshot(view, source_rect, std::move(callback));
}

void GrabViewSnapshot(gfx::NativeView view,
                      const gfx::Rect& source_rect,
                      GrabSnapshotImageCallback callback) {
  SnapshotAPI api = g_snapshot_api;
  if (api == SnapshotAPI::kUnspecified) {
    if (base::mac::MacOSVersion() >= 14'04'00 &&
        base::FeatureList::IsEnabled(kUseScreenCaptureKitForSnapshots) &&
        !ShouldForceOldAPIUse()) {
      api = SnapshotAPI::kNewAPI;
    } else {
      api = SnapshotAPI::kOldAPI;
    }
  }

  if (@available(macOS 14.4, *)) {
    if (api == SnapshotAPI::kNewAPI) {
      GrabViewSnapshotScreenCaptureKitImpl(view, source_rect,
                                           std::move(callback));
      return;
    }
  }

  gfx::Image image = GrabViewSnapshotCGWindowListImpl(view, source_rect);
  std::move(callback).Run(image);
}

}  // namespace ui
