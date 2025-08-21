// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_WAYLAND_ZCR_COLOR_MANAGEMENT_OUTPUT_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_WAYLAND_ZCR_COLOR_MANAGEMENT_OUTPUT_H_

#include <chrome-color-management-server-protocol.h>

#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/color_space.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_client;
struct wl_resource;

namespace wl {

extern const struct zcr_color_management_output_v1_interface
    kTestZcrColorManagementOutputV1Impl;

class MockZcrColorManagerV1;
class TestZcrColorSpaceV1;

class TestZcrColorManagementOutputV1 : public ServerObject {
 public:
  explicit TestZcrColorManagementOutputV1(wl_resource* resource);

  TestZcrColorManagementOutputV1(const TestZcrColorManagementOutputV1&) =
      delete;
  TestZcrColorManagementOutputV1& operator=(
      const TestZcrColorManagementOutputV1&) = delete;

  ~TestZcrColorManagementOutputV1() override;

  void GetColorSpace(wl_client* client, wl_resource* resource, uint32_t id);
  void Destroy(wl_client* client, wl_resource* resource);

  gfx::ColorSpace GetGfxColorSpace() const { return gfx_color_space_; }
  void SetGfxColorSpace(gfx::ColorSpace gfx_color_space);
  void StoreZcrColorManager(MockZcrColorManagerV1* zcr_color_manager);
  TestZcrColorSpaceV1* GetZcrColorSpace() const { return zcr_color_space_; }
  void StoreZcrColorSpace(TestZcrColorSpaceV1* zcr_color_space);

 private:
  gfx::ColorSpace gfx_color_space_;
  raw_ptr<MockZcrColorManagerV1> zcr_color_manager_ = nullptr;
  raw_ptr<TestZcrColorSpaceV1> zcr_color_space_ = nullptr;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_WAYLAND_ZCR_COLOR_MANAGEMENT_OUTPUT_H_
