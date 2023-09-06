// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gpu_fence_handle.h"
#include <atomic>
#include <cstddef>

#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "ui/gfx/switches.h"

#if BUILDFLAG(IS_POSIX)
#include <unistd.h>
#include "base/posix/eintr_wrapper.h"
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include "base/fuchsia/fuchsia_logging.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#include "base/process/process_handle.h"
#endif

namespace {
gfx::GpuFenceHandle::ScopedPlatformFence PlatformDuplicate(
    const gfx::GpuFenceHandle::ScopedPlatformFence& scoped_fence) {
#if BUILDFLAG(IS_POSIX)
  return base::ScopedFD(HANDLE_EINTR(dup(scoped_fence.get())));
#elif BUILDFLAG(IS_FUCHSIA)
  zx::event temp_event;
  zx_status_t status =
      scoped_fence.duplicate(ZX_RIGHT_SAME_RIGHTS, &temp_event);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "zx_handle_duplicate";
    return gfx::GpuFenceHandle::ScopedPlatformFence();
  }
  return temp_event;
#elif BUILDFLAG(IS_WIN)
  const base::ProcessHandle process = ::GetCurrentProcess();
  HANDLE duplicated_handle = INVALID_HANDLE_VALUE;
  const BOOL result =
      ::DuplicateHandle(process, scoped_fence.Get(), process,
                        &duplicated_handle, 0, FALSE, DUPLICATE_SAME_ACCESS);
  if (!result) {
    const DWORD last_error = ::GetLastError();
    base::debug::Alias(&last_error);
    CHECK(false);
  }
  return base::win::ScopedHandle(duplicated_handle);
#else
  NOTREACHED();
#endif
}

}  // namespace

namespace gfx {

GpuFenceHandle::GpuFenceHandle() = default;

GpuFenceHandle::GpuFenceHandle(GpuFenceHandle&& other) = default;

GpuFenceHandle& GpuFenceHandle::operator=(GpuFenceHandle&& other) = default;

GpuFenceHandle::~GpuFenceHandle() = default;

void GpuFenceHandle::Reset() {
  owned_fence_ = ScopedPlatformFence();
}

void GpuFenceHandle::Adopt(ScopedPlatformFence scoped_fence) {
  owned_fence_ = std::move(scoped_fence);
}

GpuFenceHandle::ScopedPlatformFence GpuFenceHandle::Release() {
  if (is_null()) {
    return ScopedPlatformFence();
  }
  return std::move(owned_fence_);
}

GpuFenceHandle GpuFenceHandle::Clone() const {
  if (is_null()) {
    return GpuFenceHandle();
  }

  gfx::GpuFenceHandle handle;
  handle.owned_fence_ = PlatformDuplicate(owned_fence_);
  return handle;
}

bool GpuFenceHandle::is_null() const {
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  return !owned_fence_.is_valid();
#elif BUILDFLAG(IS_WIN)
  return !owned_fence_.IsValid();
#else
  return true;
#endif
}

#if BUILDFLAG(IS_POSIX)
int GpuFenceHandle::Peek() const {
  return is_null() ? base::ScopedFD().get() : owned_fence_.get();
}
#elif BUILDFLAG(IS_WIN)
HANDLE GpuFenceHandle::Peek() const {
  return is_null() ? INVALID_HANDLE_VALUE : owned_fence_.Get();
}
#endif  // BUILDFLAG(IS_POSIX)

}  // namespace gfx
