// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_wayland_zcr_color_manager.h"

#include <chrome-color-management-server-protocol.h>

#include <cstdint>
#include <iterator>

#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "ui/base/wayland/color_manager_util.h"
#include "ui/gfx/color_space.h"
#include "ui/ozone/platform/wayland/test/global_object.h"
#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/platform/wayland/test/test_wayland_zcr_color_management_output.h"
#include "ui/ozone/platform/wayland/test/test_wayland_zcr_color_management_surface.h"
#include "ui/ozone/platform/wayland/test/test_wayland_zcr_color_space.h"
#include "ui/ozone/platform/wayland/test/test_wayland_zcr_color_space_creator.h"

namespace wl {

namespace {

constexpr uint32_t kZcrColorManagerVersion = 1;

void CreateColorSpaceFromIcc(wl_client* client,
                             wl_resource* resource,
                             uint32_t id,
                             int32_t fd) {
  auto* zcr_color_manager = GetUserDataAs<MockZcrColorManagerV1>(resource);
  zcr_color_manager->CreateColorSpaceFromIcc(client, resource, id, fd);
}

void CreateColorSpaceFromNames(wl_client* client,
                               wl_resource* resource,
                               uint32_t id,
                               uint32_t eotf,
                               uint32_t chromaticity,
                               uint32_t whitepoint) {
  wl_resource* color_space_resource =
      CreateResourceWithImpl<TestZcrColorSpaceV1>(
          client, &zcr_color_space_v1_interface, 1, &kTestZcrColorSpaceV1Impl,
          0);
  DCHECK(color_space_resource);
  auto* zcr_color_manager = GetUserDataAs<MockZcrColorManagerV1>(resource);
  auto* zcr_color_space =
      GetUserDataAs<TestZcrColorSpaceV1>(color_space_resource);

  auto chromaticity_id = gfx::ColorSpace::PrimaryID::INVALID;
  const auto maybe_primary = ui::wayland::kChromaticityMap.find(chromaticity);
  if (maybe_primary != ui::wayland::kChromaticityMap.end()) {
    chromaticity_id = maybe_primary->second.primary;
  }
  auto eotf_id = gfx::ColorSpace::TransferID::INVALID;
  const auto maybe_eotf = ui::wayland::kEotfMap.find(eotf);
  if (maybe_eotf != ui::wayland::kEotfMap.end()) {
    eotf_id = maybe_eotf->second.transfer;
  }

  zcr_color_space->SetGfxColorSpace(gfx::ColorSpace(chromaticity_id, eotf_id));
  zcr_color_space->SetZcrColorManager(zcr_color_manager);
  wl_resource* creator_resource =
      CreateResourceWithImpl<TestZcrColorSpaceCreatorV1>(
          client, &zcr_color_space_creator_v1_interface, 1, nullptr, id);
  DCHECK(creator_resource);
  zcr_color_space_creator_v1_send_created(creator_resource,
                                          color_space_resource);

  zcr_color_manager->StoreZcrColorSpace(zcr_color_space);
  zcr_color_manager->CreateColorSpaceFromNames(client, resource, id, eotf,
                                               chromaticity, whitepoint);
}

void CreateColorSpaceFromParams(wl_client* client,
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
                                uint32_t whitepoint_y) {
  wl_resource* color_space_resource =
      CreateResourceWithImpl<TestZcrColorSpaceV1>(
          client, &zcr_color_space_v1_interface, 1, &kTestZcrColorSpaceV1Impl,
          0);
  DCHECK(color_space_resource);
  auto* zcr_color_manager = GetUserDataAs<MockZcrColorManagerV1>(resource);
  auto* zcr_color_space =
      GetUserDataAs<TestZcrColorSpaceV1>(color_space_resource);

  zcr_color_space->SetGfxColorSpace(gfx::ColorSpace::CreateSRGB());
  zcr_color_space->SetZcrColorManager(zcr_color_manager);
  wl_resource* creator_resource =
      CreateResourceWithImpl<TestZcrColorSpaceCreatorV1>(
          client, &zcr_color_space_creator_v1_interface, 1, nullptr, id);
  DCHECK(creator_resource);
  zcr_color_space_creator_v1_send_created(creator_resource,
                                          color_space_resource);

  zcr_color_manager->StoreZcrColorSpace(zcr_color_space);
  zcr_color_manager->CreateColorSpaceFromParams(
      client, resource, id, eotf, primary_r_x, primary_r_y, primary_g_x,
      primary_g_y, primary_b_x, primary_b_y, whitepoint_x, whitepoint_y);
}

void GetColorManagementOutput(wl_client* client,
                              wl_resource* resource,
                              uint32_t id,
                              wl_resource* output) {
  wl_resource* output_resource =
      CreateResourceWithImpl<TestZcrColorManagementOutputV1>(
          client, &zcr_color_management_output_v1_interface, 1,
          &kTestZcrColorManagementOutputV1Impl, id);
  DCHECK(output_resource);
  auto* zcr_color_manager = GetUserDataAs<MockZcrColorManagerV1>(resource);
  auto* color_manager_output =
      GetUserDataAs<TestZcrColorManagementOutputV1>(output_resource);

  gfx::ColorSpace color_space;
  color_manager_output->SetGfxColorSpace(color_space);
  color_manager_output->StoreZcrColorManager(zcr_color_manager);
  zcr_color_manager->StoreZcrColorManagementOutput(color_manager_output);
  zcr_color_manager->GetColorManagementOutput(client, resource, id, output);
}

void GetColorManagementSurface(wl_client* client,
                               wl_resource* resource,
                               uint32_t id,
                               wl_resource* output) {
  wl_resource* surface_resource =
      CreateResourceWithImpl<TestZcrColorManagementSurfaceV1>(
          client, &zcr_color_management_surface_v1_interface, 1,
          &kTestZcrColorManagementSurfaceV1Impl, id);
  DCHECK(surface_resource);
  auto* zcr_color_manager = GetUserDataAs<MockZcrColorManagerV1>(resource);
  auto* color_manager_surface =
      GetUserDataAs<TestZcrColorManagementSurfaceV1>(surface_resource);

  gfx::ColorSpace color_space;
  color_manager_surface->SetGfxColorSpace(color_space);
  color_manager_surface->StoreZcrColorManager(zcr_color_manager);
  zcr_color_manager->StoreZcrColorManagementSurface(color_manager_surface);
  zcr_color_manager->GetColorManagementSurface(client, resource, id, output);
}

}  // namespace

const struct zcr_color_manager_v1_interface kMockZcrColorManagerV1Impl = {
    &CreateColorSpaceFromIcc,    &CreateColorSpaceFromNames,
    &CreateColorSpaceFromParams, &GetColorManagementOutput,
    &GetColorManagementSurface,  &DestroyResource};

MockZcrColorManagerV1::MockZcrColorManagerV1()
    : GlobalObject(&zcr_color_manager_v1_interface,
                   &kMockZcrColorManagerV1Impl,
                   kZcrColorManagerVersion) {}

MockZcrColorManagerV1::~MockZcrColorManagerV1() {
  DCHECK(color_manager_outputs_.empty());
  DCHECK(color_manager_surfaces_.empty());
}

void MockZcrColorManagerV1::StoreZcrColorManagementOutput(
    TestZcrColorManagementOutputV1* params) {
  color_manager_outputs_.push_back(params);
}

void MockZcrColorManagerV1::StoreZcrColorManagementSurface(
    TestZcrColorManagementSurfaceV1* params) {
  color_manager_surfaces_.push_back(params);
}

void MockZcrColorManagerV1::StoreZcrColorSpace(TestZcrColorSpaceV1* params) {
  color_manager_color_spaces_.push_back(params);
}

void MockZcrColorManagerV1::OnZcrColorManagementOutputDestroyed(
    TestZcrColorManagementOutputV1* params) {
  auto it = base::ranges::find(color_manager_outputs_, params);
  CHECK(it != color_manager_outputs_.end(), base::NotFatalUntil::M130);
  color_manager_outputs_.erase(it);
}

void MockZcrColorManagerV1::OnZcrColorManagementSurfaceDestroyed(
    TestZcrColorManagementSurfaceV1* params) {
  auto it = base::ranges::find(color_manager_surfaces_, params);
  CHECK(it != color_manager_surfaces_.end(), base::NotFatalUntil::M130);
  color_manager_surfaces_.erase(it);
}

void MockZcrColorManagerV1::OnZcrColorSpaceDestroyed(
    TestZcrColorSpaceV1* params) {
  auto it = base::ranges::find(color_manager_color_spaces_, params);
  CHECK(it != color_manager_color_spaces_.end(), base::NotFatalUntil::M130);
  color_manager_color_spaces_.erase(it);
}

}  // namespace wl
