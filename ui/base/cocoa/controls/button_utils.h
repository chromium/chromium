// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_CONTROLS_BUTTON_UTILS_H_
#define UI_BASE_COCOA_CONTROLS_BUTTON_UTILS_H_

#include "base/component_export.h"

#include <Cocoa/Cocoa.h>

COMPONENT_EXPORT(UI_BASE)
@interface ButtonUtils : NSObject

// These methods are a polyfill for convenience constructors that exist on
// NSButton in macOS 10.12+.
// TODO(https://crbug.com/1241080): once we target only 10.12+, delete these.
+ (NSButton*)buttonWithTitle:(NSString*)title
                      action:(SEL)action
                      target:(id)target;

+ (NSButton*)checkboxWithTitle:(NSString*)title;

@end

#endif  // UI_BASE_COCOA_CONTROLS_BUTTON_UTILS_H_
