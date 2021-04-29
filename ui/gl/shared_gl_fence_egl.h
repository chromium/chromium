// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_SHARED_GL_FENCE_EGL_H_
#define UI_GL_SHARED_GL_FENCE_EGL_H_

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "ui/gl/gl_export.h"

namespace gl {

class GLFenceEGL;
class GLApi;

// This class is an optimized way to share an egl fence among multiple
// consumers. Once the shared |egl_fence_| has been waited upon by any of the
// consumer, all the future waits to the same fence becomes no-op since we don't
// need to wait again on the same fence any more. This saves un-neccesary gl
// calls issued to do wait by each consumer.
// This object should only be shared among consumers of the same GL context
// which is true for Webview case.
// TODO(vikassoni): Add logic to handle consumers from different GL context.
class GL_EXPORT SharedGLFenceEGL
    : public base::RefCountedThreadSafe<SharedGLFenceEGL> {
 public:
  SharedGLFenceEGL();

  // Issues a ServerWait on the |egl_fence_|.
  void ServerWait();

 protected:
  virtual ~SharedGLFenceEGL();

 private:
  friend class base::RefCountedThreadSafe<SharedGLFenceEGL>;

  std::unique_ptr<GLFenceEGL> egl_fence_ GUARDED_BY(lock_);

  // A lock that guard against multiple threads trying to access |egl_fence_|.
  base::Lock lock_;

  // GLApi on which all the consumers for this object should be on.
  gl::GLApi* gl_api_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SharedGLFenceEGL);
};

}  // namespace gl

#endif  // UI_GL_SHARED_GL_FENCE_EGL_H_
