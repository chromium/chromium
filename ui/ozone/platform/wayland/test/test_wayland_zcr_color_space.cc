// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_wayland_zcr_color_space.h"

#include "ui/ozone/platform/wayland/test/mock_wayland_zcr_color_manager.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

namespace {

void GetInformation(wl_client* client, wl_resource* resource) {
  auto* zcr_color_space = GetUserDataAs<TestZcrColorSpaceV1>(resource);
  DCHECK(zcr_color_space);
}

}  // namespace

const struct zcr_color_space_v1_interface kTestZcrColorSpaceV1Impl = {
    &GetInformation, &DestroyResource};

TestZcrColorSpaceV1::TestZcrColorSpaceV1(wl_resource* resource)
    : ServerObject(resource) {}

TestZcrColorSpaceV1::~TestZcrColorSpaceV1() {
  if (zcr_color_manager_) {
    zcr_color_manager_->OnZcrColorSpaceDestroyed(this);
  }
}

void TestZcrColorSpaceV1::SetGfxColorSpace(gfx::ColorSpace gfx_color_space) {
  gfx_color_space_ = gfx_color_space;
}

void TestZcrColorSpaceV1::SetZcrColorManager(
    MockZcrColorManagerV1* zcr_color_manager) {
  zcr_color_manager_ = zcr_color_manager;
}

}  // namespace wl