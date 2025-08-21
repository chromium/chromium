// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_WAYLAND_ZCR_COLOR_SPACE_CREATOR_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_WAYLAND_ZCR_COLOR_SPACE_CREATOR_H_

#include <chrome-color-management-server-protocol.h>

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_resource;

namespace wl {

class TestZcrColorSpaceCreatorV1 : public ServerObject {
 public:
  explicit TestZcrColorSpaceCreatorV1(wl_resource* resource);

  TestZcrColorSpaceCreatorV1(const TestZcrColorSpaceCreatorV1&) = delete;
  TestZcrColorSpaceCreatorV1& operator=(const TestZcrColorSpaceCreatorV1&) =
      delete;

  ~TestZcrColorSpaceCreatorV1() override;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_WAYLAND_ZCR_COLOR_SPACE_CREATOR_H_
