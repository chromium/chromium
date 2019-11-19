// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_fence.h"

#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence_arb.h"
#include "ui/gl/gl_fence_egl.h"
#include "ui/gl/gl_fence_nv.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_version_info.h"

#if defined(OS_MACOSX)
#include "ui/gl/gl_fence_apple.h"
#endif

#if defined(USE_EGL) && defined(OS_POSIX) && !defined(OS_MACOSX)
#define USE_GL_FENCE_ANDROID_NATIVE_FENCE_SYNC
#include "ui/gl/gl_fence_android_native_fence_sync.h"
#include "ui/gl/gl_surface_egl.h"
#endif

namespace gl {

GLFence::GLFence() {
}

GLFence::~GLFence() {
}

bool GLFence::IsSupported() {
  DCHECK(g_current_gl_version && g_current_gl_driver);
  return g_current_gl_driver->ext.b_GL_ARB_sync ||
         g_current_gl_version->is_es3 ||
         g_current_gl_version->is_desktop_core_profile ||
#if defined(OS_MACOSX)
         g_current_gl_driver->ext.b_GL_APPLE_fence ||
#else
         g_driver_egl.ext.b_EGL_KHR_fence_sync ||
#endif
         g_current_gl_driver->ext.b_GL_NV_fence;
}

std::unique_ptr<GLFence> GLFence::Create() {
  DCHECK(GLContext::GetCurrent())
      << "Trying to create fence with no context";

  std::unique_ptr<GLFence> fence;
#if !defined(OS_MACOSX)
  if (g_driver_egl.ext.b_EGL_KHR_fence_sync &&
      g_driver_egl.ext.b_EGL_KHR_wait_sync) {
    // Prefer GLFenceEGL which doesn't require GL context switching.
    fence = GLFenceEGL::Create();
    DCHECK(fence);
  } else
#endif
      if (g_current_gl_driver->ext.b_GL_ARB_sync ||
          g_current_gl_version->is_es3 ||
          g_current_gl_version->is_desktop_core_profile) {
    // Prefer ARB_sync which supports server-side wait.
    fence = std::make_unique<GLFenceARB>();
#if defined(OS_MACOSX)
  } else if (g_current_gl_driver->ext.b_GL_APPLE_fence) {
    fence = std::make_unique<GLFenceAPPLE>();
#else
  } else if (g_driver_egl.ext.b_EGL_KHR_fence_sync) {
    fence = GLFenceEGL::Create();
    DCHECK(fence);
#endif
  } else if (g_current_gl_driver->ext.b_GL_NV_fence) {
    fence = std::make_unique<GLFenceNV>();
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
  return gl::GLSurfaceEGL::IsAndroidNativeFenceSyncSupported();
#else
  return false;
#endif
}

// static
std::unique_ptr<GLFence> GLFence::CreateFromGpuFence(
    const gfx::GpuFence& gpu_fence) {
  DCHECK(IsGpuFenceSupported());
  switch (gpu_fence.GetGpuFenceHandle().type) {
    case gfx::GpuFenceHandleType::kAndroidNativeFenceSync:
#if defined(USE_GL_FENCE_ANDROID_NATIVE_FENCE_SYNC)
      return GLFenceAndroidNativeFenceSync::CreateFromGpuFence(gpu_fence);
#else
      NOTREACHED();
      return nullptr;
#endif
    default:
      NOTREACHED();
      return nullptr;
  }
}

// static
std::unique_ptr<GLFence> GLFence::CreateForGpuFence() {
  DCHECK(IsGpuFenceSupported());
#if defined(USE_GL_FENCE_ANDROID_NATIVE_FENCE_SYNC)
  return GLFenceAndroidNativeFenceSync::CreateForGpuFence();
#endif
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<gfx::GpuFence> GLFence::GetGpuFence() {
  return nullptr;
}

}  // namespace gl
