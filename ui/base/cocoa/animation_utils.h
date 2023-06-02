// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_ANIMATION_UTILS_H_
#define UI_BASE_COCOA_ANIMATION_UTILS_H_

#include "build/build_config.h"

#include <QuartzCore/QuartzCore.h>

class ScopedCAActionDisabler {
 public:
  ScopedCAActionDisabler() {
    [CATransaction begin];
    [CATransaction setValue:@YES forKey:kCATransactionDisableActions];
  }

  ~ScopedCAActionDisabler() {
    [CATransaction commit];
  }
};

#endif  // UI_BASE_COCOA_ANIMATION_UTILS_H_
