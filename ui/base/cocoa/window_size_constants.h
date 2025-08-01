// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_WINDOW_SIZE_CONSTANTS_H_
#define UI_BASE_COCOA_WINDOW_SIZE_CONSTANTS_H_

#include "base/component_export.h"

#import <Foundation/Foundation.h>

namespace ui {

// It is not valid to make a zero-sized window. Use this constant instead.
COMPONENT_EXPORT(UI_BASE) extern const NSRect kWindowSizeDeterminedLater;

}  // namespace ui

#endif  // UI_BASE_COCOA_WINDOW_SIZE_CONSTANTS_H_
