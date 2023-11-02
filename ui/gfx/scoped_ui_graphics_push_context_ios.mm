// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/scoped_ui_graphics_push_context_ios.h"

#import <UIKit/UIKit.h>

#include "base/check_op.h"

namespace gfx {

ScopedUIGraphicsPushContext::ScopedUIGraphicsPushContext(CGContextRef context)
    : context_(context) {
  UIGraphicsPushContext(context_);
}

ScopedUIGraphicsPushContext::~ScopedUIGraphicsPushContext() {
  DCHECK_EQ(context_, UIGraphicsGetCurrentContext());
  UIGraphicsPopContext();
}

}  // namespace gfx
