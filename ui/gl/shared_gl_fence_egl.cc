// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/shared_gl_fence_egl.h"

#include "base/logging.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_fence_egl.h"

namespace gl {

SharedGLFenceEGL::SharedGLFenceEGL() : egl_fence_(GLFenceEGL::Create()) {
  // GLFenceEGL::Create() is not supposed to fail.
  DCHECK(egl_fence_);
}

SharedGLFenceEGL::~SharedGLFenceEGL() = default;

void SharedGLFenceEGL::ServerWait() {
  base::AutoLock lock(lock_);

#if DCHECK_IS_ON()
  if (!gl_api_) {
    gl_api_ = gl::g_current_gl_context;
  } else if (gl_api_ != gl::g_current_gl_context) {
    LOG(FATAL) << "This object should be shared among consumers on the same GL "
                  "context";
  }
#endif

  // If there is a fence, we do a wait on it. Once it has been waited upon, we
  // clear the fence and all future call to this method will be a no-op since we
  // do not need to wait on that same fence any more.
  if (egl_fence_) {
    egl_fence_->ServerWait();
    egl_fence_.reset();
  }
}

}  // namespace gl
