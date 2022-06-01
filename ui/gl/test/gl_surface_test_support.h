// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_TEST_GL_SURFACE_TEST_SUPPORT_H_
#define UI_GL_TEST_GL_SURFACE_TEST_SUPPORT_H_

#include "ui/gl/gl_implementation.h"

namespace gl {

class GLSurfaceTestSupport {
 public:
  static void InitializeOneOff();
  static void InitializeNoExtensionsOneOff();
  static void InitializeOneOffImplementation(GLImplementationParts impl,
                                             bool fallback_to_swiftshader);
  static void InitializeOneOffWithMockBindings();
  static void InitializeOneOffWithStubBindings();
  static void ShutdownGL();
};

}  // namespace gl

#endif  // UI_GL_TEST_GL_SURFACE_TEST_SUPPORT_H_
