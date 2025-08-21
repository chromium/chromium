// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_wayland_zcr_color_management_surface.h"

#include "base/notreached.h"
#include "ui/gfx/color_space.h"
#include "ui/ozone/platform/wayland/test/mock_wayland_zcr_color_manager.h"
#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/platform/wayland/test/test_wayland_zcr_color_space.h"

namespace wl {

namespace {

void SetAlphaMode(wl_client* client,
                  wl_resource* resource,
                  uint32_t alpha_mode) {
  auto* color_management_surface =
      GetUserDataAs<TestZcrColorManagementSurfaceV1>(resource);
  DCHECK(color_management_surface);
}

void SetExtendedDynamicRange(wl_client* client,
                             wl_resource* resource,
                             uint32_t value) {
  auto* color_management_surface =
      GetUserDataAs<TestZcrColorManagementSurfaceV1>(resource);
  DCHECK(color_management_surface);
}

void SetColorSpace(wl_client* client,
                   wl_resource* resource,
                   wl_resource* color_space_resource,
                   uint32_t render_intent) {
  auto* color_management_surface =
      GetUserDataAs<TestZcrColorManagementSurfaceV1>(resource);
  DCHECK(color_space_resource);
  auto* zcr_color_space =
      GetUserDataAs<TestZcrColorSpaceV1>(color_space_resource);
  color_management_surface->SetGfxColorSpace(
      zcr_color_space->GetGfxColorSpace());
}

void SetDefaultColorSpace(wl_client* client, wl_resource* resource) {
  auto* color_management_surface =
      GetUserDataAs<TestZcrColorManagementSurfaceV1>(resource);
  color_management_surface->SetGfxColorSpace(gfx::ColorSpace::CreateSRGB());
}

}  // namespace

const struct zcr_color_management_surface_v1_interface
    kTestZcrColorManagementSurfaceV1Impl = {
        &SetAlphaMode, &SetExtendedDynamicRange, &SetColorSpace,
        &SetDefaultColorSpace, &DestroyResource};

TestZcrColorManagementSurfaceV1::TestZcrColorManagementSurfaceV1(
    wl_resource* resource)
    : ServerObject(resource) {}

TestZcrColorManagementSurfaceV1::~TestZcrColorManagementSurfaceV1() {
  DCHECK(zcr_color_manager_);
  zcr_color_manager_->OnZcrColorManagementSurfaceDestroyed(this);
}

gfx::ColorSpace TestZcrColorManagementSurfaceV1::GetGfxColorSpace() {
  return gfx_color_space_;
}

void TestZcrColorManagementSurfaceV1::SetGfxColorSpace(
    gfx::ColorSpace gfx_color_space) {
  gfx_color_space_ = gfx_color_space;
}

void TestZcrColorManagementSurfaceV1::StoreZcrColorManager(
    MockZcrColorManagerV1* zcr_color_manager) {
  zcr_color_manager_ = zcr_color_manager;
}

}  // namespace wl