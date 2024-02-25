// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_fence_android_native_fence_sync.h"

#include <sync/sync.h>

#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gl/gl_surface_egl.h"

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"
#endif

namespace gl {

GLFenceAndroidNativeFenceSync::GLFenceAndroidNativeFenceSync() {}

GLFenceAndroidNativeFenceSync::~GLFenceAndroidNativeFenceSync() {}

// static
std::unique_ptr<GLFenceAndroidNativeFenceSync>
GLFenceAndroidNativeFenceSync::CreateInternal(EGLenum type, EGLint* attribs) {
  DCHECK(GLSurfaceEGL::GetGLDisplayEGL()->IsAndroidNativeFenceSyncSupported());

  // Can't use MakeUnique, the no-args constructor is private.
  auto fence = base::WrapUnique(new GLFenceAndroidNativeFenceSync());

  if (!fence->InitializeInternal(type, attribs)) {
    return nullptr;
  }
  return fence;
}

// static
std::unique_ptr<GLFenceAndroidNativeFenceSync>
GLFenceAndroidNativeFenceSync::CreateForGpuFence() {
  return CreateInternal(EGL_SYNC_NATIVE_FENCE_ANDROID, nullptr);
}

// static
std::unique_ptr<GLFenceAndroidNativeFenceSync>
GLFenceAndroidNativeFenceSync::CreateFromGpuFence(
    const gfx::GpuFence& gpu_fence) {
  gfx::GpuFenceHandle handle = gpu_fence.GetGpuFenceHandle().Clone();
  DCHECK_GE(handle.Peek(), 0);
  EGLint attribs[] = {EGL_SYNC_NATIVE_FENCE_FD_ANDROID,
                      handle.Release().release(), EGL_NONE};
  return CreateInternal(EGL_SYNC_NATIVE_FENCE_ANDROID, attribs);
}

std::unique_ptr<gfx::GpuFence> GLFenceAndroidNativeFenceSync::GetGpuFence() {
  DCHECK(GLSurfaceEGL::GetGLDisplayEGL()->IsAndroidNativeFenceSyncSupported());

  const EGLint sync_fd = eglDupNativeFenceFDANDROID(display_, sync_);
  if (sync_fd < 0) {
    return nullptr;
  }

  gfx::GpuFenceHandle handle;
  handle.Adopt(base::ScopedFD(sync_fd));

  return std::make_unique<gfx::GpuFence>(std::move(handle));
}

base::TimeTicks GLFenceAndroidNativeFenceSync::GetStatusChangeTime() {
  EGLint sync_fd = eglDupNativeFenceFDANDROID(display_, sync_);
  if (sync_fd < 0)
    return base::TimeTicks();

  base::ScopedFD scoped_fd(sync_fd);
  base::TimeTicks time;
  gfx::GpuFence::GetStatusChangeTime(sync_fd, &time);
  return time;
}

}  // namespace gl
