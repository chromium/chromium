// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/drag_permission.h"

#import <AppKit/AppKit.h>

#include "base/check_op.h"

namespace ui {

namespace {

// There is SPI to inquire as to the existence of a current drag session
// ([_NSDragManager isDragging]). However, usage of SPI is avoided if it is easy
// to do so, and it is easy in this case.
bool gDragSessionIsInProgress;

}  // namespace

bool IsDragSessionInitiationAllowed() {
  CHECK_EQ(NSRunLoop.currentRunLoop, NSRunLoop.mainRunLoop);
  return !gDragSessionIsInProgress &&
         NSRunLoop.currentRunLoop.currentMode != NSEventTrackingRunLoopMode;
}

void SetDragSessionInProgress(bool in_progress) {
  CHECK_NE(gDragSessionIsInProgress, in_progress);
  gDragSessionIsInProgress = in_progress;
}

}  // namespace ui
