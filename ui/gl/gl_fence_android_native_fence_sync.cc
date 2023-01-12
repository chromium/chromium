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

#if BUILDFLAG(IS_CHROMEOS)
#include <poll.h>
#include <sys/resource.h>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "components/crash/core/common/crash_key.h"
#endif

namespace {

#if BUILDFLAG(IS_CHROMEOS)
// TODO(hitawala): Merge this and viz::GatherFDStats functions.
bool GatherFDStats(uint32_t& fd_max,
                   uint32_t& active_fd_count,
                   uint32_t& rlim_cur) {
  // https://stackoverflow.com/questions/7976769/
  // getting-count-of-current-used-file-descriptors-from-c-code
  rlimit limit_data;
  getrlimit(RLIMIT_NOFILE, &limit_data);
  std::vector<pollfd> poll_data;
  constexpr uint32_t kMaxNumFDTested = 1 << 16;
  // |rlim_cur| is the soft max but is likely the value we can rely on instead
  // of the real max.
  rlim_cur = limit_data.rlim_cur;
  fd_max = std::max(1u, std::min(rlim_cur, kMaxNumFDTested));
  poll_data.resize(fd_max);
  for (size_t i = 0; i < poll_data.size(); i++) {
    auto& each = poll_data[i];
    each.fd = static_cast<int>(i);
    each.events = 0;
    each.revents = 0;
  }

  poll(poll_data.data(), poll_data.size(), 0);
  active_fd_count = 0;
  for (auto&& each : poll_data) {
    if (each.revents != POLLNVAL) {
      active_fd_count++;
    }
  }
  return true;
}

void SetFDCrashKeys() {
  uint32_t fd_max;
  uint32_t active_fd_count;
  uint32_t rlim_cur;

  if (!GatherFDStats(fd_max, active_fd_count, rlim_cur)) {
    return;
  }
  static crash_reporter::CrashKeyString<32> num_fd_max("num-fd-max");
  static crash_reporter::CrashKeyString<32> num_fd_soft_max("num-fd-soft-max");
  static crash_reporter::CrashKeyString<32> num_fd_active("num-fd-active");
  num_fd_max.Set(base::NumberToString(fd_max));
  num_fd_soft_max.Set(base::NumberToString(rlim_cur));
  num_fd_active.Set(base::NumberToString(active_fd_count));
}
#endif

}  // namespace

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
#if BUILDFLAG(IS_CHROMEOS)
    SetFDCrashKeys();
#endif
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
  DCHECK_GE(handle.owned_fd.get(), 0);
  EGLint attribs[] = {EGL_SYNC_NATIVE_FENCE_FD_ANDROID,
                      handle.owned_fd.release(), EGL_NONE};
  return CreateInternal(EGL_SYNC_NATIVE_FENCE_ANDROID, attribs);
}

std::unique_ptr<gfx::GpuFence> GLFenceAndroidNativeFenceSync::GetGpuFence() {
  DCHECK(GLSurfaceEGL::GetGLDisplayEGL()->IsAndroidNativeFenceSyncSupported());

  const EGLint sync_fd = eglDupNativeFenceFDANDROID(display_, sync_);
  if (sync_fd < 0) {
#if BUILDFLAG(IS_CHROMEOS)
    SetFDCrashKeys();
#endif
    return nullptr;
  }

  gfx::GpuFenceHandle handle;
  handle.owned_fd = base::ScopedFD(sync_fd);

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
