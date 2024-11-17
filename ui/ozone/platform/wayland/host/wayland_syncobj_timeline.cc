// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_syncobj_timeline.h"

#include <linux-drm-syncobj-v1-client-protocol.h>
#include <sys/eventfd.h>
#include <xf86drm.h>

#include "base/posix/eintr_wrapper.h"
#include "base/task/current_thread.h"
#include "ui/ozone/platform/wayland/host/drm_syncobj_ioctl_wrapper.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

struct WaylandSyncobjTimeline::DrmSyncobj {
  DrmSyncobj(DrmSyncobjIoctlWrapper* drm, uint32_t handle)
      : drm(drm), handle(handle) {}
  ~DrmSyncobj() { drm->SyncobjDestroy(handle); }

  static std::unique_ptr<DrmSyncobj> Create(DrmSyncobjIoctlWrapper* drm) {
    uint32_t syncobj_handle;
    if (drm->SyncobjCreate(0, &syncobj_handle)) {
      VPLOG(1) << "failed to create syncobj";
      return nullptr;
    }
    return std::make_unique<DrmSyncobj>(drm, syncobj_handle);
  }

  const raw_ptr<DrmSyncobjIoctlWrapper> drm;
  const uint32_t handle;
};

// static
std::pair<std::unique_ptr<WaylandSyncobjTimeline::DrmSyncobj>,
          wp_linux_drm_syncobj_timeline_v1*>
WaylandSyncobjTimeline::CreateHelper(const WaylandConnection* connection) {
  auto* drm = connection->buffer_manager_host()->drm_syncobj_wrapper();
  CHECK(drm);
  CHECK(connection->linux_drm_syncobj_manager_v1());

  auto syncobj = DrmSyncobj::Create(drm);
  if (!syncobj) {
    return {};
  }

  int syncobj_fd;
  if (drm->SyncobjHandleToFD(syncobj->handle, &syncobj_fd)) {
    VPLOG(1) << "Cannot export syncobj handle to fd";
    return {};
  }
  base::ScopedFD scoped_syncobj_fd(syncobj_fd);
  auto* timeline = wp_linux_drm_syncobj_manager_v1_import_timeline(
      connection->linux_drm_syncobj_manager_v1(), syncobj_fd);
  return {std::move(syncobj), timeline};
}

WaylandSyncobjTimeline::WaylandSyncobjTimeline(
    DrmSyncobjIoctlWrapper* drm,
    std::unique_ptr<DrmSyncobj> syncobj,
    wp_linux_drm_syncobj_timeline_v1* timeline)
    : drm_(drm), syncobj_(std::move(syncobj)), timeline_(timeline) {}

WaylandSyncobjTimeline::~WaylandSyncobjTimeline() = default;

void WaylandSyncobjTimeline::IncrementSyncPoint() {
  ++sync_point_;
  current_sync_point_has_fence_ = false;
}

void WaylandSyncobjTimeline::DecrementSyncPoint() {
  if (sync_point_ > 0) {
    --sync_point_;
  }
}

WaylandSyncobjAcquireTimeline::WaylandSyncobjAcquireTimeline(
    DrmSyncobjIoctlWrapper* drm,
    std::unique_ptr<DrmSyncobj> syncobj,
    wp_linux_drm_syncobj_timeline_v1* timeline)
    : WaylandSyncobjTimeline(drm, std::move(syncobj), timeline) {}

WaylandSyncobjAcquireTimeline::~WaylandSyncobjAcquireTimeline() = default;

// static
std::unique_ptr<WaylandSyncobjAcquireTimeline>
WaylandSyncobjAcquireTimeline::Create(const WaylandConnection* connection) {
  auto pair = CreateHelper(connection);
  if (pair == std::pair<std::unique_ptr<DrmSyncobj>,
                        wp_linux_drm_syncobj_timeline_v1*>()) {
    return nullptr;
  }

  return base::WrapUnique<WaylandSyncobjAcquireTimeline>(
      new WaylandSyncobjAcquireTimeline(
          connection->buffer_manager_host()->drm_syncobj_wrapper(),
          std::move(pair.first), pair.second));
}

bool WaylandSyncobjAcquireTimeline::ImportSyncFdAtCurrentSyncPoint(
    int sync_fd) {
  DCHECK(!current_sync_point_has_fence_);
  // It is illegal to call this before the first sync point.
  CHECK(sync_point_ > 0);
  // Create a binary syncobj wrapping the fence from the sync_fd.
  auto temp = DrmSyncobj::Create(drm_);
  if (drm_->SyncobjImportSyncFile(temp->handle, sync_fd)) {
    DVPLOG(3) << "Unable to import sync file to syncobj";
    return false;
  }

  // Now transfer the fence from the binary syncobj to this syncobj at the
  // specified sync point.
  // Sync point 0 is used for binary syncobj.
  if (drm_->SyncobjTransfer(syncobj_->handle, sync_point_, temp->handle, 0,
                            0)) {
    DVPLOG(3) << "Unable to transfer syncobj";
    return false;
  }
  current_sync_point_has_fence_ = true;
  return true;
}

// static
std::unique_ptr<WaylandSyncobjReleaseTimeline>
WaylandSyncobjReleaseTimeline::Create(const WaylandConnection* connection) {
  auto pair = CreateHelper(connection);
  if (pair == std::pair<std::unique_ptr<DrmSyncobj>,
                        wp_linux_drm_syncobj_timeline_v1*>()) {
    return nullptr;
  }

  return base::WrapUnique<WaylandSyncobjReleaseTimeline>(
      new WaylandSyncobjReleaseTimeline(
          connection->buffer_manager_host()->drm_syncobj_wrapper(),
          std::move(pair.first), pair.second));
}

WaylandSyncobjReleaseTimeline::WaylandSyncobjReleaseTimeline(
    DrmSyncobjIoctlWrapper* drm,
    std::unique_ptr<DrmSyncobj> syncobj,
    wp_linux_drm_syncobj_timeline_v1* timeline)
    : WaylandSyncobjTimeline(drm, std::move(syncobj), timeline),
      controller_(FROM_HERE) {}

WaylandSyncobjReleaseTimeline::~WaylandSyncobjReleaseTimeline() = default;

void WaylandSyncobjReleaseTimeline::IncrementSyncPoint() {
  // Sync point shouldn't be incremented if we are still waiting for fence to be
  // available at the current sync point.
  CHECK(fence_available_callback_.is_null());
  WaylandSyncobjTimeline::IncrementSyncPoint();
}

base::ScopedFD WaylandSyncobjReleaseTimeline::ExportCurrentSyncPointToSyncFd() {
  // It is illegal to call this before the first sync point.
  CHECK(sync_point_ > 0);
  int sync_fd;
  // Create a binary syncobj to wrap the fence at the given sync point.
  auto temp = DrmSyncobj::Create(drm_);
  // Destination sync point 0 is used for binary syncobj.
  if (drm_->SyncobjTransfer(temp->handle, 0, syncobj_->handle, sync_point_,
                            0)) {
    DVPLOG(3) << "Unable to transfer timeline syncobj=" << syncobj_->handle
              << " at sync point=" << sync_point_
              << " to binary syncobj=" << temp->handle;
    return base::ScopedFD();
  }

  // Export a sync fd from the binary syncobj.
  if (drm_->SyncobjExportSyncFile(temp->handle, &sync_fd)) {
    DVPLOG(3) << "Unable to export syncobj to sync file";
    return base::ScopedFD();
  }
  return base::ScopedFD(sync_fd);
}

void WaylandSyncobjReleaseTimeline::OnFileCanReadWithoutBlocking(int fd) {
  DVLOG(3) << "Fence available for syncobj=" << syncobj_->handle
           << " sync point=" << sync_point_;
  PCHECK(fd == event_fd_.get());
  current_sync_point_has_fence_ = true;
  uint64_t value;
  ssize_t n = HANDLE_EINTR(read(event_fd_.get(), &value, sizeof(value)));
  DPCHECK(n == sizeof(value));
  DCHECK(1 == value);
  CHECK(!fence_available_callback_.is_null());
  std::move(fence_available_callback_).Run(ExportCurrentSyncPointToSyncFd());
}

void WaylandSyncobjReleaseTimeline::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED() << "Unexpected notification about fd being ready for writing";
}

void WaylandSyncobjReleaseTimeline::WaitForFenceAvailableAtCurrentSyncPoint(
    FenceAvailableCallback callback) {
  DVLOG(3) << __func__ << " syncobj=" << syncobj_->handle
           << " sync point=" << sync_point_;
  // It is illegal to call this before the first sync point.
  CHECK(sync_point_ > 0);

  if (current_sync_point_has_fence_) {
    std::move(callback).Run(ExportCurrentSyncPointToSyncFd());
    return;
  }
  // It is illegal to call this function again before the callback from the
  // previous call is run.
  CHECK(fence_available_callback_.is_null());
  fence_available_callback_ = std::move(callback);
  if (!event_fd_.is_valid()) {
    event_fd_.reset(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC));
    base::CurrentUIThread::Get()->WatchFileDescriptor(
        event_fd_.get(), true, base::MessagePumpForUI::WATCH_READ, &controller_,
        this);
  }
  if (drm_->SyncobjEventfd(syncobj_->handle, sync_point_, event_fd_.get(),
                           DRM_SYNCOBJ_WAIT_FLAGS_WAIT_AVAILABLE)) {
    VPLOG(1) << "unable to set fd for syncobj fence available event";
    // Attempt to get fence at current sync point in any case and invoke the
    // callback to ensure no graphics freeze occurs if setting the event fd
    // fails for any reason.
    std::move(fence_available_callback_).Run(ExportCurrentSyncPointToSyncFd());
  }
}

}  // namespace ui
