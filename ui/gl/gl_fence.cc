// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_fence.h"

#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_fence_arb.h"
#include "ui/gl/gl_fence_egl.h"
#include "ui/gl/gl_fence_nv.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_version_info.h"

#if BUILDFLAG(IS_APPLE)
#include "ui/gl/gl_fence_apple.h"
#endif

#if defined(USE_EGL)
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
#define USE_GL_FENCE_ANDROID_NATIVE_FENCE_SYNC
#include "ui/gl/gl_fence_android_native_fence_sync.h"
#endif

#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_surface_egl.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "ui/gl/gl_fence_win.h"
#endif

namespace gl {

GLFence::GLFence() {
}

GLFence::~GLFence() {
}

bool GLFence::IsSupported() {
  DCHECK(g_current_gl_version && g_current_gl_driver);
#if !BUILDFLAG(IS_APPLE) && defined(USE_EGL)
  GLDisplayEGL* display = GLDisplayEGL::GetDisplayForCurrentContext();
#endif  // !ISAPPLE && USE_EGL

  return g_current_gl_driver->ext.b_GL_ARB_sync ||
         g_current_gl_version->is_es3 ||
         g_current_gl_version->is_desktop_core_profile ||
#if BUILDFLAG(IS_APPLE)
         g_current_gl_driver->ext.b_GL_APPLE_fence ||
#elif defined(USE_EGL)
         (display && display->ext->b_EGL_KHR_fence_sync) ||
#endif
         g_current_gl_driver->ext.b_GL_NV_fence;
}

std::unique_ptr<GLFence> GLFence::Create() {
  DCHECK(GLContext::GetCurrent())
      << "Trying to create fence with no context";

  std::unique_ptr<GLFence> fence;

#if !BUILDFLAG(IS_APPLE) && defined(USE_EGL)
  GLDisplayEGL* display = GLDisplayEGL::GetDisplayForCurrentContext();
  if (display && display->ext->b_EGL_KHR_fence_sync &&
      display->ext->b_EGL_KHR_wait_sync) {
    // Prefer GLFenceEGL which doesn't require GL context switching.
    fence = GLFenceEGL::Create();
    DCHECK(fence);
    DCHECK(GLFence::IsSupported());
    return fence;
  }
#endif  // !IS_APPLE && USE_EGL

  if (g_current_gl_driver->ext.b_GL_ARB_sync || g_current_gl_version->is_es3 ||
      g_current_gl_version->is_desktop_core_profile) {
    // Prefer ARB_sync which supports server-side wait.
    fence = std::make_unique<GLFenceARB>();
    DCHECK(fence);
#if BUILDFLAG(IS_APPLE)
  } else if (g_current_gl_driver->ext.b_GL_APPLE_fence) {
    fence = std::make_unique<GLFenceAPPLE>();
    DCHECK(fence);
#elif defined(USE_EGL)
  } else if (display && display->ext->b_EGL_KHR_fence_sync) {
    fence = GLFenceEGL::Create();
    DCHECK(fence);
#endif
  } else if (g_current_gl_driver->ext.b_GL_NV_fence) {
    fence = std::make_unique<GLFenceNV>();
    DCHECK(fence);
  }

  DCHECK_EQ(!!fence.get(), GLFence::IsSupported());
  return fence;
}

bool GLFence::ResetSupported() {
  // Resetting a fence to its original state isn't supported by default.
  return false;
}

void GLFence::ResetState() {
  NOTIMPLEMENTED();
}

void GLFence::Invalidate() {
  NOTIMPLEMENTED();
}

bool GLFence::IsGpuFenceSupported() {
#if defined(USE_GL_FENCE_ANDROID_NATIVE_FENCE_SYNC)
  return gl::GLSurfaceEGL::GetGLDisplayEGL()
      ->IsAndroidNativeFenceSyncSupported();
#elif BUILDFLAG(IS_WIN)
  return gl::GLFenceWin::IsSupported();
#else
  return false;
#endif
}

// static
std::unique_ptr<GLFence> GLFence::CreateFromGpuFence(
    const gfx::GpuFence& gpu_fence) {
  DCHECK(IsGpuFenceSupported());
#if defined(USE_GL_FENCE_ANDROID_NATIVE_FENCE_SYNC)
  return GLFenceAndroidNativeFenceSync::CreateFromGpuFence(gpu_fence);
#elif BUILDFLAG(IS_WIN)
  return GLFenceWin::CreateFromGpuFence(gpu_fence);
#else
  NOTREACHED();
  return nullptr;
#endif
}

// static
std::unique_ptr<GLFence> GLFence::CreateForGpuFence() {
  DCHECK(IsGpuFenceSupported());
#if defined(USE_GL_FENCE_ANDROID_NATIVE_FENCE_SYNC)
  return GLFenceAndroidNativeFenceSync::CreateForGpuFence();
#elif BUILDFLAG(IS_WIN)
  return GLFenceWin::CreateForGpuFence();
#else
  NOTREACHED();
  return nullptr;
#endif
}

std::unique_ptr<gfx::GpuFence> GLFence::GetGpuFence() {
  return nullptr;
}

}  // namespace gl
