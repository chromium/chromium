// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_FLIPPED_VIEW_H_
#define UI_BASE_COCOA_FLIPPED_VIEW_H_

#import <Cocoa/Cocoa.h>

#include "base/component_export.h"

// A view where the Y axis is flipped such that the origin is at the top left
// and Y value increases downwards. Drawing is flipped so that layout of the
// sections is easier. Apple recommends flipping the coordinate origin when
// doing a lot of text layout because it's more natural.
COMPONENT_EXPORT(UI_BASE)
@interface FlippedView : NSView
@end

#endif  // UI_BASE_COCOA_FLIPPED_VIEW_H_
