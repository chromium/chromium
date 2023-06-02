// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/quartz_util.h"

#include <QuartzCore/QuartzCore.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ui {

void BeginCATransaction() {
  [CATransaction begin];
}

void CommitCATransaction() {
  [CATransaction commit];
}

}  // namespace ui
