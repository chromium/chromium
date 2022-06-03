// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/underlay_opengl_hosting_window.h"

#include "base/check.h"

@implementation UnderlayOpenGLHostingWindow

- (instancetype)initWithContentRect:(NSRect)contentRect
                          styleMask:(NSUInteger)windowStyle
                            backing:(NSBackingStoreType)bufferingType
                              defer:(BOOL)deferCreation {
  // It is invalid to create windows with zero width or height. It screws things
  // up royally:
  // - It causes console spew: <http://crbug.com/78973>
  // - It breaks Expose: <http://sourceforge.net/projects/heat-meteo/forums/forum/268087/topic/4582610>
  //
  // This is a banned practice
  // <http://www.chromium.org/developers/coding-style/cocoa-dos-and-donts>. Do
  // not do this. Use kWindowSizeDeterminedLater in
  // ui/base/cocoa/window_size_constants.h instead.
  //
  // (This is checked here because UnderlayOpenGLHostingWindow is the base of
  // most Chromium windows, not because this is related to its functionality.)
  DCHECK(!NSIsEmptyRect(contentRect));
  self = [super initWithContentRect:contentRect
                          styleMask:windowStyle
                            backing:bufferingType
                             defer:deferCreation];
  return self;
}

@end
