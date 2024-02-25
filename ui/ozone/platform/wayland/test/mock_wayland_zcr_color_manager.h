// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_WAYLAND_ZCR_COLOR_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_WAYLAND_ZCR_COLOR_MANAGER_H_

#include <chrome-color-management-server-protocol.h>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/ozone/platform/wayland/test/global_object.h"

struct wl_client;
struct wl_resource;

namespace wl {

extern const struct zcr_color_manager_v1_interface kMockZcrColorManagerV1Impl;

class TestZcrColorManagementOutputV1;
class TestZcrColorManagementSurfaceV1;
class TestZcrColorSpaceV1;

// Manage zwp_linux_buffer_params_v1
class MockZcrColorManagerV1 : public GlobalObject {
 public:
  MockZcrColorManagerV1();

  MockZcrColorManagerV1(const MockZcrColorManagerV1&) = delete;
  MockZcrColorManagerV1& operator=(const MockZcrColorManagerV1&) = delete;

  ~MockZcrColorManagerV1() override;

  MOCK_METHOD4(
      CreateColorSpaceFromIcc,
      void(wl_client* client, wl_resource* resource, uint32_t id, int32_t fd));
  MOCK_METHOD6(CreateColorSpaceFromNames,
               void(wl_client* client,
                    wl_resource* resource,
                    uint32_t id,
                    uint32_t eotf,
                    uint32_t chromaticity,
                    uint32_t whitepoint));
  MOCK_METHOD(void,
              CreateColorSpaceFromParams,
              (wl_client * client,
               wl_resource* resource,
               uint32_t id,
               uint32_t eotf,
               uint32_t primary_r_x,
               uint32_t primary_r_y,
               uint32_t primary_g_x,
               uint32_t primary_g_y,
               uint32_t primary_b_x,
               uint32_t primary_b_y,
               uint32_t whitepoint_x,
               uint32_t whitepoint_y));
  MOCK_METHOD4(GetColorManagementOutput,
               void(wl_client* client,
                    wl_resource* resource,
                    uint32_t id,
                    wl_resource* output));
  MOCK_METHOD4(GetColorManagementSurface,
               void(wl_client* client,
                    wl_resource* resource,
                    uint32_t id,
                    wl_resource* surface));
  MOCK_METHOD2(Destroy, void(wl_client* client, wl_resource* resource));

  const std::vector<raw_ptr<TestZcrColorManagementOutputV1, VectorExperimental>>
  color_management_outputs() const {
    return color_manager_outputs_;
  }

  const std::vector<
      raw_ptr<TestZcrColorManagementSurfaceV1, VectorExperimental>>
  color_management_surfaces() const {
    return color_manager_surfaces_;
  }
  void StoreZcrColorManagementOutput(TestZcrColorManagementOutputV1* params);
  void StoreZcrColorManagementSurface(TestZcrColorManagementSurfaceV1* params);
  void StoreZcrColorSpace(TestZcrColorSpaceV1* params);

  void OnZcrColorManagementOutputDestroyed(
      TestZcrColorManagementOutputV1* params);
  void OnZcrColorManagementSurfaceDestroyed(
      TestZcrColorManagementSurfaceV1* params);
  void OnZcrColorSpaceDestroyed(TestZcrColorSpaceV1* params);

 private:
  std::vector<raw_ptr<TestZcrColorManagementOutputV1, VectorExperimental>>
      color_manager_outputs_;
  std::vector<raw_ptr<TestZcrColorManagementSurfaceV1, VectorExperimental>>
      color_manager_surfaces_;
  std::vector<raw_ptr<TestZcrColorSpaceV1, VectorExperimental>>
      color_manager_color_spaces_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_WAYLAND_ZCR_COLOR_MANAGER_H_
