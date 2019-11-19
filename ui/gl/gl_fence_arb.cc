// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_fence_arb.h"

#include "base/strings/stringprintf.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"

namespace gl {

namespace {

std::string GetGLErrors() {
  // Clears and logs all current gl errors.
  std::string accumulated_errors;
  GLenum error;
  while ((error = glGetError()) != GL_NO_ERROR) {
    accumulated_errors += base::StringPrintf("0x%x ", error);
  }
  return accumulated_errors;
}

}  // namespace

GLFenceARB::GLFenceARB() {
  sync_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  DCHECK_EQ(GL_TRUE, glIsSync(sync_));
  glFlush();
}

bool GLFenceARB::HasCompleted() {
  // Handle the case where FenceSync failed.
  if (!sync_)
    return true;

  DCHECK_EQ(GL_TRUE, glIsSync(sync_));
  // We could potentially use glGetSynciv here, but it doesn't work
  // on OSX 10.7 (always says the fence is not signaled yet).
  // glClientWaitSync works better, so let's use that instead.
  GLenum result = glClientWaitSync(sync_, 0, 0);
  if (result == GL_WAIT_FAILED) {
    HandleClientWaitFailure();
    return false;
  }
  return result != GL_TIMEOUT_EXPIRED;
}

void GLFenceARB::ClientWait() {
  DCHECK_EQ(GL_TRUE, glIsSync(sync_));
  GLenum result =
      glClientWaitSync(sync_, GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
  DCHECK_NE(static_cast<GLenum>(GL_TIMEOUT_EXPIRED), result);
  if (result == GL_WAIT_FAILED) {
    HandleClientWaitFailure();
  }
}

void GLFenceARB::ServerWait() {
  DCHECK_EQ(GL_TRUE, glIsSync(sync_));
  glWaitSync(sync_, 0, GL_TIMEOUT_IGNORED);
}

GLFenceARB::~GLFenceARB() {
  if (sync_) {
    DCHECK_EQ(GL_TRUE, glIsSync(sync_));
    glDeleteSync(sync_);
  }
}

void GLFenceARB::Invalidate() {
  sync_ = 0;
}

void GLFenceARB::HandleClientWaitFailure() {
  DCHECK(GLContext::GetCurrent());
  if (GLContext::GetCurrent()->CheckStickyGraphicsResetStatus()) {
    LOG(ERROR) << "Failed to wait for GLFence; context was lost. Error code: "
               << GetGLErrors();
  } else {
    // There's no provision for reporting these failures higher up the
    // stack and thereby losing the context (or exiting the GPU
    // process) more cooperatively. Some of this code is called from
    // deep call stacks. Report the wait failure and crash with a log
    // message.
    LOG(FATAL) << "Failed to wait for GLFence. Error code: " << GetGLErrors();
  }
}

}  // namespace gl
