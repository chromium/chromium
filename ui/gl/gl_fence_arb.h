// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_FENCE_ARB_H_
#define UI_GL_GL_FENCE_ARB_H_

#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_fence.h"

namespace gl {

class GL_EXPORT GLFenceARB : public GLFence {
 public:
  GLFenceARB();

  GLFenceARB(const GLFenceARB&) = delete;
  GLFenceARB& operator=(const GLFenceARB&) = delete;

  ~GLFenceARB() override;

  // GLFence implementation:
  bool HasCompleted() override;
  void ClientWait() override;
  void ServerWait() override;
  void Invalidate() override;

 private:
  GLsync sync_ = 0;

  void HandleClientWaitFailure();
};

}  // namespace gl

#endif  // UI_GL_GL_FENCE_ARB_H_
