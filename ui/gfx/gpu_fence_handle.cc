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
#include "base/memory/scoped_refptr.h"
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
std::atomic<uint32_t> g_num_clones_counter{0};

bool IsEnabledUseSmartRefForGPUFenceHandle() {
  static bool is_enabled =
      base::FeatureList::IsEnabled(features::kUseSmartRefForGPUFenceHandle);
  return is_enabled;
}  // namespace

gfx::GpuFenceHandle::ScopedPlatformFence PlatformDuplicate(
    const gfx::GpuFenceHandle::ScopedPlatformFence& scoped_fence) {
  g_num_clones_counter++;
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
  NOTREACHED_IN_MIGRATION();
#endif
}

}  // namespace

namespace gfx {

GpuFenceHandle::GpuFenceHandle() = default;

GpuFenceHandle::GpuFenceHandle(GpuFenceHandle&& other)
    : smart_fence_(std::move(other.smart_fence_)) {}

GpuFenceHandle& GpuFenceHandle::operator=(GpuFenceHandle&& other) {
  if (this != &other) {
    smart_fence_ = std::move(other.smart_fence_);
  }
  return *this;
}

void GpuFenceHandle::Reset() {
  smart_fence_.reset();
}

GpuFenceHandle::~GpuFenceHandle() = default;

bool GpuFenceHandle::is_null() const {
  if (!smart_fence_.get()) {
    return true;
  }

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  return !smart_fence_.get()->scoped_fence_.is_valid();
#elif BUILDFLAG(IS_WIN)
  return !smart_fence_.get()->scoped_fence_.IsValid();
#else
  return true;
#endif
}

GpuFenceHandle::RefCountedScopedFence::RefCountedScopedFence(
    ScopedPlatformFence scoped_fence)
    : scoped_fence_(std::move(scoped_fence)) {}

GpuFenceHandle::RefCountedScopedFence::~RefCountedScopedFence() = default;

#if BUILDFLAG(IS_POSIX)
int GpuFenceHandle::Peek() const {
  return is_null() ? base::ScopedFD().get()
                   : smart_fence_.get()->scoped_fence_.get();
}
#elif BUILDFLAG(IS_WIN)
HANDLE GpuFenceHandle::Peek() const {
  return is_null() ? INVALID_HANDLE_VALUE
                   : smart_fence_.get()->scoped_fence_.Get();
}
#endif

void GpuFenceHandle::Adopt(ScopedPlatformFence scoped_fence) {
  if (scoped_fence.is_valid()) {
    smart_fence_ =
        base::MakeRefCounted<RefCountedScopedFence>(std::move(scoped_fence));
  } else {
    Reset();
  }
}

GpuFenceHandle::ScopedPlatformFence GpuFenceHandle::Release() {
  if (is_null()) {
    return ScopedPlatformFence();
  }

  GpuFenceHandle::ScopedPlatformFence return_fence;

  if (smart_fence_->HasOneRef()) {
    // Sole owner of this FD. Non dup optimization below.
    return_fence = std::move(smart_fence_->scoped_fence_);
  } else {
    DCHECK(IsEnabledUseSmartRefForGPUFenceHandle());
    return_fence = PlatformDuplicate(smart_fence_->scoped_fence_);
  }

  Reset();
  return return_fence;
}

// static
uint32_t GpuFenceHandle::GetAndClearNumberOfClones() {
  return g_num_clones_counter.exchange(0);
}

GpuFenceHandle GpuFenceHandle::Clone() const {
  if (is_null()) {
    return GpuFenceHandle();
  }

  gfx::GpuFenceHandle handle;

  if (IsEnabledUseSmartRefForGPUFenceHandle()) {
    handle.smart_fence_ = this->smart_fence_;
  } else {
    handle.Adopt(PlatformDuplicate(smart_fence_->scoped_fence_));
  }
  return handle;
}

}  // namespace gfx
