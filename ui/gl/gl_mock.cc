// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_mock.h"

namespace gl {

namespace {

// This is called mainly to prevent the compiler combining the code of mock
// functions with identical contents, so that their function pointers will be
// different.
void MakeFunctionUnique(const char* func_name) {
  VLOG(2) << "Calling mock " << func_name;
}

}  // namespace anonymous

MockGLInterface::MockGLInterface() {
}

MockGLInterface::~MockGLInterface() {
}

MockGLInterface* MockGLInterface::interface_;

void MockGLInterface::SetGLInterface(MockGLInterface* gl_interface) {
  interface_ = gl_interface;
}

void GL_BINDING_CALL MockGLInterface::Mock_glTexSubImage3DNoData(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
    GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type) {
  MakeFunctionUnique("glTexSubImage3DNoData");
  interface_->TexSubImage3DNoData(
      target, level, xoffset, yoffset, zoffset, width, height, depth,
      format, type);
}

}  // namespace gl
