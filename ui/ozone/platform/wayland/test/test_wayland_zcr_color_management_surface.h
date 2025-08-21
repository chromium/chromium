// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_WAYLAND_ZCR_COLOR_MANAGEMENT_SURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_WAYLAND_ZCR_COLOR_MANAGEMENT_SURFACE_H_

#include <chrome-color-management-server-protocol.h>

#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/color_space.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_client;
struct wl_resource;

namespace wl {

extern const struct zcr_color_management_surface_v1_interface
    kTestZcrColorManagementSurfaceV1Impl;

class MockZcrColorManagerV1;

class TestZcrColorManagementSurfaceV1 : public ServerObject {
 public:
  explicit TestZcrColorManagementSurfaceV1(wl_resource* resource);

  TestZcrColorManagementSurfaceV1(const TestZcrColorManagementSurfaceV1&) =
      delete;
  TestZcrColorManagementSurfaceV1& operator=(
      const TestZcrColorManagementSurfaceV1&) = delete;

  ~TestZcrColorManagementSurfaceV1() override;

  void SetAlphaMode(wl_client* client,
                    wl_resource* resource,
                    uint32_t alpha_mode);
  void SetExtendedDynamicRange(wl_client* client,
                               wl_resource* resource,
                               uint32_t value);
  void SetColorSpace(wl_client* client,
                     wl_resource* resource,
                     wl_resource* color_space_resource,
                     uint32_t render_intent);
  void SetDefaultColorSpace(wl_client* client, wl_resource* resource);
  void Destroy(wl_client* client, wl_resource* resource);

  gfx::ColorSpace GetGfxColorSpace();
  void SetGfxColorSpace(gfx::ColorSpace gfx_color_space);
  void StoreZcrColorManager(MockZcrColorManagerV1* zcr_color_manager);

 private:
  gfx::ColorSpace gfx_color_space_;
  raw_ptr<MockZcrColorManagerV1> zcr_color_manager_ = nullptr;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_WAYLAND_ZCR_COLOR_MANAGEMENT_SURFACE_H_
