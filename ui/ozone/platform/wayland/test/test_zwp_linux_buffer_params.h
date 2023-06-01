// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZWP_LINUX_BUFFER_PARAMS_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZWP_LINUX_BUFFER_PARAMS_H_

#include <linux-dmabuf-unstable-v1-server-protocol.h>

#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_client;
struct wl_resource;

namespace wl {

extern const struct zwp_linux_buffer_params_v1_interface
    kTestZwpLinuxBufferParamsV1Impl;

class MockZwpLinuxDmabufV1;

// Manage zwp_linux_buffer_params_v1
class TestZwpLinuxBufferParamsV1 : public ServerObject {
 public:
  explicit TestZwpLinuxBufferParamsV1(wl_resource* resource);

  TestZwpLinuxBufferParamsV1(const TestZwpLinuxBufferParamsV1&) = delete;
  TestZwpLinuxBufferParamsV1& operator=(const TestZwpLinuxBufferParamsV1&) =
      delete;

  ~TestZwpLinuxBufferParamsV1() override;

  void Destroy(wl_client* client, wl_resource* resource);
  void Add(wl_client* client,
           wl_resource* resource,
           int32_t fd,
           uint32_t plane_idx,
           uint32_t offset,
           uint32_t stride,
           uint32_t modifier_hi,
           uint32_t modifier_lo);
  void Create(wl_client* client,
              wl_resource* resource,
              int32_t width,
              int32_t height,
              uint32_t format,
              uint32_t flags);
  void CreateImmed(wl_client* client,
                   wl_resource* resource,
                   uint32_t buffer_id,
                   int32_t width,
                   int32_t height,
                   uint32_t format,
                   uint32_t flags);

  wl_resource* buffer_resource() const { return buffer_resource_; }

  void SetZwpLinuxDmabuf(MockZwpLinuxDmabufV1* linux_dmabuf);

  void SetBufferResource(wl_resource* resource);

  std::vector<base::ScopedFD> fds_;

  uint32_t modifier_hi_ = 0;
  uint32_t modifier_lo_ = 0;

 private:
  // Non-owned pointer to the linux dmabuf object, which created this params
  // resource and holds a pointer to it. On destruction, must notify it about
  // going out of scope.
  raw_ptr<MockZwpLinuxDmabufV1> linux_dmabuf_ = nullptr;

  // A buffer resource, which is created on Create or CreateImmed call. Can be
  // null if not created/failed to be created.
  raw_ptr<wl_resource, DanglingUntriaged> buffer_resource_ = nullptr;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZWP_LINUX_BUFFER_PARAMS_H_
