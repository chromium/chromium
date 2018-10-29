// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_APPKIT_UTILS_H
#define UI_BASE_COCOA_APPKIT_UTILS_H

#import <Cocoa/Cocoa.h>

#include "ui/base/ui_base_export.h"

namespace ui {

// Whether a force-click event on the touchpad should invoke Quick Look.
UI_BASE_EXPORT bool ForceClickInvokesQuickLook();

// Returns true if both CGFloat values are equal.
UI_BASE_EXPORT bool IsCGFloatEqual(CGFloat a, CGFloat b);

}  // namespace ui

#endif  // UI_BASE_COCOA_APPKIT_UTILS_H
