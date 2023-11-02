// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements mock EGL Interface for unit testing. The interface
// corresponds to the set of functionally distinct EGL functions defined in
// generate_bindings.py.

#ifndef UI_GL_EGL_MOCK_H_
#define UI_GL_EGL_MOCK_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gl/gl_bindings.h"

namespace gl {

using GLFunctionPointerType = void (*)();

class MockEGLInterface {
 public:
  MockEGLInterface();
  virtual ~MockEGLInterface();

  // Set the functions called from the mock EGL implementation for the purposes
  // of testing.
  static void SetEGLInterface(MockEGLInterface* egl_interface);

  // Find an entry point to the mock GL implementation.
  static GLFunctionPointerType GL_BINDING_CALL
  GetGLProcAddress(const char* name);

// Include the auto-generated parts of this class. We split this because
// it means we can easily edit the non-auto generated parts right here in
// this file instead of having to edit some template or the code generator.

// Member functions
#include "gl_mock_autogen_egl.h"

 private:
  static MockEGLInterface* interface_;

// Static mock functions that invoke the member functions of interface_.
#include "egl_bindings_autogen_mock.h"
};

}  // namespace gl

#endif  // UI_GL_EGL_MOCK_H_
