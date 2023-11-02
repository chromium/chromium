// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_APPKIT_UTILS_H_
#define UI_BASE_COCOA_APPKIT_UTILS_H_

#import <Cocoa/Cocoa.h>

#include "base/component_export.h"

namespace ui {

// Whether a force-click event on the touchpad should invoke Quick Look.
COMPONENT_EXPORT(UI_BASE) bool ForceClickInvokesQuickLook();

// Returns true if both CGFloat values are equal.
COMPONENT_EXPORT(UI_BASE) bool IsCGFloatEqual(CGFloat a, CGFloat b);

}  // namespace ui

#endif  // UI_BASE_COCOA_APPKIT_UTILS_H_
