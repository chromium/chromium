// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/animation.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/mac_util.h"
#include "base/message_loop/message_loop_current.h"

// Only available since 10.12.
@interface NSWorkspace (AvailableSinceSierra)
@property(readonly) BOOL accessibilityDisplayShouldReduceMotion;
@end

namespace gfx {

// static
bool Animation::ShouldRenderRichAnimationImpl() {
  return !PrefersReducedMotion();
}

// static
bool Animation::ScrollAnimationsEnabledBySystem() {
  // Because of sandboxing, OS settings should only be queried from the browser
  // process.
  DCHECK(base::MessageLoopCurrentForUI::IsSet() ||
         base::MessageLoopCurrentForIO::IsSet());

  bool enabled = false;
  id value = nil;
  value = [[NSUserDefaults standardUserDefaults]
      objectForKey:@"NSScrollAnimationEnabled"];
  if (value)
    enabled = [value boolValue];
  return enabled;
}

// static
void Animation::UpdatePrefersReducedMotion() {
  // prefers_reduced_motion_ should only be modified on the UI thread.
  // TODO(crbug.com/927163): DCHECK this assertion once tests are well-behaved.

  // We default to assuming that animations are enabled, to avoid impacting the
  // experience for users on pre-10.12 systems.
  prefers_reduced_motion_ = false;
  SEL sel = @selector(accessibilityDisplayShouldReduceMotion);
  if ([[NSWorkspace sharedWorkspace] respondsToSelector:sel]) {
    prefers_reduced_motion_ =
        [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldReduceMotion];
  }
}

} // namespace gfx
