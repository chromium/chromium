// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

#import <AppKit/AppKit.h>

#include "base/check_op.h"

namespace gfx {

struct ScopedNSGraphicsContextSaveGState::ObjCStorage {
  NSGraphicsContext* __weak context;
};

ScopedNSGraphicsContextSaveGState::ScopedNSGraphicsContextSaveGState()
    : objc_storage_(std::make_unique<ObjCStorage>()) {
  objc_storage_->context = NSGraphicsContext.currentContext;
  [NSGraphicsContext saveGraphicsState];
}

ScopedNSGraphicsContextSaveGState::~ScopedNSGraphicsContextSaveGState() {
  [NSGraphicsContext restoreGraphicsState];
  DCHECK_EQ(objc_storage_->context, NSGraphicsContext.currentContext);
}

}  // namespace gfx
