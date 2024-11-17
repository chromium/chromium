// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_WP_LINUX_DRM_SYNCOBJ_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_WP_LINUX_DRM_SYNCOBJ_H_

#include <map>

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/ozone/platform/wayland/test/global_object.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

class TestWpLinuxDrmSyncobjManagerV1;

class MockLinuxDrmSyncobjSurface : public ServerObject {
 public:
  MockLinuxDrmSyncobjSurface(wl_resource* resource, wl_resource* surface);
  ~MockLinuxDrmSyncobjSurface() override;

  MOCK_METHOD(void,
              SetAcquirePoint,
              (int timeline_fd, uint64_t acquire_point),
              ());
  MOCK_METHOD(void,
              SetReleasePoint,
              (int timeline_fd, uint64_t acquire_point),
              ());
};

class TestLinuxDrmSyncobjTimeline : public ServerObject {
 public:
  TestLinuxDrmSyncobjTimeline(wl_resource* resource,
                              TestWpLinuxDrmSyncobjManagerV1* manager,
                              int fd);
  ~TestLinuxDrmSyncobjTimeline() override;

  int fd() const { return fd_; }

 private:
  raw_ptr<TestWpLinuxDrmSyncobjManagerV1> manager_;
  int fd_;
};

class TestWpLinuxDrmSyncobjManagerV1 : public GlobalObject {
 public:
  TestWpLinuxDrmSyncobjManagerV1();
  ~TestWpLinuxDrmSyncobjManagerV1() override;
  TestWpLinuxDrmSyncobjManagerV1(const TestWpLinuxDrmSyncobjManagerV1&) =
      delete;
  TestWpLinuxDrmSyncobjManagerV1& operator=(
      const TestWpLinuxDrmSyncobjManagerV1&) = delete;

  void OnTimelineDestroyed(TestLinuxDrmSyncobjTimeline* timeline);

 private:
  std::map<int, raw_ptr<wl_resource>> timelines_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_WP_LINUX_DRM_SYNCOBJ_H_
