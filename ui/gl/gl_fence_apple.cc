// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_fence_apple.h"

#include "ui/gl/gl_bindings.h"

namespace gl {

GLFenceAPPLE::GLFenceAPPLE() {
  glGenFencesAPPLE(1, &fence_);
  glSetFenceAPPLE(fence_);
  DCHECK(glIsFenceAPPLE(fence_));
  glFlush();
}

bool GLFenceAPPLE::HasCompleted() {
  DCHECK(glIsFenceAPPLE(fence_));
  return !!glTestFenceAPPLE(fence_);
}

void GLFenceAPPLE::ClientWait() {
  DCHECK(glIsFenceAPPLE(fence_));
  glFinishFenceAPPLE(fence_);
}

void GLFenceAPPLE::ServerWait() {
  DCHECK(glIsFenceAPPLE(fence_));
  ClientWait();
}

GLFenceAPPLE::~GLFenceAPPLE() {
  if (fence_) {
    DCHECK(glIsFenceAPPLE(fence_));
    glDeleteFencesAPPLE(1, &fence_);
  }
}

void GLFenceAPPLE::Invalidate() {
  fence_ = 0;
}

}  // namespace gl
