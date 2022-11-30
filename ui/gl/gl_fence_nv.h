// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_FENCE_NV_H_
#define UI_GL_GL_FENCE_NV_H_

#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_fence.h"

namespace gl {

class GL_EXPORT GLFenceNV : public GLFence {
 public:
  GLFenceNV();

  GLFenceNV(const GLFenceNV&) = delete;
  GLFenceNV& operator=(const GLFenceNV&) = delete;

  ~GLFenceNV() override;

  // GLFence implementation:
  bool ResetSupported() override;
  void ResetState() override;
  bool HasCompleted() override;
  void ClientWait() override;
  void ServerWait() override;
  void Invalidate() override;

 private:
  GLuint fence_ = 0;
};

}  // namespace gl

#endif  // UI_GL_GL_FENCE_NV_H_
