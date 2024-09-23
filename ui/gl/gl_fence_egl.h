// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_FENCE_EGL_H_
#define UI_GL_GL_FENCE_EGL_H_

#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_fence.h"

namespace gl {

class GL_EXPORT GLFenceEGL : public GLFence {
 public:
  GLFenceEGL(const GLFenceEGL&) = delete;
  GLFenceEGL& operator=(const GLFenceEGL&) = delete;

  ~GLFenceEGL() override;

  // Factory method using default initialization.
  static std::unique_ptr<GLFenceEGL> Create();

  // Factory method using custom initialization.
  static std::unique_ptr<GLFenceEGL> Create(EGLenum type, EGLint* attribs);

  // On i965, passing an already signalled fence has a large performance
  // cost. See crbug.com/1246254. This function should be called at
  // initialization to enable checking the fence before waiting on
  // i965 platforms. TODO(crbug.com/40196471): Remove this.
  static void CheckEGLFenceBeforeWait();

  static void FlushBeforeCreateFence();

  // GLFence implementation:
  bool HasCompleted() override;
  void ClientWait() override;
  void ServerWait() override;
  void Invalidate() override;

  // EGL-specific wait-with-timeout implementation:
  EGLint ClientWaitWithTimeoutNanos(EGLTimeKHR timeout);

 protected:
  GLFenceEGL();
  bool InitializeInternal(EGLenum type, EGLint* attribs);

  EGLSyncKHR sync_;
  EGLDisplay display_;
};

}  // namespace gl

#endif  // UI_GL_GL_FENCE_EGL_H_
