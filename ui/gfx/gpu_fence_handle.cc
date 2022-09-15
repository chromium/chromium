// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gpu_fence_handle.h"

#include "base/debug/alias.h"
#include "base/notreached.h"
#include "build/build_config.h"

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

namespace gfx {

GpuFenceHandle::GpuFenceHandle() = default;

GpuFenceHandle::GpuFenceHandle(GpuFenceHandle&& other) = default;

GpuFenceHandle& GpuFenceHandle::operator=(GpuFenceHandle&& other) = default;

GpuFenceHandle::~GpuFenceHandle() = default;

bool GpuFenceHandle::is_null() const {
#if BUILDFLAG(IS_POSIX)
  return !owned_fd.is_valid();
#elif BUILDFLAG(IS_FUCHSIA)
  return !owned_event.is_valid();
#elif BUILDFLAG(IS_WIN)
  return !owned_handle.IsValid();
#else
  return true;
#endif
}

GpuFenceHandle GpuFenceHandle::Clone() const {
  if (is_null())
    return GpuFenceHandle();

  gfx::GpuFenceHandle handle;
#if BUILDFLAG(IS_POSIX)
  const int duped_handle = HANDLE_EINTR(dup(owned_fd.get()));
  if (duped_handle < 0)
    return GpuFenceHandle();
  handle.owned_fd = base::ScopedFD(duped_handle);
#elif BUILDFLAG(IS_FUCHSIA)
  zx_status_t status =
      owned_event.duplicate(ZX_RIGHT_SAME_RIGHTS, &handle.owned_event);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "zx_handle_duplicate";
    return GpuFenceHandle();
  }
#elif BUILDFLAG(IS_WIN)
  const base::ProcessHandle process = ::GetCurrentProcess();
  HANDLE duplicated_handle = INVALID_HANDLE_VALUE;
  const BOOL result =
      ::DuplicateHandle(process, owned_handle.Get(), process,
                        &duplicated_handle, 0, FALSE, DUPLICATE_SAME_ACCESS);
  if (!result) {
    const DWORD last_error = ::GetLastError();
    base::debug::Alias(&last_error);
    CHECK(false);
  }
  handle.owned_handle.Set(duplicated_handle);
#else
  NOTREACHED();
#endif
  return handle;
}

}  // namespace gfx
