// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SYNCOBJ_TIMELINE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SYNCOBJ_TIMELINE_H_

#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/message_loop/message_pump_for_ui.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class DrmSyncobjIoctlWrapper;
class WaylandConnection;

class WaylandSyncobjTimeline {
 public:
  WaylandSyncobjTimeline(const WaylandSyncobjTimeline&) = delete;
  WaylandSyncobjTimeline& operator=(const WaylandSyncobjTimeline&) = delete;

  virtual ~WaylandSyncobjTimeline();

  // Increments the sync point for this syncobj timeline.
  // The sync point is initially set to zero.
  virtual void IncrementSyncPoint();
  void DecrementSyncPoint();

  wp_linux_drm_syncobj_timeline_v1* timeline() const { return timeline_.get(); }
  uint64_t sync_point() const { return sync_point_; }

 protected:
  struct DrmSyncobj;

  WaylandSyncobjTimeline(DrmSyncobjIoctlWrapper* drm,
                         std::unique_ptr<DrmSyncobj> syncobj,
                         wp_linux_drm_syncobj_timeline_v1* timeline);

  static std::pair<std::unique_ptr<DrmSyncobj>,
                   wp_linux_drm_syncobj_timeline_v1*>
  CreateHelper(const WaylandConnection* connection);

  const raw_ptr<DrmSyncobjIoctlWrapper> drm_;
  const std::unique_ptr<DrmSyncobj> syncobj_;
  const wl::Object<wp_linux_drm_syncobj_timeline_v1> timeline_;
  uint64_t sync_point_ = 0;
  bool current_sync_point_has_fence_ = false;
};

class WaylandSyncobjAcquireTimeline : public WaylandSyncobjTimeline {
 public:
  WaylandSyncobjAcquireTimeline(const WaylandSyncobjAcquireTimeline&) = delete;
  WaylandSyncobjAcquireTimeline& operator=(
      const WaylandSyncobjAcquireTimeline&) = delete;

  ~WaylandSyncobjAcquireTimeline() override;

  static std::unique_ptr<WaylandSyncobjAcquireTimeline> Create(
      const WaylandConnection* connection);

  // Imports the sync_file fd at the current sync point within this timeline
  // syncobj.
  // Should be called only after the first sync point has been set.
  bool ImportSyncFdAtCurrentSyncPoint(int sync_fd);

 private:
  WaylandSyncobjAcquireTimeline(DrmSyncobjIoctlWrapper* drm,
                                std::unique_ptr<DrmSyncobj> syncobj,
                                wp_linux_drm_syncobj_timeline_v1* timeline);
};

class WaylandSyncobjReleaseTimeline : public WaylandSyncobjTimeline,
                                      public base::MessagePumpForUI::FdWatcher {
 public:
  using FenceAvailableCallback = base::OnceCallback<void(base::ScopedFD)>;

  WaylandSyncobjReleaseTimeline(const WaylandSyncobjReleaseTimeline&) = delete;
  WaylandSyncobjReleaseTimeline& operator=(
      const WaylandSyncobjReleaseTimeline&) = delete;

  ~WaylandSyncobjReleaseTimeline() override;

  void IncrementSyncPoint() override;

  static std::unique_ptr<WaylandSyncobjReleaseTimeline> Create(
      const WaylandConnection* connection);

  // Issues a non-blocking DRM_IOCTL_SYNCOBJ_EVENTFD [1] request using
  // DRM_SYNCOBJ_WAIT_FLAGS_WAIT_AVAILABLE flag which signals an eventfd when
  // the fence is available at the current sync point, without waiting for the
  // fence to be signaled. The |callback| is invoked with the available fence.
  // Should be called only after the first sync point has been set.
  // Also, it is illegal to call this again before the provided callback has
  // been invoked.
  // [1] https://docs.kernel.org/gpu/drm-mm.html#host-side-wait-on-syncobjs
  void WaitForFenceAvailableAtCurrentSyncPoint(FenceAvailableCallback callback);

 private:
  WaylandSyncobjReleaseTimeline(DrmSyncobjIoctlWrapper* drm,
                                std::unique_ptr<DrmSyncobj> syncobj,
                                wp_linux_drm_syncobj_timeline_v1* timeline);
  base::ScopedFD ExportCurrentSyncPointToSyncFd();
  FRIEND_TEST_ALL_PREFIXES(WaylandSyncobjTimelineTest,
                           ExportCurrentSyncPointToSyncFd);
  FRIEND_TEST_ALL_PREFIXES(
      WaylandSyncobjTimelineTest,
      ExportCurrentSyncPointToSyncFd_FailOnSyncobjTransfer);
  FRIEND_TEST_ALL_PREFIXES(
      WaylandSyncobjTimelineTest,
      ExportCurrentSyncPointToSyncFd_FailOnSyncobjExportSyncFile);

  // base::MessagePumpForUI::FdWatcher:
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;
  base::MessagePumpForUI::FdWatchController controller_;
  base::ScopedFD event_fd_;
  FenceAvailableCallback fence_available_callback_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SYNCOBJ_TIMELINE_H_
