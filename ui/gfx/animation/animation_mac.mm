// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/animation.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/mac_util.h"
#include "base/task/current_thread.h"

namespace gfx {

// static
bool Animation::ShouldRenderRichAnimationImpl() {
  return !PrefersReducedMotion();
}

// static
bool Animation::ScrollAnimationsEnabledBySystem() {
  // Because of sandboxing, OS settings should only be queried from the browser
  // process.
  DCHECK(base::CurrentUIThread::IsSet() || base::CurrentIOThread::IsSet());

  bool enabled = false;
  id value = nil;
  value = [NSUserDefaults.standardUserDefaults
      objectForKey:@"NSScrollAnimationEnabled"];
  if (value)
    enabled = [value boolValue];
  return enabled;
}

// static
void Animation::UpdatePrefersReducedMotion() {
  // prefers_reduced_motion_ should only be modified on the UI thread.
  // TODO(crbug.com/40611878): DCHECK this assertion once tests are
  // well-behaved.

  prefers_reduced_motion_ =
      NSWorkspace.sharedWorkspace.accessibilityDisplayShouldReduceMotion;
}

} // namespace gfx
