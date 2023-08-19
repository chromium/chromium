// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mac/scoped_cocoa_disable_screen_updates.h"

#import <Cocoa/Cocoa.h>

namespace gfx {

ScopedCocoaDisableScreenUpdates::ScopedCocoaDisableScreenUpdates() {
  [NSAnimationContext beginGrouping];
}

ScopedCocoaDisableScreenUpdates::~ScopedCocoaDisableScreenUpdates() {
  [NSAnimationContext endGrouping];
}

}  // namespace gfx
