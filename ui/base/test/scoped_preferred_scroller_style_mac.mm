// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/test/scoped_preferred_scroller_style_mac.h"

#import <AppKit/AppKit.h>

#include "base/logging.h"
#import "base/mac/scoped_objc_class_swizzler.h"

using base::mac::ScopedObjCClassSwizzler;

namespace {

// Swizzling can be stacked, but not interleaved without creating unexpected
// states. Require that there is only one swizzler rather than tracking a stack.
bool g_swizzling = false;

void NotifyStyleChanged() {
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSPreferredScrollerStyleDidChangeNotification
                    object:nil];
}

NSScrollerStyle GetScrollerStyle(bool overlay) {
  return overlay ? NSScrollerStyleOverlay : NSScrollerStyleLegacy;
}

}  // namespace

// Donates a testing implementation of +[NSScroller preferredScrollerStyle] by
// returning NSScrollerStyleLegacy.
@interface FakeNSScrollerPreferredStyleLegacyDonor : NSObject
@end

@implementation FakeNSScrollerPreferredStyleLegacyDonor

+ (NSInteger)preferredScrollerStyle {
  return NSScrollerStyleLegacy;
}

@end

// Donates a testing implementation of +[NSScroller preferredScrollerStyle] by
// returning NSScrollerStyleOverlay.
@interface FakeNSScrollerPreferredStyleOverlayDonor : NSObject
@end

@implementation FakeNSScrollerPreferredStyleOverlayDonor

+ (NSInteger)preferredScrollerStyle {
  return NSScrollerStyleOverlay;
}

@end

namespace ui {
namespace test {

ScopedPreferredScrollerStyle::ScopedPreferredScrollerStyle(bool overlay)
    : overlay_(overlay) {
  NSInteger previous_style = [NSScroller preferredScrollerStyle];
  Class style_class = overlay_
                          ? [FakeNSScrollerPreferredStyleOverlayDonor class]
                          : [FakeNSScrollerPreferredStyleLegacyDonor class];

  DCHECK(!g_swizzling);
  g_swizzling = true;
  swizzler_ = std::make_unique<ScopedObjCClassSwizzler>(
      [NSScroller class], style_class, @selector(preferredScrollerStyle));

  if (previous_style != GetScrollerStyle(overlay_))
    NotifyStyleChanged();
}

ScopedPreferredScrollerStyle::~ScopedPreferredScrollerStyle() {
  swizzler_.reset();
  DCHECK(g_swizzling);
  g_swizzling = false;

  if ([NSScroller preferredScrollerStyle] != GetScrollerStyle(overlay_))
    NotifyStyleChanged();
}

}  // namespace test
}  // namespace ui
