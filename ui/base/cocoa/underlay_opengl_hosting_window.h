// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_UNDERLAY_OPENGL_HOSTING_WINDOW_H_
#define UI_BASE_COCOA_UNDERLAY_OPENGL_HOSTING_WINDOW_H_

#import <Cocoa/Cocoa.h>

#include "base/component_export.h"

// Common base class for windows that host a OpenGL surface that renders under
// the window. Previously contained methods related to hole punching, now just
// contains common asserts.
COMPONENT_EXPORT(UI_BASE)
@interface UnderlayOpenGLHostingWindow : NSWindow
@end

#endif  // UI_BASE_COCOA_UNDERLAY_OPENGL_HOSTING_WINDOW_H_
