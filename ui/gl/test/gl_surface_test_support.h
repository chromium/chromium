// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_TEST_GL_SURFACE_TEST_SUPPORT_H_
#define UI_GL_TEST_GL_SURFACE_TEST_SUPPORT_H_

#include "ui/gl/gl_display.h"
#include "ui/gl/gl_implementation.h"

namespace gl {

class GLSurfaceTestSupport {
 public:
  static GLDisplay* InitializeOneOff();
  static GLDisplay* InitializeNoExtensionsOneOff();
  static GLDisplay* InitializeOneOffImplementation(
      GLImplementationParts impl,
      bool fallback_to_swiftshader);
  static GLDisplay* InitializeOneOffWithMockBindings();
  static GLDisplay* InitializeOneOffWithStubBindings();
  static void ShutdownGL(GLDisplay* display);
};

}  // namespace gl

#endif  // UI_GL_TEST_GL_SURFACE_TEST_SUPPORT_H_
