// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_WINDOW_SIZE_CONSTANTS_H_
#define UI_BASE_COCOA_WINDOW_SIZE_CONSTANTS_H_

#import <Foundation/Foundation.h>

namespace ui {

// It is not valid to make a zero-sized window. Use this constant instead.
inline constexpr NSRect kWindowSizeDeterminedLater = {{0, 0}, {1, 1}};

}  // namespace ui

#endif  // UI_BASE_COCOA_WINDOW_SIZE_CONSTANTS_H_
