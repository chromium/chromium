// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_fence_egl.h"

#include "base/memory/ptr_util.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_bindings_autogen_gl.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_surface_egl.h"

namespace gl {

namespace {
bool g_check_egl_fence_before_wait = false;
bool g_flush_before_create_fence = false;
}  // namespace

GLFenceEGL::GLFenceEGL() = default;

// static
std::unique_ptr<GLFenceEGL> GLFenceEGL::Create() {
  // Prefer EGL_ANGLE_global_fence_sync as it guarantees synchronization with
  // past submissions from all contexts, rather than the current context.
  gl::GLContext* context = gl::GLContext::GetCurrent();
  gl::GLDisplayEGL* display = context ? context->GetGLDisplayEGL() : nullptr;
  const EGLenum syncType =
      display && display->ext->b_EGL_ANGLE_global_fence_sync
          ? EGL_SYNC_GLOBAL_FENCE_ANGLE
          : EGL_SYNC_FENCE_KHR;

  auto fence = Create(syncType, nullptr);
  // Default creation isn't supposed to fail.
  DCHECK(fence);
  return fence;
}

// static
std::unique_ptr<GLFenceEGL> GLFenceEGL::Create(EGLenum type, EGLint* attribs) {
  // Can't use MakeUnique, the no-args constructor is private.
  auto fence = base::WrapUnique(new GLFenceEGL());

  if (!fence->InitializeInternal(type, attribs))
    return nullptr;
  return fence;
}

// static
void GLFenceEGL::CheckEGLFenceBeforeWait() {
  g_check_egl_fence_before_wait = true;
}

// static
void GLFenceEGL::FlushBeforeCreateFence() {
  g_flush_before_create_fence = true;
}

bool GLFenceEGL::InitializeInternal(EGLenum type, EGLint* attribs) {
  sync_ = EGL_NO_SYNC_KHR;
  display_ = eglGetCurrentDisplay();
  if (display_ != EGL_NO_DISPLAY) {
    if (g_flush_before_create_fence)
      glFlush();
    sync_ = eglCreateSyncKHR(display_, type, attribs);
    glFlush();
  }
  return sync_ != EGL_NO_SYNC_KHR;
}

bool GLFenceEGL::HasCompleted() {
  EGLint value = 0;
  if (eglGetSyncAttribKHR(display_, sync_, EGL_SYNC_STATUS_KHR, &value) !=
      EGL_TRUE) {
    LOG(ERROR) << "Failed to get EGLSync attribute. error code:"
               << eglGetError();
    return true;
  }

  DCHECK(value == EGL_SIGNALED_KHR || value == EGL_UNSIGNALED_KHR);
  return !value || value == EGL_SIGNALED_KHR;
}

void GLFenceEGL::ClientWait() {
  EGLint result = ClientWaitWithTimeoutNanos(EGL_FOREVER_KHR);
  DCHECK_NE(EGL_TIMEOUT_EXPIRED_KHR, result);
}

EGLint GLFenceEGL::ClientWaitWithTimeoutNanos(EGLTimeKHR timeout) {
  EGLint flags = 0;
  EGLint result = eglClientWaitSyncKHR(display_, sync_, flags, timeout);
  if (result == EGL_FALSE) {
    LOG(ERROR) << "Failed to wait for EGLSync. error:"
               << ui::GetLastEGLErrorString();
    CHECK(false);
  }
  return result;
}

void GLFenceEGL::ServerWait() {
  GLDisplayEGL* display = GLSurfaceEGL::GetGLDisplayEGL();
  if (!display->ext->b_EGL_KHR_wait_sync) {
    ClientWait();
    return;
  }
  EGLint flags = 0;

  bool completed = false;
  if (g_check_egl_fence_before_wait) {
    // The i965 driver ends up doing a bunch of flushing if an already
    // signalled fence is waited on. This causes performance to suffer.
    // Check whether the fence is signalled before waiting.
    completed = HasCompleted();
  }

  if (!completed && eglWaitSyncKHR(display_, sync_, flags) == EGL_FALSE) {
    LOG(ERROR) << "Failed to wait for EGLSync. error:"
               << ui::GetLastEGLErrorString();
    CHECK(false);
  }
}

void GLFenceEGL::Invalidate() {
  // Do nothing. We want the destructor to destroy the EGL fence even if the GL
  // context was lost. The EGLDisplay may still be valid, and this helps avoid
  // leaks.
}

GLFenceEGL::~GLFenceEGL() {
  if (sync_ != EGL_NO_SYNC) {
    eglDestroySyncKHR(display_, sync_);
  }
}

}  // namespace gl
