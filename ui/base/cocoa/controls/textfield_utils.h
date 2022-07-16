// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_CONTROLS_TEXTFIELD_UTILS_H_
#define UI_BASE_COCOA_CONTROLS_TEXTFIELD_UTILS_H_

#include "base/component_export.h"

#include <Cocoa/Cocoa.h>

COMPONENT_EXPORT(UI_BASE)
@interface TextFieldUtils : NSObject

// This method is a polyfill for a method on NSTextField on macOS 10.12+.
// TODO(https://crbug.com/1241080): Once we only support 10.12+, delete this.
+ (NSTextField*)labelWithString:(NSString*)text;

@end

#endif  // UI_BASE_COCOA_CONTROLS_TEXTFIELD_UTILS_H_
