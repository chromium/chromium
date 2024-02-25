// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/android/egl_fence_utils.h"

#include "ui/gl/gl_fence_android_native_fence_sync.h"

namespace gl {

base::ScopedFD CreateEglFenceAndExportFd() {
  std::unique_ptr<gl::GLFenceAndroidNativeFenceSync> android_native_fence =
      gl::GLFenceAndroidNativeFenceSync::CreateForGpuFence();
  if (!android_native_fence) {
    LOG(ERROR) << "Failed to create android native fence sync object.";
    return base::ScopedFD();
  }
  std::unique_ptr<gfx::GpuFence> gpu_fence =
      android_native_fence->GetGpuFence();
  if (!gpu_fence) {
    LOG(ERROR) << "Unable to get a gpu fence object.";
    return base::ScopedFD();
  }
  gfx::GpuFenceHandle fence_handle = gpu_fence->GetGpuFenceHandle().Clone();
  if (fence_handle.is_null()) {
    LOG(ERROR) << "Gpu fence handle is null";
    return base::ScopedFD();
  }
  return fence_handle.Release();
}

bool InsertEglFenceAndWait(base::ScopedFD acquire_fence_fd) {
  int fence_fd = acquire_fence_fd.release();

  // If fence_fd is -1, we do not need synchronization fence and image is ready
  // to be used immediately. Also we dont need to close any fd. Else we need to
  // create a sync fence which is used to signal when the buffer is ready to be
  // consumed.
  if (fence_fd == -1)
    return true;

  // Create attribute-value list with the fence_fd.
  EGLint attribs[] = {EGL_SYNC_NATIVE_FENCE_FD_ANDROID, fence_fd, EGL_NONE};

  // Create and insert the fence sync gl command using the helper class in
  // gl_fence_egl.h. This method takes the ownership of the file descriptor if
  // it succeeds.
  std::unique_ptr<gl::GLFenceEGL> egl_fence(
      gl::GLFenceEGL::Create(EGL_SYNC_NATIVE_FENCE_ANDROID, attribs));

  // If above method fails to create an egl_fence, we need to close the file
  // descriptor.
  if (egl_fence == nullptr) {
    // Create a scoped FD to close fence_fd.
    base::ScopedFD temp_fd(fence_fd);
    LOG(ERROR) << " Failed to created egl fence object ";
    return false;
  }

  // Make the server wait and not the client.
  egl_fence->ServerWait();
  return true;
}

}  // namespace gl
