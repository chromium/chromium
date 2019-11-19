// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_zwp_linux_dmabuf.h"

#include <linux-dmabuf-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>

#include "ui/ozone/platform/wayland/test/mock_buffer.h"
#include "ui/ozone/platform/wayland/test/test_zwp_linux_buffer_params.h"

namespace wl {

namespace {

constexpr uint32_t kLinuxDmabufVersion = 1;

void CreateParams(wl_client* client, wl_resource* resource, uint32_t id) {
  wl_resource* params_resource =
      CreateResourceWithImpl<TestZwpLinuxBufferParamsV1>(
          client, &zwp_linux_buffer_params_v1_interface,
          wl_resource_get_version(resource), &kTestZwpLinuxBufferParamsV1Impl,
          id);

  auto* zwp_linux_dmabuf = GetUserDataAs<MockZwpLinuxDmabufV1>(resource);
  auto* buffer_params =
      GetUserDataAs<TestZwpLinuxBufferParamsV1>(params_resource);

  DCHECK(buffer_params);
  zwp_linux_dmabuf->StoreBufferParams(buffer_params);
  buffer_params->SetZwpLinuxDmabuf(zwp_linux_dmabuf);
  zwp_linux_dmabuf->CreateParams(client, resource, id);
}

}  // namespace

const struct zwp_linux_dmabuf_v1_interface kMockZwpLinuxDmabufV1Impl = {
    &DestroyResource,  // destroy
    &CreateParams,     // create_params
};

MockZwpLinuxDmabufV1::MockZwpLinuxDmabufV1()
    : GlobalObject(&zwp_linux_dmabuf_v1_interface,
                   &kMockZwpLinuxDmabufV1Impl,
                   kLinuxDmabufVersion) {}

MockZwpLinuxDmabufV1::~MockZwpLinuxDmabufV1() {
  DCHECK(buffer_params_.empty());
}

void MockZwpLinuxDmabufV1::StoreBufferParams(
    TestZwpLinuxBufferParamsV1* params) {
  buffer_params_.push_back(params);
}

void MockZwpLinuxDmabufV1::OnBufferParamsDestroyed(
    TestZwpLinuxBufferParamsV1* params) {
  auto it = std::find(buffer_params_.begin(), buffer_params_.end(), params);
  DCHECK(it != buffer_params_.end());
  buffer_params_.erase(it);
}

}  // namespace wl
