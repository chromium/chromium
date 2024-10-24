// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_wp_linux_drm_syncobj.h"

#include <linux-drm-syncobj-v1-server-protocol.h>

#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

const struct wp_linux_drm_syncobj_timeline_v1_interface
    kTestLinuxDrmSyncobjTimelineImpl = {.destroy = DestroyResource};

namespace {

constexpr uint32_t kLinuxDrmSyncobjVersion = 1;
constexpr uint32_t kTestLinuxDrmSyncobjTimelineVersion = 1;

void GetSurface(struct wl_client* client,
                struct wl_resource* resource,
                uint32_t id,
                struct wl_resource* surface) {
  // TODO(crbug.com/367623923) Add support for creating test surface along with
  // implementation and unit tests.
}

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
