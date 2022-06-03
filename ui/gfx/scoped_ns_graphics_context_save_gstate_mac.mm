// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

#import <AppKit/AppKit.h>

#include "base/check_op.h"

namespace gfx {

ScopedNSGraphicsContextSaveGState::ScopedNSGraphicsContextSaveGState()
    : context_([NSGraphicsContext currentContext]) {
  [NSGraphicsContext saveGraphicsState];
}

ScopedNSGraphicsContextSaveGState::~ScopedNSGraphicsContextSaveGState() {
  [NSGraphicsContext restoreGraphicsState];
  DCHECK_EQ(context_, [NSGraphicsContext currentContext]);
}

}  // namespace gfx
