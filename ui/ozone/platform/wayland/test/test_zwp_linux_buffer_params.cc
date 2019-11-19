// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_zwp_linux_buffer_params.h"

#include "ui/ozone/platform/wayland/test/mock_buffer.h"
#include "ui/ozone/platform/wayland/test/mock_zwp_linux_dmabuf.h"

namespace wl {

namespace {

void Add(wl_client* client,
         wl_resource* resource,
         int32_t fd,
         uint32_t plane_idx,
         uint32_t offset,
         uint32_t stride,
         uint32_t modifier_hi,
         uint32_t modifier_lo) {
  auto* buffer_params = GetUserDataAs<TestZwpLinuxBufferParamsV1>(resource);

  buffer_params->fds_.emplace_back(fd);
  buffer_params->modifier_lo_ = modifier_lo;
  buffer_params->modifier_hi_ = modifier_hi;
}

void CreateCommon(TestZwpLinuxBufferParamsV1* buffer_params,
                  wl_client* client,
                  int32_t width,
                  int32_t height,
                  uint32_t format,
                  uint32_t flags) {
  wl_resource* buffer_resource =
      CreateResourceWithImpl<::testing::NiceMock<MockBuffer>>(
          client, &wl_buffer_interface, 1, &kMockWlBufferImpl, 0,
          std::move(buffer_params->fds_));

  buffer_params->SetBufferResource(buffer_resource);
}

void Create(wl_client* client,
            wl_resource* buffer_params_resource,
            int32_t width,
            int32_t height,
            uint32_t format,
            uint32_t flags) {
  auto* buffer_params =
      GetUserDataAs<TestZwpLinuxBufferParamsV1>(buffer_params_resource);
  CreateCommon(buffer_params, client, width, height, format, flags);
}

void CreateImmed(wl_client* client,
                 wl_resource* buffer_params_resource,
                 uint32_t buffer_id,
                 int32_t width,
                 int32_t height,
                 uint32_t format,
                 uint32_t flags) {
  auto* buffer_params =
      GetUserDataAs<TestZwpLinuxBufferParamsV1>(buffer_params_resource);
  CreateCommon(buffer_params, client, width, height, format, flags);
}

}  // namespace

const struct zwp_linux_buffer_params_v1_interface
    kTestZwpLinuxBufferParamsV1Impl = {&DestroyResource, &Add, &Create,
                                       &CreateImmed};

TestZwpLinuxBufferParamsV1::TestZwpLinuxBufferParamsV1(wl_resource* resource)
    : ServerObject(resource) {}

TestZwpLinuxBufferParamsV1::~TestZwpLinuxBufferParamsV1() {
  DCHECK(linux_dmabuf_);
  linux_dmabuf_->OnBufferParamsDestroyed(this);
}

void TestZwpLinuxBufferParamsV1::SetZwpLinuxDmabuf(
    MockZwpLinuxDmabufV1* linux_dmabuf) {
  DCHECK(!linux_dmabuf_);
  linux_dmabuf_ = linux_dmabuf;
}

void TestZwpLinuxBufferParamsV1::SetBufferResource(wl_resource* resource) {
  DCHECK(!buffer_resource_);
  buffer_resource_ = resource;
}

}  // namespace wl
