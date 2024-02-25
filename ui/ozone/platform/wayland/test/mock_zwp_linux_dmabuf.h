// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZWP_LINUX_DMABUF_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZWP_LINUX_DMABUF_H_

#include <linux-dmabuf-unstable-v1-server-protocol.h>

#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/ozone/platform/wayland/test/global_object.h"

struct wl_client;
struct wl_resource;

namespace wl {

extern const struct zwp_linux_dmabuf_v1_interface kMockZwpLinuxDmabufV1Impl;

class TestZwpLinuxBufferParamsV1;

// Manage zwp_linux_dmabuf_v1 object.
class MockZwpLinuxDmabufV1 : public GlobalObject {
 public:
  MockZwpLinuxDmabufV1();

  MockZwpLinuxDmabufV1(const MockZwpLinuxDmabufV1&) = delete;
  MockZwpLinuxDmabufV1& operator=(const MockZwpLinuxDmabufV1&) = delete;

  ~MockZwpLinuxDmabufV1() override;

  MOCK_METHOD2(Destroy, void(wl_client* client, wl_resource* resource));
  MOCK_METHOD3(CreateParams,
               void(wl_client* client,
                    wl_resource* resource,
                    uint32_t params_id));

  const std::vector<raw_ptr<TestZwpLinuxBufferParamsV1, VectorExperimental>>&
  buffer_params() const {
    return buffer_params_;
  }

  void StoreBufferParams(TestZwpLinuxBufferParamsV1* params);

  void OnBufferParamsDestroyed(TestZwpLinuxBufferParamsV1* params);

 private:
  std::vector<raw_ptr<TestZwpLinuxBufferParamsV1, VectorExperimental>>
      buffer_params_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_ZWP_LINUX_DMABUF_H_
