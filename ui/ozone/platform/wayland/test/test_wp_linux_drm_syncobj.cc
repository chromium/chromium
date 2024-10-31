// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_wp_linux_drm_syncobj.h"

#include <linux-drm-syncobj-v1-server-protocol.h>

#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

namespace {

constexpr uint32_t kLinuxDrmSyncobjVersion = 1;
constexpr uint32_t kTestLinuxDrmSyncobjTimelineVersion = 1;
constexpr uint32_t kMockLinuxDrmSyncobjSurfaceVersion = 1;

uint64_t ToU64SyncPoint(uint32_t point_hi, uint32_t point_lo) {
  return (static_cast<uint64_t>(point_hi) << 32) |
         static_cast<uint64_t>(point_lo);
}

void SetAcquirePoint(struct wl_client* client,
                     struct wl_resource* resource,
                     struct wl_resource* timeline,
                     uint32_t point_hi,
                     uint32_t point_lo) {
  auto* timeline_resource =
      GetUserDataAs<TestLinuxDrmSyncobjTimeline>(timeline);
  GetUserDataAs<MockLinuxDrmSyncobjSurface>(resource)->SetAcquirePoint(
      timeline_resource->fd(), ToU64SyncPoint(point_hi, point_lo));
}

void SetReleasePoint(struct wl_client* client,
                     struct wl_resource* resource,
                     struct wl_resource* timeline,
                     uint32_t point_hi,
                     uint32_t point_lo) {
  auto* timeline_resource =
      GetUserDataAs<TestLinuxDrmSyncobjTimeline>(timeline);
  GetUserDataAs<MockLinuxDrmSyncobjSurface>(resource)->SetReleasePoint(
      timeline_resource->fd(), ToU64SyncPoint(point_hi, point_lo));
}

const struct wp_linux_drm_syncobj_surface_v1_interface
    kMockLinuxDrmSyncobjSurfaceImpl = {.destroy = DestroyResource,
                                       .set_acquire_point = SetAcquirePoint,
                                       .set_release_point = SetReleasePoint};

void GetSurface(struct wl_client* client,
                struct wl_resource* resource,
                uint32_t id,
                struct wl_resource* surface) {
  CreateResourceWithImpl<MockLinuxDrmSyncobjSurface>(
      client, &wp_linux_drm_syncobj_surface_v1_interface,
      kMockLinuxDrmSyncobjSurfaceVersion, &kMockLinuxDrmSyncobjSurfaceImpl, id,
      surface);
}

const struct wp_linux_drm_syncobj_timeline_v1_interface
    kTestLinuxDrmSyncobjTimelineImpl = {.destroy = DestroyResource};

void ImportTimeline(struct wl_client* client,
                    struct wl_resource* resource,
                    uint32_t id,
                    int32_t fd) {
  CreateResourceWithImpl<TestLinuxDrmSyncobjTimeline>(
      client, &wp_linux_drm_syncobj_timeline_v1_interface,
      kTestLinuxDrmSyncobjTimelineVersion, &kTestLinuxDrmSyncobjTimelineImpl,
      id, GetUserDataAs<TestWpLinuxDrmSyncobjManagerV1>(resource), fd);
}

}  // namespace

MockLinuxDrmSyncobjSurface::MockLinuxDrmSyncobjSurface(wl_resource* resource,
                                                       wl_resource* surface)
    : ServerObject(resource) {
  GetUserDataAs<MockSurface>(surface)->set_linux_drm_syncobj_surface(this);
}

MockLinuxDrmSyncobjSurface::~MockLinuxDrmSyncobjSurface() = default;

TestLinuxDrmSyncobjTimeline::TestLinuxDrmSyncobjTimeline(
    wl_resource* resource,
    TestWpLinuxDrmSyncobjManagerV1* manager,
    int fd)
    : ServerObject(resource), manager_(manager), fd_(fd) {}

TestLinuxDrmSyncobjTimeline::~TestLinuxDrmSyncobjTimeline() {
  if (manager_) {
    manager_->OnTimelineDestroyed(this);
  }
}

const struct wp_linux_drm_syncobj_manager_v1_interface
    kTestLinuxDrmSyncobjManagerImpl = {
        .destroy = DestroyResource,
        .get_surface = GetSurface,
        .import_timeline = ImportTimeline,
};

TestWpLinuxDrmSyncobjManagerV1::TestWpLinuxDrmSyncobjManagerV1()
    : GlobalObject(&wp_linux_drm_syncobj_manager_v1_interface,
                   &kTestLinuxDrmSyncobjManagerImpl,
                   kLinuxDrmSyncobjVersion) {}

TestWpLinuxDrmSyncobjManagerV1::~TestWpLinuxDrmSyncobjManagerV1() = default;

void TestWpLinuxDrmSyncobjManagerV1::OnTimelineDestroyed(
    TestLinuxDrmSyncobjTimeline* timeline) {
  std::erase_if(timelines_, [timeline](const auto& pair) {
    return pair.second == timeline->resource();
  });
}

}  // namespace wl
