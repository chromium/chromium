// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_FENCE_APPLE_H_
#define UI_GL_GL_FENCE_APPLE_H_

#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_fence.h"

namespace gl {

class GL_EXPORT GLFenceAPPLE : public GLFence {
 public:
  GLFenceAPPLE();

  GLFenceAPPLE(const GLFenceAPPLE&) = delete;
  GLFenceAPPLE& operator=(const GLFenceAPPLE&) = delete;

  ~GLFenceAPPLE() override;

  // GLFence implementation:
  bool HasCompleted() override;
  void ClientWait() override;
  void ServerWait() override;
  void Invalidate() override;

 private:
  GLuint fence_ = 0;
};

}  // namespace gl

#endif  // UI_GL_GL_FENCE_APPLE_H_
