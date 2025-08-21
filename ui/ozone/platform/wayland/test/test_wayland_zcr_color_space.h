// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_WAYLAND_ZCR_COLOR_SPACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_WAYLAND_ZCR_COLOR_SPACE_H_

#include <chrome-color-management-server-protocol.h>

#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/color_space.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_client;
struct wl_resource;

namespace wl {

extern const struct zcr_color_space_v1_interface kTestZcrColorSpaceV1Impl;

class MockZcrColorManagerV1;

// Manage zcr_color_space_v1_interface
class TestZcrColorSpaceV1 : public ServerObject {
 public:
  explicit TestZcrColorSpaceV1(wl_resource* resource);

  TestZcrColorSpaceV1(const TestZcrColorSpaceV1&) = delete;
  TestZcrColorSpaceV1& operator=(const TestZcrColorSpaceV1&) = delete;

  ~TestZcrColorSpaceV1() override;

  void GetInformation(wl_client* client, wl_resource* resource);
  void Destroy(wl_client* client, wl_resource* resource);

  gfx::ColorSpace GetGfxColorSpace() const { return gfx_color_space_; }
  void SetGfxColorSpace(gfx::ColorSpace gfx_color_space);

  void SetZcrColorManager(MockZcrColorManagerV1* zcr_color_manager);

 private:
  gfx::ColorSpace gfx_color_space_;
  raw_ptr<MockZcrColorManagerV1> zcr_color_manager_ = nullptr;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_WAYLAND_ZCR_COLOR_SPACE_H_
