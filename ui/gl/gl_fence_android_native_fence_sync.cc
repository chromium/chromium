// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_fence_android_native_fence_sync.h"

#include <sync/sync.h>

#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gl/gl_surface_egl.h"

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"
#endif

namespace gl {

GLFenceAndroidNativeFenceSync::GLFenceAndroidNativeFenceSync() {}

GLFenceAndroidNativeFenceSync::~GLFenceAndroidNativeFenceSync() {}

// static
std::unique_ptr<GLFenceAndroidNativeFenceSync>
GLFenceAndroidNativeFenceSync::CreateInternal(EGLenum type, EGLint* attribs) {
  DCHECK(GLSurfaceEGL::IsAndroidNativeFenceSyncSupported());

  // Can't use MakeUnique, the no-args constructor is private.
  auto fence = base::WrapUnique(new GLFenceAndroidNativeFenceSync());

  if (!fence->InitializeInternal(type, attribs))
    return nullptr;
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
  gfx::GpuFenceHandle handle =
      gfx::CloneHandleForIPC(gpu_fence.GetGpuFenceHandle());
  DCHECK_GE(handle.native_fd.fd, 0);
  EGLint attribs[] = {EGL_SYNC_NATIVE_FENCE_FD_ANDROID, handle.native_fd.fd,
                      EGL_NONE};
  return CreateInternal(EGL_SYNC_NATIVE_FENCE_ANDROID, attribs);
}

std::unique_ptr<gfx::GpuFence> GLFenceAndroidNativeFenceSync::GetGpuFence() {
  DCHECK(GLSurfaceEGL::IsAndroidNativeFenceSyncSupported());

  EGLint sync_fd = eglDupNativeFenceFDANDROID(display_, sync_);
  if (sync_fd < 0)
    return nullptr;

  gfx::GpuFenceHandle handle;
  handle.type = gfx::GpuFenceHandleType::kAndroidNativeFenceSync;
  handle.native_fd = base::FileDescriptor(sync_fd, /*auto_close=*/true);

  return std::make_unique<gfx::GpuFence>(handle);
}

base::TimeTicks GLFenceAndroidNativeFenceSync::GetStatusChangeTime() {
  EGLint sync_fd = eglDupNativeFenceFDANDROID(display_, sync_);
  if (sync_fd < 0)
    return base::TimeTicks();

  base::ScopedFD scoped_fd(sync_fd);
  base::TimeTicks time;
  GetStatusChangeTimeForFence(sync_fd, &time);
  return time;
}

// static
GLFenceAndroidNativeFenceSync::Status
GLFenceAndroidNativeFenceSync::GetStatusChangeTimeForFence(
    int fd,
    base::TimeTicks* time) {
  DCHECK_NE(fd, -1);

  auto info =
      std::unique_ptr<sync_fence_info_data, void (*)(sync_fence_info_data*)>{
          sync_fence_info(fd), sync_fence_info_free};
  if (!info) {
    LOG(ERROR) << "sync_fence_info returned null for fd : " << fd;
    return Status::kInvalid;
  }

  const bool signaled = info->status == 1;
  if (!signaled)
    return Status::kNotSignaled;

  struct sync_pt_info* pt_info = nullptr;
  uint64_t timestamp_ns = 0u;
  while ((pt_info = sync_pt_info(info.get(), pt_info)))
    timestamp_ns = std::max(timestamp_ns, pt_info->timestamp_ns);

  if (timestamp_ns == 0u) {
    LOG(ERROR) << "No timestamp provided from sync_pt_info for fd : " << fd;
    return Status::kInvalid;
  }

  *time = base::TimeTicks() + base::TimeDelta::FromNanoseconds(timestamp_ns);
  return Status::kSignaled;
}

}  // namespace gl
