// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gpu_fence.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include <sync/sync.h>
#endif

namespace gfx {

GpuFence::GpuFence(GpuFenceHandle fence_handle)
    : fence_handle_(std::move(fence_handle)) {}

GpuFence::~GpuFence() = default;

GpuFence::GpuFence(GpuFence&& other) = default;

GpuFence& GpuFence::operator=(GpuFence&& other) = default;

const GpuFenceHandle& GpuFence::GetGpuFenceHandle() const {
  return fence_handle_;
}

ClientGpuFence GpuFence::AsClientGpuFence() {
  return reinterpret_cast<ClientGpuFence>(this);
}

// static
GpuFence* GpuFence::FromClientGpuFence(ClientGpuFence gpu_fence) {
  return reinterpret_cast<GpuFence*>(gpu_fence);
}

void GpuFence::Wait() {
  if (fence_handle_.is_null()) {
    return;
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  static const int kInfiniteSyncWaitTimeout = -1;
  DCHECK_GE(fence_handle_.Peek(), 0);
  if (sync_wait(fence_handle_.Peek(), kInfiniteSyncWaitTimeout) < 0) {
    LOG(FATAL) << "Failed while waiting for gpu fence fd";
  }
#else
  NOTREACHED_IN_MIGRATION();
#endif
}

// static
GpuFence::FenceStatus GpuFence::GetStatusChangeTime(int fd,
                                                    base::TimeTicks* time) {
  DCHECK_NE(fd, -1);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  auto info =
      std::unique_ptr<sync_fence_info_data, void (*)(sync_fence_info_data*)>{
          sync_fence_info(fd), sync_fence_info_free};
  if (!info) {
    LOG(ERROR) << "sync_fence_info returned null for fd : " << fd;
    return FenceStatus::kInvalid;
  }

  // Not signalled yet.
  if (info->status != 1) {
    return FenceStatus::kNotSignaled;
  }

  uint64_t timestamp_ns = 0u;
  struct sync_pt_info* pt_info = nullptr;
  while ((pt_info = sync_pt_info(info.get(), pt_info)))
    timestamp_ns = std::max(timestamp_ns, pt_info->timestamp_ns);

  if (timestamp_ns == 0u) {
    LOG(ERROR) << "No timestamp provided from sync_pt_info for fd : " << fd;
    return FenceStatus::kInvalid;
  }
  *time = base::TimeTicks() + base::Nanoseconds(timestamp_ns);
  return FenceStatus::kSignaled;
#endif
  NOTREACHED_IN_MIGRATION();
  return FenceStatus::kInvalid;
}

base::TimeTicks GpuFence::GetMaxTimestamp() const {
  base::TimeTicks timestamp;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  FenceStatus status = GetStatusChangeTime(fence_handle_.Peek(), &timestamp);
  DCHECK_EQ(status, FenceStatus::kSignaled);
  return timestamp;
#endif
  NOTREACHED_IN_MIGRATION();
  return timestamp;
}

}  // namespace gfx
