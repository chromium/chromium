// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_wayland_zcr_color_management_output.h"

#include "base/notreached.h"
#include "ui/gfx/color_space.h"
#include "ui/ozone/platform/wayland/test/mock_wayland_zcr_color_manager.h"
#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/platform/wayland/test/test_wayland_zcr_color_space.h"

namespace wl {

namespace {

void GetColorSpace(wl_client* client, wl_resource* resource, uint32_t id) {
  wl_resource* color_space_resource =
      CreateResourceWithImpl<TestZcrColorSpaceV1>(
          client, &zcr_color_space_v1_interface, 1, &kTestZcrColorSpaceV1Impl,
          id);
  auto* color_management_output =
      GetUserDataAs<TestZcrColorManagementOutputV1>(resource);
  auto* zcr_color_space =
      GetUserDataAs<TestZcrColorSpaceV1>(color_space_resource);
  zcr_color_space->SetGfxColorSpace(
      color_management_output->GetGfxColorSpace());
  color_management_output->StoreZcrColorSpace(zcr_color_space);
}

}  // namespace

const struct zcr_color_management_output_v1_interface
    kTestZcrColorManagementOutputV1Impl = {&GetColorSpace, &DestroyResource};

TestZcrColorManagementOutputV1::TestZcrColorManagementOutputV1(
    wl_resource* resource)
    : ServerObject(resource) {}

TestZcrColorManagementOutputV1::~TestZcrColorManagementOutputV1() {
  DCHECK(zcr_color_manager_);
  zcr_color_manager_->OnZcrColorManagementOutputDestroyed(this);
}

void TestZcrColorManagementOutputV1::SetGfxColorSpace(
    gfx::ColorSpace gfx_color_space) {
  gfx_color_space_ = gfx_color_space;
}

void TestZcrColorManagementOutputV1::StoreZcrColorManager(
    MockZcrColorManagerV1* zcr_color_manager) {
  zcr_color_manager_ = zcr_color_manager;
}

void TestZcrColorManagementOutputV1::StoreZcrColorSpace(
    TestZcrColorSpaceV1* zcr_color_space) {
  zcr_color_space_ = zcr_color_space;
}
}  // namespace wl